/// @file TypeChecker.cpp
/// Semantic analysis for the typed Sere frontend.

#include "sere/sema/TypeChecker.h"

#include "sere/Version.h"
#include "sere/ast/Query.h"
#include "sere/diag/DiagnosticEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sere {
namespace {

/// Expands the space-separated parameter-name string stored on an intrinsic
/// registry row (IntrinsicInfo::params) into the vector used by symbols and
/// hover text. Keeping the tokens inline on the row means there is a single
/// source of truth for an intrinsic's parameters.
[[nodiscard]] std::vector<std::string> splitIntrinsicParams(std::string_view params) {
  std::vector<std::string> names;
  std::size_t tokenStart = 0;
  while (tokenStart <= params.size()) {
    const std::size_t space = params.find(' ', tokenStart);
    const std::size_t tokenEnd = space == std::string_view::npos ? params.size() : space;
    if (tokenEnd > tokenStart) {
      names.emplace_back(params.substr(tokenStart, tokenEnd - tokenStart));
    }
    tokenStart = tokenEnd + 1;
  }
  return names;
}

// Generic parameter names are scoped to their declaration, including unconstrained
// parameters that shadow a constrained parameter with the same name.
class TypeConstraintScope {
public:
  TypeConstraintScope(std::unordered_map<std::string, const Type*>& active,
                      const std::vector<std::string>& names,
                      const std::vector<const Type*>& constraints)
      : active_(active), saved_(active) {
    for (std::size_t i = 0; i < names.size(); ++i) {
      active_[names[i]] = i < constraints.size() ? constraints[i] : nullptr;
    }
  }
  ~TypeConstraintScope() { active_ = std::move(saved_); }

private:
  std::unordered_map<std::string, const Type*>& active_;
  std::unordered_map<std::string, const Type*> saved_;
};

[[nodiscard]] std::vector<const Type*>
resolvedConstraints(const std::vector<std::unique_ptr<TypeExpr>>& expressions) {
  std::vector<const Type*> result;
  for (const auto& expression : expressions) {
    result.push_back(expression == nullptr ? nullptr : expression->resolvedType());
  }
  return result;
}

[[nodiscard]] bool isConcreteConstraint(const Type* type) {
  if (type == nullptr)
    return false;
  type = type->canonical();
  if (type->isTypeParam() || type->isAny() || type->isNamed("void") || type->isSizeLiteral() ||
      !type->typeParams().empty())
    return false;
  for (const Type* arg : type->args()) {
    if (!isConcreteConstraint(arg))
      return false;
  }
  for (const Type* param : type->paramTypes()) {
    if (!isConcreteConstraint(param))
      return false;
  }
  return true;
}

[[nodiscard]] const NameExpr* asName(const Expr& expr) {
  if (expr.kind() != NodeKind::NameExpr) {
    return nullptr;
  }
  return static_cast<const NameExpr*>(&expr);
}

[[nodiscard]] NameExpr* asName(Expr& expr) {
  return const_cast<NameExpr*>(asName(static_cast<const Expr&>(expr)));
}

[[nodiscard]] const Type* unwrapRecordType(const Type* type) {
  if (type == nullptr) {
    return nullptr;
  }
  if (type->isTypeObject() && type->typeObjectInstance() != nullptr) {
    type = type->typeObjectInstance();
  }
  type = type->canonical();
  return type != nullptr && type->isRecord() ? type : nullptr;
}

/// Folds `1`, `+1`, and `-1` (including nested unary signs) into an integer.
[[nodiscard]] std::optional<std::int64_t> evalEnumInt(const Expr& expr) {
  if (expr.kind() == NodeKind::IntegerLiteral) {
    return static_cast<const IntegerLiteral&>(expr).value();
  }
  if (expr.kind() != NodeKind::UnaryExpr) {
    return std::nullopt;
  }
  const auto& unary = static_cast<const UnaryExpr&>(expr);
  const std::optional<std::int64_t> inner = evalEnumInt(unary.operand());
  if (!inner.has_value()) {
    return std::nullopt;
  }
  if (unary.op() == UnaryOp::Pos) {
    return inner;
  }
  if (unary.op() != UnaryOp::Neg) {
    return std::nullopt;
  }
  if (*inner == std::numeric_limits<std::int64_t>::min()) {
    return std::nullopt;
  }
  return -*inner;
}

[[nodiscard]] std::optional<double> foldFloat(const Expr& expr) {
  if (expr.kind() == NodeKind::FloatLiteral) {
    return static_cast<const FloatLiteral&>(expr).value();
  }
  if (expr.kind() == NodeKind::IntegerLiteral) {
    return static_cast<double>(static_cast<const IntegerLiteral&>(expr).value());
  }
  if (expr.kind() != NodeKind::UnaryExpr) {
    return std::nullopt;
  }
  const auto& unary = static_cast<const UnaryExpr&>(expr);
  const std::optional<double> inner = foldFloat(unary.operand());
  if (!inner.has_value()) {
    return std::nullopt;
  }
  if (unary.op() == UnaryOp::Pos) {
    return inner;
  }
  if (unary.op() != UnaryOp::Neg) {
    return std::nullopt;
  }
  return -*inner;
}

[[nodiscard]] bool integerLiteralFits(std::int64_t value, const Type* dest) {
  if (dest == nullptr || !dest->isScalarInteger()) {
    return true;
  }
  const int bits = dest->integerBitWidth();
  if (dest->isUnsignedInteger()) {
    if (value < 0) {
      return false;
    }
    if (bits >= 64) {
      return true;
    }
    const std::uint64_t max = (1ull << bits) - 1ull;
    return static_cast<std::uint64_t>(value) <= max;
  }
  if (bits >= 64) {
    return true;
  }
  const std::int64_t min = -(static_cast<std::int64_t>(1) << (bits - 1));
  const std::int64_t max = (static_cast<std::int64_t>(1) << (bits - 1)) - 1;
  return value >= min && value <= max;
}

[[nodiscard]] bool floatLiteralFits(double value, const Type* dest) {
  if (dest == nullptr || !dest->isNamed("f32")) {
    return true;
  }
  if (!std::isfinite(value)) {
    return true;
  }
  return std::fabs(value) <= static_cast<double>(std::numeric_limits<float>::max());
}

[[nodiscard]] bool inferTypeBindings(const Type* pattern,
                                     const Type* actual,
                                     std::unordered_map<std::string, const Type*>& bindings) {
  if (pattern == nullptr || actual == nullptr) {
    return false;
  }
  pattern = pattern->canonical();
  actual = actual->canonical();
  if (pattern->isTypeParam()) {
    const auto found = bindings.find(pattern->name());
    if (found == bindings.end()) {
      bindings[pattern->name()] = actual;
      return true;
    }
    return found->second->canonical() == actual;
  }
  if (pattern->kind() == TypeKind::Generic && actual->kind() == TypeKind::Generic &&
      pattern->name() == actual->name() && pattern->args().size() == actual->args().size()) {
    for (std::size_t index = 0; index < pattern->args().size(); ++index) {
      if (!inferTypeBindings(pattern->args()[index], actual->args()[index], bindings)) {
        return false;
      }
    }
    return true;
  }
  return true;
}

[[nodiscard]] bool abstractMethodNeedsOverride(const FunctionDef& method) {
  if (!method.isAbstract()) {
    return false;
  }
  bool sawCode = false;
  for (const std::unique_ptr<Stmt>& statement : method.body()) {
    if (statement == nullptr || statement->kind() == NodeKind::PassStmt) {
      continue;
    }
    sawCode = true;
    break;
  }
  return !sawCode;
}

[[nodiscard]] bool isAssignableTarget(const Expr& expr) {
  switch (expr.kind()) {
  case NodeKind::NameExpr:
  case NodeKind::MemberExpr:
    return true;
  case NodeKind::IndexExpr:
    return !static_cast<const IndexExpr&>(expr).isSlice();
  case NodeKind::UnaryExpr:
    return static_cast<const UnaryExpr&>(expr).op() == UnaryOp::Deref;
  default:
    return false;
  }
}

[[nodiscard]] bool isAddressable(const Expr& expr) {
  switch (expr.kind()) {
  case NodeKind::NameExpr:
    return true;
  case NodeKind::MemberExpr:
    return isAddressable(static_cast<const MemberExpr&>(expr).object());
  case NodeKind::IndexExpr: {
    const auto& index = static_cast<const IndexExpr&>(expr);
    return !index.isSlice();
  }
  case NodeKind::UnaryExpr:
    return static_cast<const UnaryExpr&>(expr).op() == UnaryOp::Deref;
  default:
    return false;
  }
}

[[nodiscard]] bool isCallableSymbol(const Symbol& symbol) {
  return symbol.kind == SymbolKind::Function || symbol.kind == SymbolKind::Intrinsic;
}

struct CallableShape {
  bool functionsOnly = false;
  bool hasParams = false;
  bool variadic = false;
  std::vector<const Type*> params{};
  bool hasReturn = false;
  const Type* returnType = nullptr;
};

[[nodiscard]] bool fillCallableShape(const Type* type, CallableShape& out) {
  if (type == nullptr || !type->isCallableConstraint()) {
    return false;
  }
  type = type->canonical();
  out = CallableShape{};
  out.functionsOnly = type->isGenericCtor("Function");
  const std::vector<const Type*>& args = type->args();
  if (args.empty()) {
    return true;
  }
  if (args[0] != nullptr && args[0]->isParamList()) {
    out.hasParams = true;
    for (const Type* param : args[0]->args()) {
      if (param != nullptr && param->isEllipsis()) {
        out.variadic = true;
        continue;
      }
      out.params.push_back(param);
    }
    if (args.size() >= 2) {
      out.hasReturn = true;
      out.returnType = args[1];
    }
    return true;
  }
  out.hasReturn = true;
  out.returnType = args[0];
  return true;
}

[[nodiscard]] bool constructorParams(const Type* record, std::vector<const Type*>& params) {
  if (record == nullptr) {
    return false;
  }
  record = record->canonical();
  const int initIndex = record->methodIndex("__init__");
  if (initIndex >= 0) {
    const Type* functionType = record->methods()[static_cast<std::size_t>(initIndex)].type;
    if (functionType == nullptr || functionType->paramTypes().empty()) {
      return false;
    }
    for (std::size_t index = 1; index < functionType->paramTypes().size(); ++index) {
      params.push_back(functionType->paramTypes()[index]);
    }
    return true;
  }
  for (const RecordField& field : record->fields()) {
    if (!field.isStatic && field.stored) {
      params.push_back(field.type);
    }
  }
  return true;
}

[[nodiscard]] std::string loweredCallName(const NameExpr& callee, const Symbol& symbol) {
  if (symbol.function == nullptr) {
    return callee.name();
  }
  if (symbol.function->isExtern()) {
    return symbol.function->externName();
  }
  if (!symbol.function->modulePrefix().empty()) {
    return symbol.function->modulePrefix() + "_" + symbol.function->name();
  }
  return symbol.function->name();
}

[[nodiscard]] std::string quoteType(const Type* type) {
  return "'" + (type == nullptr ? std::string("?") : type->display()) + "'";
}

[[nodiscard]] std::vector<std::string> splitQualifiedName(const std::string& name) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  for (std::size_t index = 0; index <= name.size(); ++index) {
    if (index == name.size() || name[index] == '.') {
      parts.push_back(name.substr(start, index - start));
      start = index + 1;
    }
  }
  return parts;
}

[[nodiscard]] bool isParseableType(const Type* type) {
  if (type == nullptr) {
    return false;
  }
  type = type->canonical();
  if (type->isNamed("str") || type->isNamed("bool") || type->isVoidLike() ||
      type->isScalarInteger() || type->isFloat()) {
    return true;
  }
  if (!type->isUnion() || type->args().empty()) {
    return false;
  }
  bool allInt = true;
  bool allFloat = true;
  for (const Type* member : type->args()) {
    if (member == nullptr || !member->isInteger()) {
      allInt = false;
    }
    if (member == nullptr || !member->isFloat()) {
      allFloat = false;
    }
  }
  return allInt || allFloat;
}

[[nodiscard]] const Type* joinNumeric(TypeContext& types, const Type* left, const Type* right) {
  if (left == nullptr || right == nullptr) {
    return nullptr;
  }
  if (left->canonical() == right->canonical()) {
    return left;
  }
  const bool leftNum = left->isInteger() || left->isFloat();
  const bool rightNum = right->isInteger() || right->isFloat();
  if (!leftNum || !rightNum) {
    return nullptr;
  }
  if (left->isFloat() || right->isFloat()) {
    if (left->isNamed("f64") || right->isNamed("f64")) {
      return types.f64Type();
    }
    if (left->isFloat()) {
      return left;
    }
    return right;
  }
  if (left->isUnsignedInteger() != right->isUnsignedInteger()) {
    const Type* signedType = left->isUnsignedInteger() ? right : left;
    const Type* unsignedType = left->isUnsignedInteger() ? left : right;
    const unsigned needed =
        std::max(signedType->integerBitWidth(), unsignedType->integerBitWidth() + 1);
    if (needed <= 8)
      return types.primitive("i8");
    if (needed <= 16)
      return types.primitive("i16");
    if (needed <= 32)
      return types.i32Type();
    if (needed <= 64)
      return types.primitive("i64");
    return types.anyType();
  }
  return left->integerBitWidth() >= right->integerBitWidth() ? left : right;
}

[[nodiscard]] std::string countLabel(std::size_t count, const char* singular, const char* plural) {
  return std::to_string(count) + " " + (count == 1 ? singular : plural);
}

[[nodiscard]] int editDistance(const std::string& left, const std::string& right) {
  if (left.size() > 24 || right.size() > 24) {
    return 99;
  }
  const std::size_t height = left.size();
  const std::size_t width = right.size();
  if (height > width && height - width > 2) {
    return 99;
  }
  if (width > height && width - height > 2) {
    return 99;
  }
  std::vector<int> prev(width + 1);
  std::vector<int> next(width + 1);
  for (std::size_t index = 0; index <= width; ++index) {
    prev[index] = static_cast<int>(index);
  }
  for (std::size_t row = 0; row < height; ++row) {
    next[0] = static_cast<int>(row + 1);
    for (std::size_t col = 0; col < width; ++col) {
      const int cost = left[row] == right[col] ? 0 : 1;
      next[col + 1] = std::min({prev[col + 1] + 1, next[col] + 1, prev[col] + cost});
    }
    prev.swap(next);
  }
  return prev[width];
}

[[nodiscard]] std::string bestSuggestion(const std::vector<std::string>& candidates,
                                         const std::string& name) {
  const int maxDistance = name.size() <= 3 ? 1 : 2;
  std::string best;
  int bestDistance = maxDistance + 1;
  for (const std::string& candidate : candidates) {
    if (candidate == name) {
      continue;
    }
    const int distance = editDistance(name, candidate);
    if (distance > 0 && distance < bestDistance) {
      bestDistance = distance;
      best = candidate;
    }
  }
  return best;
}

void reportUnknownMember(DiagnosticEngine& diagnostics,
                         SourceRange range,
                         const Type* type,
                         const std::string& name,
                         bool isMethod) {
  const char* kind = isMethod ? "method" : "field";
  diagnostics.error(range, quoteType(type) + " has no " + kind + " '" + name + "'");
  std::vector<std::string> names;
  if (type != nullptr && isMethod) {
    for (const RecordMethod& method : type->methods()) {
      names.push_back(method.name);
    }
  } else if (type != nullptr) {
    for (const RecordField& field : type->fields()) {
      names.push_back(field.name);
    }
  }
  const std::string suggestion = bestSuggestion(names, name);
  if (!suggestion.empty()) {
    diagnostics.help("did you mean '" + suggestion + "'?");
  }
}

} // namespace

TypeChecker::TypeChecker(TypeContext& types, DiagnosticEngine& diagnostics)
    : types_(&types), diagnostics_(&diagnostics) {
  pushScope();
  registerBuiltins();
}

void TypeChecker::pushScope(SourceRange range) {
  scopes_.emplace_back();
  scopeRanges_.push_back(range);
}

void TypeChecker::popScope() {
  scopes_.pop_back();
  scopeRanges_.pop_back();
}

bool TypeChecker::declare(const std::string& name,
                          Symbol symbol,
                          SourceLocation location,
                          bool navigable) {
  auto& scope = scopes_.back();
  if (scope.contains(name)) {
    diagnostics_->error(SourceRange{location, location}, "redeclaration of '" + name + "'");
    diagnostics_->note(scope.at(name).declarationLocation,
                       "'" + name + "' previously declared here");
    return false;
  }
  SemanticSymbol collected;
  collected.name = name;
  collected.location = location;
  collected.range = SourceRange{location, location};
  collected.scopeRange = scopeRanges_.empty() ? SourceRange{} : scopeRanges_.back();
  collected.scopeDepth = scopes_.size();
  collected.snippet = symbol.snippet;
  collected.type = symbol.type;
  collected.typeDisplay = !symbol.typeDisplay.empty()
                              ? symbol.typeDisplay
                              : (symbol.type == nullptr ? "" : symbol.type->display());
  switch (symbol.kind) {
  case SymbolKind::Variable:
    collected.kind = "variable";
    break;
  case SymbolKind::Function:
    collected.kind = "function";
    break;
  case SymbolKind::Class:
    if (symbol.type != nullptr && symbol.type->isEnum()) {
      collected.kind = "enum";
    } else if (symbol.type != nullptr && symbol.type->isStruct()) {
      collected.kind = "struct";
    } else {
      collected.kind = "class";
    }
    break;
  case SymbolKind::Type:
    collected.kind = "type";
    break;
  case SymbolKind::Module:
    collected.kind = "module";
    break;
  case SymbolKind::Intrinsic:
    collected.kind = "function";
    break;
  case SymbolKind::Macro:
    collected.kind = "macro";
    break;
  }
  collected.navigable = navigable;
  if (symbol.type != nullptr && symbol.type->kind() == TypeKind::Function) {
    for (const Type* param : symbol.type->paramTypes()) {
      collected.paramTypes.push_back(param == nullptr ? "?" : param->display());
    }
    collected.returnType =
        symbol.type->returnType() == nullptr ? "void" : symbol.type->returnType()->display();
  }
  if (!symbol.paramNames.empty()) {
    collected.paramNames = symbol.paramNames;
  } else if (symbol.function != nullptr) {
    for (const ParamDecl& param : symbol.function->params()) {
      collected.paramNames.push_back(param.name);
    }
  }
  symbols_.push_back(std::move(collected));
  symbol.declarationLocation = location;
  scope.emplace(name, symbol);
  return true;
}

const std::vector<SemanticSymbol>& TypeChecker::symbols() const {
  return symbols_;
}

std::vector<const SemanticSymbol*> TypeChecker::visibleSymbolsAt(std::uint32_t offset) const {
  std::unordered_map<std::string, const SemanticSymbol*> visible;
  for (const SemanticSymbol& symbol : symbols_) {
    const bool global = symbol.scopeDepth <= 1 || symbol.scopeRange.end.offset == 0;
    if (!global &&
        (offset < symbol.scopeRange.start.offset || offset > symbol.scopeRange.end.offset)) {
      continue;
    }
    if (!global && symbol.location.offset > offset) {
      continue;
    }
    const auto found = visible.find(symbol.name);
    if (found == visible.end() || found->second->scopeDepth < symbol.scopeDepth ||
        (found->second->scopeDepth == symbol.scopeDepth &&
         found->second->location.offset <= symbol.location.offset)) {
      visible[symbol.name] = &symbol;
    }
  }
  std::vector<const SemanticSymbol*> result;
  result.reserve(visible.size());
  for (const auto& entry : visible) {
    result.push_back(entry.second);
  }
  return result;
}

const Type* TypeChecker::typeOfName(std::string_view name) const {
  const std::string key(name);
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    const auto found = scope->find(key);
    if (found != scope->end() && found->second.type != nullptr) {
      return found->second.type->canonical();
    }
  }
  const Type* fromSymbols = nullptr;
  for (const SemanticSymbol& symbol : symbols_) {
    if (symbol.name != key) {
      continue;
    }
    if (symbol.type != nullptr) {
      fromSymbols = symbol.type;
      continue;
    }
    if (symbol.typeDisplay.empty()) {
      continue;
    }
    if (const Type* named = types_->lookupNamed(symbol.typeDisplay)) {
      fromSymbols = named;
    }
  }
  return fromSymbols == nullptr ? nullptr : fromSymbols->canonical();
}

const Type* TypeChecker::typeOfPath(const std::vector<std::string>& parts) const {
  if (parts.empty()) {
    return nullptr;
  }
  const Type* current = typeOfName(parts[0]);
  for (std::size_t index = 1; current != nullptr && index < parts.size(); ++index) {
    current = current->canonical();
    if (current->isTypeObject() && current->typeObjectInstance() != nullptr) {
      current = current->typeObjectInstance()->canonical();
    }
    const RecordField* field = current->findField(parts[index]);
    if (field == nullptr || field->type == nullptr) {
      return nullptr;
    }
    current = field->type;
  }
  return current == nullptr ? nullptr : current->canonical();
}

bool TypeChecker::importSymbol(const std::string& name, Symbol symbol, SourceLocation location) {
  auto& scope = scopes_.back();
  const auto existing = scope.find(name);
  // Imported names must not replace compiler intrinsics such as alloc.
  if (existing != scope.end() && existing->second.kind == SymbolKind::Intrinsic) {
    return true;
  }
  return declare(name, std::move(symbol), location);
}

void TypeChecker::recordSymbol(const std::string& name,
                               const std::string& kind,
                               const Type* type,
                               SourceLocation location,
                               std::string container,
                               std::vector<std::string> paramNames) {
  SemanticSymbol collected;
  collected.name = name;
  collected.kind = kind;
  collected.type = type;
  collected.typeDisplay = type == nullptr ? "" : type->display();
  collected.container = std::move(container);
  collected.paramNames = std::move(paramNames);
  if (type != nullptr && type->kind() == TypeKind::Function) {
    for (const Type* param : type->paramTypes()) {
      collected.paramTypes.push_back(param == nullptr ? "?" : param->display());
    }
    collected.returnType = type->returnType() == nullptr ? "void" : type->returnType()->display();
  }
  collected.location = location;
  collected.range = SourceRange{location, location};
  collected.scopeRange = scopeRanges_.empty() ? SourceRange{} : scopeRanges_.back();
  collected.scopeDepth = scopes_.size();
  symbols_.push_back(std::move(collected));
}

Symbol* TypeChecker::lookup(const std::string& name) {
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    const auto found = scope->find(name);
    if (found != scope->end()) {
      return &found->second;
    }
  }
  return nullptr;
}

std::string TypeChecker::suggestName(const std::string& name) const {
  std::vector<std::string> candidates;
  for (const auto& scope : scopes_) {
    for (const auto& entry : scope) {
      candidates.push_back(entry.first);
    }
  }
  return bestSuggestion(candidates, name);
}

void TypeChecker::reportUnknown(SourceRange range,
                                const std::string& kind,
                                const std::string& name) {
  diagnostics_->error(range, "unknown " + kind + " '" + name + "'");
  std::string suggestion = suggestName(name);
  if (suggestion.empty() && kind == "type") {
    const std::vector<std::string> primitives = {
        "void", "None",   "Any",    "bool", "i8",   "i16",   "i32", "i64",
        "u8",   "u16",    "u32",    "u64",  "f32",  "f64",   "str", "regex",
        "byte", "Unique", "Shared", "Ptr",  "list", "array", "dict"};
    suggestion = bestSuggestion(primitives, name);
  }
  if (!suggestion.empty()) {
    diagnostics_->help("did you mean '" + suggestion + "'?");
  }
}

void TypeChecker::setModuleInfo(
    std::string file, std::string name, std::string package, std::string doc, bool debug) {
  moduleFile_ = std::move(file);
  moduleName_ = std::move(name);
  modulePackage_ = std::move(package);
  moduleDoc_ = std::move(doc);
  moduleDebug_ = debug;
}

void TypeChecker::injectModuleGlobals() {
  const auto addConst = [&](const char* name, const Type* type, std::string text) {
    Symbol symbol;
    symbol.kind = SymbolKind::Variable;
    symbol.type = type;
    symbol.readonly = true;
    symbol.compileTimeText = std::move(text);
    (void)declare(name, symbol, {}, false);
  };
  addConst("__name__", types_->strType(), moduleName_);
  addConst("__file__", types_->strType(), moduleFile_);
  addConst("__package__", types_->strType(), modulePackage_);
  addConst("__doc__", types_->strType(), moduleDoc_);
  addConst("__sere_version__", types_->strType(), SERE_VERSION_STRING);
#if defined(_WIN32)
  addConst("__platform__", types_->strType(), "windows");
#elif defined(__APPLE__)
  addConst("__platform__", types_->strType(), "macos");
#else
  addConst("__platform__", types_->strType(), "linux");
#endif
#if defined(_M_X64) || defined(__x86_64__)
  addConst("__arch__", types_->strType(), "x86_64");
#elif defined(_M_ARM64) || defined(__aarch64__)
  addConst("__arch__", types_->strType(), "arm64");
#else
  addConst("__arch__", types_->strType(), "unknown");
#endif
  const auto addFlag = [&](const char* name, bool value) {
    Symbol symbol;
    symbol.kind = SymbolKind::Variable;
    symbol.type = types_->boolType();
    symbol.readonly = true;
    symbol.hasCompileTimeBool = true;
    symbol.compileTimeBool = value;
    (void)declare(name, symbol, {}, false);
  };
#if defined(_WIN32)
  addFlag("__windows__", true);
  addFlag("__linux__", false);
  addFlag("__macos__", false);
  addFlag("__unix__", false);
#elif defined(__APPLE__)
  addFlag("__windows__", false);
  addFlag("__linux__", false);
  addFlag("__macos__", true);
  addFlag("__unix__", true);
#else
  addFlag("__windows__", false);
  addFlag("__linux__", true);
  addFlag("__macos__", false);
  addFlag("__unix__", true);
#endif
#if defined(_M_X64) || defined(__x86_64__)
  addFlag("__x86_64__", true);
  addFlag("__arm64__", false);
#elif defined(_M_ARM64) || defined(__aarch64__)
  addFlag("__x86_64__", false);
  addFlag("__arm64__", true);
#else
  addFlag("__x86_64__", false);
  addFlag("__arm64__", false);
#endif
  Symbol debug;
  debug.kind = SymbolKind::Variable;
  debug.type = types_->boolType();
  debug.readonly = true;
  debug.hasCompileTimeBool = true;
  debug.compileTimeBool = moduleDebug_;
  (void)declare("__debug__", debug, {}, false);
}

void TypeChecker::registerBuiltins() {
  for (const IntrinsicInfo& info : allIntrinsics()) {
    if (!info.declared) {
      continue;
    }
    Symbol symbol;
    symbol.kind = SymbolKind::Intrinsic;
    symbol.intrinsic = info.kind;
    symbol.paramNames = splitIntrinsicParams(info.params);
    if (!declare(std::string(info.name), symbol, {}, false)) {
      return;
    }
  }
}

bool TypeChecker::isAssignable(const Type* from, const Type* to) const {
  if (from == nullptr || to == nullptr) {
    return false;
  }
  if (from == to || from->canonical() == to->canonical()) {
    return true;
  }
  if (to->isAny() || from->isAny()) {
    return true;
  }
  if (from->isVoidLike() && to->isVoidLike()) {
    return true;
  }
  if (from->isNamed("null") && to->isPointerLike()) {
    return true;
  }
  if (from->isSubtypeOf(to)) {
    return true;
  }
  if (from->valueType() != from->canonical() && !to->isRecord()) {
    return isAssignable(from->valueType(), to);
  }
  if (from->isTypeObject() && to->isTypeObject()) {
    const Type* fromInst = from->typeObjectInstance();
    const Type* toInst = to->typeObjectInstance();
    if (fromInst != nullptr && toInst != nullptr &&
        (fromInst->canonical() == toInst->canonical() || fromInst->isSubtypeOf(toInst))) {
      return true;
    }
  }
  if (from->isList() && to->isList()) {
    const Type* fromElem = from->elementType();
    const Type* toElem = to->elementType();
    if (fromElem != nullptr && toElem != nullptr &&
        (fromElem->canonical() == toElem->canonical() || toElem->isAny() ||
         isAssignable(fromElem, toElem))) {
      const std::int64_t wanted = to->listSize();
      return wanted < 0 || from->listSize() == wanted;
    }
  }
  if (from->isPointerLike() && to->isPointerLike()) {
    const Type* fromPointee = from->pointeeType();
    const Type* toPointee = to->pointeeType();
    // Writable pointers are invariant: numeric widening and class slicing
    // would let a callee overwrite storage with an incompatible layout.
    return fromPointee != nullptr && toPointee != nullptr &&
           fromPointee->canonical() == toPointee->canonical() &&
           (to->isGenericCtor("Ptr") ||
            (from->isGenericCtor("Unique") && to->isGenericCtor("Unique")) ||
            (from->isGenericCtor("Shared") && to->isGenericCtor("Shared")));
  }
  if (from->isRecord() && to->isRecord()) {
    std::string_view fromName = from->name();
    const auto fromBracket = fromName.find('[');
    if (fromBracket != std::string_view::npos) {
      fromName = fromName.substr(0, fromBracket);
    }
    std::string_view toName = to->name();
    const auto toBracket = toName.find('[');
    if (toBracket != std::string_view::npos) {
      toName = toName.substr(0, toBracket);
    }
    if (fromName == toName && !from->args().empty() && from->args().size() == to->args().size()) {
      bool allCompatible = true;
      for (std::size_t i = 0; i < from->args().size(); ++i) {
        if (!to->args()[i]->isAny() && !isAssignable(from->args()[i], to->args()[i])) {
          allCompatible = false;
          break;
        }
      }
      if (allCompatible) {
        return true;
      }
    }
  }
  if (from->isDict() && to->isDict()) {
    const Type* fromKey = from->dictKeyType();
    const Type* toKey = to->dictKeyType();
    const Type* fromValue = from->dictValueType();
    const Type* toValue = to->dictValueType();
    return fromKey != nullptr && toKey != nullptr && fromValue != nullptr && toValue != nullptr &&
           fromKey->canonical() == toKey->canonical() &&
           fromValue->canonical() == toValue->canonical();
  }
  if ((from->isEnum() && to->isEnum() && from->canonical() == to->canonical())) {
    return true;
  }
  if (from->isNever()) {
    return true;
  }
  if (from->isInteger() && to->isInteger()) {
    return true;
  }
  if (from->isInteger() && to->isFloat()) {
    return true;
  }
  if (from->isFloat() && to->isFloat()) {
    return true;
  }
  if (from->isIntEnum() && to->isInteger()) {
    return true;
  }
  if (to->isUnion()) {
    if (from->isUnion()) {
      for (const Type* member : from->args()) {
        if (!isAssignable(member, to)) {
          return false;
        }
      }
      return !from->args().empty();
    }
    for (const Type* member : to->args()) {
      if (isAssignable(from, member)) {
        return true;
      }
    }
  }
  if (to->isCallableConstraint() && callableSatisfies(from, to)) {
    return true;
  }
  if (to->isClassConstraint() && to->args().empty() && from->isTypeObject()) {
    return true;
  }
  return false;
}

bool TypeChecker::ensureLiteralFits(const Expr& expr, const Type* dest) {
  if (dest == nullptr) {
    return true;
  }
  dest = dest->canonical();
  if (dest->isNamed("f32")) {
    const std::optional<double> value = foldFloat(expr);
    if (value.has_value() && !floatLiteralFits(*value, dest)) {
      diagnostics_->error(expr.range(), "value does not fit in f32");
      return false;
    }
  }
  if (dest->isScalarInteger()) {
    const std::optional<std::int64_t> value = evalEnumInt(expr);
    if (value.has_value() && !integerLiteralFits(*value, dest)) {
      diagnostics_->error(expr.range(), "value does not fit in " + quoteType(dest));
      return false;
    }
  }
  if (expr.kind() == NodeKind::ListLiteral && dest->isSequence()) {
    const Type* element = dest->elementType();
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      if (item != nullptr && !ensureLiteralFits(*item, element)) {
        return false;
      }
    }
  }
  if (expr.kind() == NodeKind::DictLiteral && dest->isDict()) {
    const auto& literal = static_cast<const DictLiteral&>(expr);
    for (std::size_t index = 0; index < literal.keys().size(); ++index) {
      if (literal.keys()[index] != nullptr &&
          !ensureLiteralFits(*literal.keys()[index], dest->dictKeyType())) {
        return false;
      }
      if (index < literal.values().size() && literal.values()[index] != nullptr &&
          !ensureLiteralFits(*literal.values()[index], dest->dictValueType())) {
        return false;
      }
    }
  }
  return true;
}

bool TypeChecker::callableSatisfies(const Type* from, const Type* to) const {
  CallableShape dest;
  if (from == nullptr || to == nullptr || !fillCallableShape(to, dest)) {
    return false;
  }
  from = from->canonical();
  // Value conversions need boxing/unboxing instructions. A callable constraint
  // cannot change a function's ABI merely by changing its annotation.
  const auto signatureAssignable = [&](const Type* source, const Type* destType) {
    return source != nullptr && destType != nullptr && source->isAny() == destType->isAny() &&
           isAssignable(source, destType);
  };
  auto paramsMatch = [&](const std::vector<const Type*>& source) -> bool {
    if (!dest.hasParams) {
      return true;
    }
    if (dest.variadic) {
      if (source.size() < dest.params.size()) {
        return false;
      }
      for (std::size_t index = 0; index < dest.params.size(); ++index) {
        if (dest.params[index] == nullptr || source[index] == nullptr ||
            !signatureAssignable(dest.params[index], source[index])) {
          return false;
        }
      }
      return true;
    }
    if (source.size() != dest.params.size()) {
      return false;
    }
    for (std::size_t index = 0; index < dest.params.size(); ++index) {
      if (dest.params[index] == nullptr || source[index] == nullptr ||
          !signatureAssignable(dest.params[index], source[index])) {
        return false;
      }
    }
    return true;
  };
  if (from->kind() == TypeKind::Function) {
    if (dest.hasReturn) {
      const Type* ret = from->returnType();
      if (ret == nullptr || dest.returnType == nullptr ||
          !signatureAssignable(ret, dest.returnType)) {
        return false;
      }
    }
    return paramsMatch(from->paramTypes());
  }
  if (from->isTypeObject()) {
    if (dest.functionsOnly) {
      return false;
    }
    const Type* record = from->typeObjectInstance();
    if (record == nullptr || record->isEnum() || record->isAbstract()) {
      return false;
    }
    if (dest.hasReturn &&
        (dest.returnType == nullptr || !signatureAssignable(record, dest.returnType))) {
      return false;
    }
    std::vector<const Type*> params;
    if (!constructorParams(record, params)) {
      return false;
    }
    return paramsMatch(params);
  }
  if (from->isCallableConstraint()) {
    if (dest.functionsOnly && !from->isGenericCtor("Function")) {
      return false;
    }
    CallableShape src;
    if (!fillCallableShape(from, src)) {
      return false;
    }
    if (dest.hasReturn &&
        (!src.hasReturn || src.returnType == nullptr || dest.returnType == nullptr ||
         !signatureAssignable(src.returnType, dest.returnType))) {
      return false;
    }
    if (!dest.hasParams) {
      return true;
    }
    if (!src.hasParams || (src.variadic && !dest.variadic)) {
      return false;
    }
    return paramsMatch(src.params);
  }
  return false;
}

bool TypeChecker::canCast(const Type* from, const Type* to) const {
  if (from == nullptr || to == nullptr) {
    return false;
  }
  from = from->canonical();
  to = to->canonical();
  if (from == to || isAssignable(from, to) || from->isAny() || to->isAny()) {
    return true;
  }
  if (from->isUnion()) {
    for (const Type* member : from->args()) {
      if (member != nullptr && (isAssignable(member, to) || member->canonical() == to)) {
        return true;
      }
    }
  }
  if (to->isUnion()) {
    return true;
  }
  const bool fromNumber = from->isInteger() || from->isNamed("bool") || from->isFloat();
  const bool toNumber = to->isInteger() || to->isNamed("bool") || to->isFloat();
  if (fromNumber && toNumber) {
    return true;
  }
  if ((from->isEnum() && to->isInteger()) || (from->isInteger() && to->isEnum())) {
    return true;
  }
  if (from->isPointerLike() && to->isPointerLike()) {
    return true;
  }
  if (from->isPointerLike() && to->isInteger()) {
    return true;
  }
  if ((from->isNamed("str") && to->isNamed("regex")) ||
      (from->isNamed("regex") && to->isNamed("str"))) {
    return true;
  }
  if (from->isInteger() && to->isPointerLike()) {
    return true;
  }
  if ((from->isNamed("void") || from->isNamed("null")) && to->isPointerLike()) {
    return true;
  }
  if (from->isEnum() && to->isInteger()) {
    return true;
  }
  if (from->isInteger() && to->isEnum()) {
    return true;
  }
  return false;
}

bool TypeChecker::isPrintable(const Type* type) const {
  return type != nullptr;
}

bool TypeChecker::isVoidLike(const Type* type) const {
  return type != nullptr && (type->isVoidLike() || type->isNever());
}

bool TypeChecker::declareInferred(NameExpr& name, const Type* type, SourceLocation location) {
  if (type == nullptr || isVoidLike(type)) {
    diagnostics_->error(name.range(), "cannot infer a type for '" + name.name() + "'");
    return false;
  }
  Symbol symbol;
  symbol.kind = SymbolKind::Variable;
  symbol.type = type;
  if (!declare(name.name(), symbol, location)) {
    return false;
  }
  name.setResolvedType(type);
  return true;
}

bool TypeChecker::resolveTypeConstraints(
    const std::vector<std::unique_ptr<TypeExpr>>& constraints) {
  for (const auto& constraint : constraints) {
    if (constraint == nullptr || constraint->resolvedType() != nullptr)
      continue;
    const Type* resolved = resolveTypeExpr(*constraint);
    if (resolved == nullptr)
      return false;
    if (!isConcreteConstraint(resolved)) {
      diagnostics_->error(constraint->range(),
                          "generic constraint must name concrete types, not " +
                              quoteType(resolved));
      return false;
    }
    constraint->setResolvedType(resolved);
  }
  return true;
}

bool TypeChecker::ensureRecordConstraints(const Type* record) {
  record = record->canonical();
  const auto found = recordConstraintExprs_.find(record);
  if (found == recordConstraintExprs_.end() ||
      record->typeConstraints().size() == record->typeParams().size())
    return true;
  if (std::find(resolvingConstraints_.begin(), resolvingConstraints_.end(), record) !=
      resolvingConstraints_.end()) {
    diagnostics_->error("cyclic generic constraint for '" + record->name() + "'");
    return false;
  }
  resolvingConstraints_.push_back(record);
  const bool ok = resolveTypeConstraints(*found->second);
  resolvingConstraints_.pop_back();
  if (ok)
    types_->setRecordTypeConstraints(record, resolvedConstraints(*found->second));
  return ok;
}

bool TypeChecker::satisfiesTypeConstraint(const Type* argument, const Type* constraint) const {
  if (constraint == nullptr)
    return true;
  if (argument == nullptr)
    return false;
  argument = argument->canonical();
  constraint = constraint->canonical();
  if (argument->isTypeParam()) {
    const auto found = activeTypeConstraints_.find(argument->name());
    if (found == activeTypeConstraints_.end() || found->second == nullptr)
      return false;
    const Type* allowed = found->second->canonical();
    if (allowed->isUnion()) {
      return std::all_of(allowed->args().begin(), allowed->args().end(), [&](const Type* member) {
        return satisfiesTypeConstraint(member, constraint);
      });
    }
    return satisfiesTypeConstraint(allowed, constraint);
  }
  if (constraint->isUnion()) {
    return std::any_of(constraint->args().begin(),
                       constraint->args().end(),
                       [&](const Type* member) { return argument == member->canonical(); });
  }
  return argument == constraint;
}

bool TypeChecker::checkTypeConstraints(const std::vector<std::string>& names,
                                       const std::vector<const Type*>& constraints,
                                       const std::vector<const Type*>& args,
                                       SourceRange range) {
  for (std::size_t i = 0; i < constraints.size() && i < args.size(); ++i) {
    if (!satisfiesTypeConstraint(args[i], constraints[i])) {
      diagnostics_->error(range,
                          "type argument " + quoteType(args[i]) + " for '" + names[i] +
                              "' must be one of " + quoteType(constraints[i]));
      return false;
    }
  }
  return true;
}

const Type* TypeChecker::resolveNamedType(const std::string& name,
                                          const std::vector<std::unique_ptr<TypeExpr>>& args,
                                          SourceRange range,
                                          bool reportMissing) {
  std::vector<const Type*> resolvedArgs;
  for (const std::unique_ptr<TypeExpr>& arg : args) {
    const Type* resolved = resolveTypeExpr(*arg);
    if (resolved == nullptr) {
      return nullptr;
    }
    resolvedArgs.push_back(resolved);
  }
  auto finishRecord = [&](const Type* record) -> const Type* {
    if (record == nullptr) {
      return nullptr;
    }
    if (record->isTypeObject() && record->typeObjectInstance() != nullptr) {
      record = record->typeObjectInstance();
    }
    record = record->canonical();
    if (!record->typeParams().empty()) {
      if (!ensureRecordConstraints(record))
        return nullptr;
      if (resolvedArgs.empty()) {
        std::vector<const Type*> defaultArgs(record->typeParams().size(), types_->anyType());
        if (!checkTypeConstraints(
                record->typeParams(), record->typeConstraints(), defaultArgs, range))
          return nullptr;
        return types_->instantiate(record, defaultArgs);
      }
      if (resolvedArgs.size() != record->typeParams().size()) {
        diagnostics_->error(range,
                            "'" + name + "' requires " +
                                std::to_string(record->typeParams().size()) + " type arguments");
        return nullptr;
      }
      if (!checkTypeConstraints(
              record->typeParams(), record->typeConstraints(), resolvedArgs, range))
        return nullptr;
      return types_->instantiate(record, resolvedArgs);
    }
    if (!resolvedArgs.empty()) {
      diagnostics_->error(range, "type '" + name + "' is not generic");
      return nullptr;
    }
    return record;
  };
  if (Symbol* exact = lookup(name);
      exact != nullptr && exact->type != nullptr &&
      (exact->kind == SymbolKind::Class || exact->kind == SymbolKind::Type ||
       exact->kind == SymbolKind::Module || exact->kind == SymbolKind::Variable)) {
    if (exact->kind != SymbolKind::Module) {
      if (const Type* local = finishRecord(exact->type)) {
        return local;
      }
      return nullptr;
    }
  }
  if (name.find('.') != std::string::npos) {
    if (const Type* rec = types_->record(name)) {
      if (const Type* finished = finishRecord(rec)) {
        return finished;
      }
      return nullptr;
    }
    const std::vector<std::string> parts = splitQualifiedName(name);
    if (parts.size() < 2 || parts[0].empty()) {
      if (reportMissing) {
        reportUnknown(range, "type", name);
      }
      return nullptr;
    }
    const Symbol* symbol = lookup(parts[0]);
    const Type* current = symbol == nullptr ? nullptr : symbol->type;
    if (current == nullptr) {
      current = types_->moduleType(parts[0]);
    }
    if (current != nullptr && current->isTypeObject() && current->typeObjectInstance() != nullptr) {
      current = current->typeObjectInstance();
    }
    if (current == nullptr || (!current->isModule() && !current->isRecord())) {
      if (reportMissing) {
        reportUnknown(range, "module", parts[0]);
      }
      return nullptr;
    }
    for (std::size_t index = 1; index < parts.size(); ++index) {
      const RecordField* field = current->findField(parts[index]);
      if (field == nullptr || field->type == nullptr) {
        std::string soFar = parts[0];
        for (std::size_t seen = 1; seen <= index; ++seen) {
          soFar += ".";
          soFar += parts[seen];
        }
        current = types_->record(soFar);
        if (current == nullptr) {
          if (reportMissing) {
            reportUnknown(range, "type", name);
          }
          return nullptr;
        }
        continue;
      }
      if (!field->isPublic) {
        diagnostics_->error(range, "'" + parts[index] + "' is private and is not exported");
        return nullptr;
      }
      current = field->type;
      if (current != nullptr && current->isTypeObject() &&
          current->typeObjectInstance() != nullptr) {
        current = current->typeObjectInstance();
      }
      if (index + 1 < parts.size() && current != nullptr && !current->isModule() &&
          !current->isRecord()) {
        if (reportMissing) {
          reportUnknown(range, "type", name);
        }
        return nullptr;
      }
    }
    return finishRecord(current);
  }
  if (name == "type") {
    if (resolvedArgs.size() != 1) {
      diagnostics_->error(range, "type requires exactly one type argument");
      return nullptr;
    }
    return types_->typeObject(resolvedArgs[0]);
  }
  if (name == "Class") {
    if (resolvedArgs.empty()) {
      return types_->generic("Class", {});
    }
    if (resolvedArgs.size() != 1) {
      diagnostics_->error(range, "Class takes zero or one type argument");
      return nullptr;
    }
    return types_->typeObject(resolvedArgs[0]);
  }
  if (name == "Callable" || name == "Function") {
    if (resolvedArgs.size() > 2) {
      diagnostics_->error(range, name + " takes at most a parameter list and a return type");
      return nullptr;
    }
    if (resolvedArgs.size() == 2 && !resolvedArgs[0]->isParamList()) {
      diagnostics_->error(range,
                          name + " parameter list must be written in brackets, like [[i32], str]");
      return nullptr;
    }
    if (resolvedArgs.size() >= 1 && resolvedArgs[0]->isParamList()) {
      const std::vector<const Type*>& params = resolvedArgs[0]->args();
      for (std::size_t index = 0; index < params.size(); ++index) {
        if (params[index] != nullptr && params[index]->isEllipsis() && index + 1 != params.size()) {
          diagnostics_->error(range, "'...' must be the last parameter");
          return nullptr;
        }
      }
    }
    return types_->generic(name, resolvedArgs);
  }
  if (name == "Unique" || name == "Shared" || name == "Ptr") {
    if (resolvedArgs.empty()) {
      return types_->generic(name, {types_->anyType()});
    }
    if (resolvedArgs.size() != 1) {
      diagnostics_->error(range, name + " requires exactly one type argument");
      return nullptr;
    }
    return types_->generic(name, resolvedArgs);
  }
  if (name == "Task" || name == "Future") {
    // The canonical asynchronous-computation type. `Task[T]` (aliased `Future[T]`)
    // carries one element type; `await` unwraps it to `T`.
    if (resolvedArgs.empty()) {
      return types_->generic("Task", {types_->anyType()});
    }
    if (resolvedArgs.size() != 1) {
      diagnostics_->error(range, name + " requires exactly one type argument");
      return nullptr;
    }
    return types_->generic("Task", resolvedArgs);
  }
  if (name == "list" || name == "array") {
    if (resolvedArgs.empty())
      return types_->generic(name, {types_->anyType()});
    if (resolvedArgs.empty() || resolvedArgs.size() > 2) {
      diagnostics_->error(range, name + " requires a type argument, optionally a size");
      return nullptr;
    }
    if (resolvedArgs.size() == 2) {
      if (resolvedArgs[1] == nullptr || !resolvedArgs[1]->isSizeLiteral() ||
          resolvedArgs[1]->sizeLiteral() < 0) {
        diagnostics_->error(range, name + " size must be a non-negative integer");
        return nullptr;
      }
      if (name == "list") {
        return types_->listType(resolvedArgs[0], resolvedArgs[1]->sizeLiteral());
      }
    }
    return types_->generic(name, std::vector<const Type*>{resolvedArgs[0]});
  }
  if (name == "dict") {
    if (resolvedArgs.empty())
      return types_->dictType(types_->anyType(), types_->anyType());
    if (resolvedArgs.size() != 2) {
      diagnostics_->error(range, "dict requires key and value type arguments");
      return nullptr;
    }
    return types_->generic(name, resolvedArgs);
  }
  if (name == "tuple") {
    if (resolvedArgs.empty()) {
      diagnostics_->error(range, "tuple requires element type arguments");
      return nullptr;
    }
    return types_->generic(name, resolvedArgs);
  }
  if (resolvedArgs.empty() && name == "None") {
    return types_->noneType();
  }
  if (resolvedArgs.empty() && name == "Any") {
    return types_->anyType();
  }
  if (const Type* param = types_->typeParam(name)) {
    if (!resolvedArgs.empty()) {
      diagnostics_->error(range, "type parameter '" + name + "' is not generic");
      return nullptr;
    }
    return param;
  }
  if (Symbol* symbol = lookup(name);
      symbol != nullptr && symbol->type != nullptr &&
      (symbol->kind == SymbolKind::Class || symbol->kind == SymbolKind::Type)) {
    if (const Type* local = finishRecord(symbol->type)) {
      return local;
    }
    return nullptr;
  }
  if (const Type* localClass = classType(name)) {
    if (const Type* resolved = finishRecord(localClass)) {
      return resolved;
    }
    return nullptr;
  }
  if (!resolvedArgs.empty()) {
    diagnostics_->error(range, "type '" + name + "' is not generic");
    return nullptr;
  }
  if (const Type* primitive = types_->primitive(name)) {
    return primitive;
  }
  if (const Type* alias = types_->alias(name)) {
    return alias;
  }
  if (reportMissing) {
    reportUnknown(range, "type", name);
  }
  return nullptr;
}

const Type* TypeChecker::classType(const std::string& name) const {
  const auto found = classes_.find(name);
  if (found != classes_.end() && found->second != nullptr &&
      found->second->resolvedType() != nullptr) {
    return found->second->resolvedType();
  }
  if (const Symbol* symbol = const_cast<TypeChecker*>(this)->lookup(name);
      symbol != nullptr && symbol->type != nullptr) {
    if (const Type* record = unwrapRecordType(symbol->type)) {
      return record;
    }
  }
  if (const Type* qualified = types_->record(name)) {
    return qualified;
  }
  if (const Type* qualified = types_->record(moduleName_ + "." + name)) {
    return qualified;
  }
  return nullptr;
}

const Type* TypeChecker::resolveTypeFromExpr(Expr& expr, bool reportMissing) {
  if (expr.kind() == NodeKind::NoneLiteral) {
    return types_->noneType();
  }
  if (expr.kind() == NodeKind::NameExpr) {
    const auto& name = static_cast<const NameExpr&>(expr);
    return resolveNamedType(name.name(), {}, expr.range(), reportMissing);
  }
  if (expr.kind() == NodeKind::MemberExpr) {
    std::string spelling;
    const Expr* current = &expr;
    while (current != nullptr && current->kind() == NodeKind::MemberExpr) {
      const auto& member = static_cast<const MemberExpr&>(*current);
      if (spelling.empty()) {
        spelling = member.field();
      } else {
        spelling = member.field() + "." + spelling;
      }
      current = &member.object();
    }
    if (current != nullptr && current->kind() == NodeKind::NameExpr) {
      spelling = static_cast<const NameExpr&>(*current).name() + "." + spelling;
      return resolveNamedType(spelling, {}, expr.range(), reportMissing);
    }
    if (reportMissing) {
      diagnostics_->error(expr.range(), "expected a type name");
    }
    return nullptr;
  }
  if (expr.kind() == NodeKind::CallExpr) {
    auto& call = static_cast<CallExpr&>(expr);
    if (!call.typeArgs().empty() && call.arguments().empty()) {
      if (const NameExpr* name = asName(call.callee())) {
        return resolveNamedType(name->name(), call.typeArgs(), expr.range(), reportMissing);
      }
      if (call.callee().kind() == NodeKind::MemberExpr) {
        return resolveTypeFromExpr(call.callee(), reportMissing);
      }
    }
  }
  if (reportMissing) {
    diagnostics_->error(expr.range(), "expected a type name");
  }
  return nullptr;
}

const Type* TypeChecker::resolveTypeExpr(const TypeExpr& expr) {
  if (!expr.name().empty() && expr.args().empty()) {
    bool digits = true;
    std::size_t index = expr.name()[0] == '-' ? 1 : 0;
    if (index >= expr.name().size()) {
      digits = false;
    }
    for (; digits && index < expr.name().size(); ++index) {
      if (expr.name()[index] < '0' || expr.name()[index] > '9') {
        digits = false;
      }
    }
    if (digits) {
      const Type* size = types_->sizeType(std::strtoll(expr.name().c_str(), nullptr, 10));
      const_cast<TypeExpr&>(expr).setResolvedType(size);
      return size;
    }
  }
  if (expr.name() == "|") {
    std::vector<const Type*> members;
    for (const std::unique_ptr<TypeExpr>& arg : expr.args()) {
      if (arg == nullptr) {
        return nullptr;
      }
      const Type* member = resolveTypeExpr(*arg);
      if (member == nullptr) {
        return nullptr;
      }
      members.push_back(member);
    }
    const Type* result = types_->unionType(std::move(members));
    const_cast<TypeExpr&>(expr).setResolvedType(result);
    return result;
  }
  if (expr.name() == "...") {
    const Type* result = types_->ellipsisType();
    const_cast<TypeExpr&>(expr).setResolvedType(result);
    return result;
  }
  if (expr.name() == "[]") {
    std::vector<const Type*> params;
    for (const std::unique_ptr<TypeExpr>& arg : expr.args()) {
      const Type* member = resolveTypeExpr(*arg);
      if (member == nullptr) {
        return nullptr;
      }
      params.push_back(member);
    }
    const Type* result = types_->paramList(params);
    const_cast<TypeExpr&>(expr).setResolvedType(result);
    return result;
  }
  const Type* result = resolveNamedType(expr.name(), expr.args(), expr.range(), true);
  if (result != nullptr) {
    const_cast<TypeExpr&>(expr).setResolvedType(result);
  }
  return result;
}

const Type* TypeChecker::checkCastValue(Expr& value, const Type* target, SourceRange range) {
  const Type* source = checkExpr(value);
  if (source == nullptr || target == nullptr) {
    return nullptr;
  }
  if (!canCast(source, target)) {
    diagnostics_->error(range, "cannot cast " + quoteType(source) + " to " + quoteType(target));
    return nullptr;
  }
  return target;
}

const Type* TypeChecker::checkCast(CastExpr& expr) {
  const Type* target = resolveTypeExpr(expr.target());
  const Type* result = checkCastValue(expr.value(), target, expr.range());
  expr.setResolvedType(result);
  return result;
}

const Type* TypeChecker::checkName(NameExpr& expr) {
  Symbol* symbol = lookup(expr.name());
  if (symbol == nullptr && expr.name() == "void") {
    expr.setResolvedType(types_->voidType());
    return types_->voidType();
  }
  if (symbol == nullptr && expr.name() == "None") {
    expr.setResolvedType(types_->noneType());
    return types_->noneType();
  }
  if (symbol == nullptr && expr.name() == "Any") {
    expr.setResolvedType(types_->typeObject(types_->anyType()));
    return expr.resolvedType();
  }
  if (symbol == nullptr && expr.name() == "super") {
    diagnostics_->error(expr.range(), "super must be called");
    diagnostics_->help("write super().__init__(...) or super().method(...)");
    return nullptr;
  }
  if (symbol == nullptr || symbol->kind == SymbolKind::Intrinsic) {
    const Type* builtin = types_->primitive(expr.name());
    if (expr.name() == "list")
      builtin = types_->listType(types_->anyType());
    if (expr.name() == "array")
      builtin = types_->arrayType(types_->anyType());
    if (expr.name() == "dict")
      builtin = types_->dictType(types_->anyType(), types_->anyType());
    if (builtin != nullptr && builtin->isClass()) {
      expr.setResolvedType(types_->typeObject(builtin));
      return expr.resolvedType();
    }
  }
  if (symbol == nullptr) {
    reportUnknown(expr.range(), "name", expr.name());
    if (expr.name().size() > 0 && std::isupper(static_cast<unsigned char>(expr.name()[0])) != 0) {
      diagnostics_->help("enum variants are accessed as Type.Variant");
    }
    return nullptr;
  }
  if (symbol->type == nullptr && symbol->kind != SymbolKind::Intrinsic) {
    reportUnknown(expr.range(), "name", expr.name());
    return nullptr;
  }
  if (!symbol->compileTimeText.empty()) {
    expr.setCompileTimeText(symbol->compileTimeText);
  }
  if (symbol->hasCompileTimeBool) {
    expr.setCompileTimeBool(symbol->compileTimeBool);
  }
  if (symbol->type == nullptr) {
    diagnostics_->error(expr.range(), "cannot use '" + expr.name() + "' as a value");
    return nullptr;
  }
  if ((symbol->kind == SymbolKind::Class || symbol->kind == SymbolKind::Type) &&
      symbol->type->isClass()) {
    const Type* meta = types_->typeObject(symbol->type);
    expr.setResolvedType(meta);
    return meta;
  }
  expr.setResolvedType(symbol->type);
  if (nestedFunction_ != nullptr && symbol->kind == SymbolKind::Variable) {
    int scopeIndex = -1;
    for (int index = static_cast<int>(scopes_.size()) - 1; index >= 0; --index) {
      if (scopes_[static_cast<std::size_t>(index)].contains(expr.name())) {
        scopeIndex = index;
        break;
      }
    }
    if (scopeIndex > 0 && static_cast<std::size_t>(scopeIndex) <= nestedOuterScope_) {
      nestedFunction_->addCapture(expr.name(), symbol->type);
      for (const auto& [function, boundary] : enclosingFunctions_) {
        if (static_cast<std::size_t>(scopeIndex) <= boundary) {
          function->addCapture(expr.name(), symbol->type);
        }
      }
    }
  }
  if (lambdaDepth_ > 0 && symbol->kind == SymbolKind::Variable) {
    int scopeIndex = -1;
    for (int index = static_cast<int>(scopes_.size()) - 1; index >= 0; --index) {
      if (scopes_[static_cast<std::size_t>(index)].contains(expr.name())) {
        scopeIndex = index;
        break;
      }
    }
    if (scopeIndex > 0 && scopeIndex < static_cast<int>(scopes_.size()) - 1) {
      diagnostics_->error(expr.range(), "lambda cannot capture local '" + expr.name() + "'");
      diagnostics_->help("pass '" + expr.name() + "' as a parameter, or use a nested def");
      return nullptr;
    }
  }
  return symbol->type;
}

bool TypeChecker::isClassName(const Expr& expr) {
  const NameExpr* name = asName(expr);
  if (name == nullptr) {
    return false;
  }
  const Symbol* symbol = lookup(name->name());
  return symbol != nullptr && symbol->kind == SymbolKind::Class;
}

const Type* TypeChecker::checkMember(MemberExpr& expr) {
  const Type* objectType = checkExpr(expr.object());
  if (objectType == nullptr) {
    return nullptr;
  }
  objectType = objectType->canonical();
  const bool typeObject = objectType->isTypeObject();
  if (typeObject) {
    const Type* instance = objectType->typeObjectInstance();
    if (instance == nullptr) {
      reportUnknownMember(*diagnostics_, expr.range(), objectType, expr.field(), false);
      return nullptr;
    }
    objectType = instance->canonical();
  }
  if (expr.field().empty()) {
    expr.setResolvedType(objectType);
    return objectType;
  }
  if (expr.field() == "__name__" || expr.field() == "__type__" || expr.field() == "__module__" ||
      expr.field() == "__qualname__") {
    if (expr.field() == "__name__") {
      if (const NameExpr* name = asName(expr.object())) {
        expr.setCompileTimeText(name->name());
      } else {
        expr.setCompileTimeText(objectType->name().empty() ? objectType->display()
                                                           : objectType->name());
      }
    } else if (expr.field() == "__module__") {
      expr.setCompileTimeText(moduleName_);
    } else if (expr.field() == "__qualname__") {
      if (const NameExpr* name = asName(expr.object())) {
        expr.setCompileTimeText(name->name());
      } else {
        expr.setCompileTimeText(objectType->display());
      }
    } else {
      expr.setCompileTimeText(objectType->display());
    }
    expr.setResolvedType(types_->strType());
    return types_->strType();
  }
  if (objectType->isEnum()) {
    if (expr.field() == "name") {
      expr.setResolvedType(types_->strType());
      return types_->strType();
    }
    if (expr.field() == "value") {
      expr.setResolvedType(types_->i32Type());
      return types_->i32Type();
    }
  }
  if (objectType->isModule() && expr.field() == "__file__") {
    expr.setCompileTimeText(objectType->name());
    expr.setResolvedType(types_->strType());
    return types_->strType();
  }
  if (objectType->isModule()) {
    const RecordField* exportField = objectType->findField(expr.field());
    if (exportField == nullptr) {
      reportUnknownMember(*diagnostics_, expr.range(), objectType, expr.field(), false);
      return nullptr;
    }
    if (!exportField->isPublic) {
      diagnostics_->error(expr.range(), "'" + expr.field() + "' is private and is not exported");
      return nullptr;
    }
    const Type* exported = exportField->type;
    if (exported != nullptr && exported->isRecord() && !exported->isEnum()) {
      exported = types_->typeObject(exported);
    }
    expr.setResolvedType(exported);
    return exported;
  }
  const RecordField* field = objectType->findField(expr.field());
  if (field == nullptr) {
    const int methodIndex = objectType->methodIndex(expr.field());
    if (methodIndex >= 0) {
      const RecordMethod& method = objectType->methods()[static_cast<std::size_t>(methodIndex)];
      if (!method.isPublic && !canAccessPrivate(objectType)) {
        diagnostics_->error(expr.range(),
                            "method '" + expr.field() + "' of '" + objectType->name() +
                                "' is private");
        return nullptr;
      }
      if (method.isAbstract) {
        diagnostics_->error(expr.range(),
                            "cannot take a reference to abstract method '" + expr.field() + "'");
        return nullptr;
      }
      const Type* functionType = method.type;
      if (functionType == nullptr || functionType->paramTypes().empty()) {
        return nullptr;
      }
      std::vector<const Type*> params;
      for (std::size_t index = 1; index < functionType->paramTypes().size(); ++index) {
        params.push_back(functionType->paramTypes()[index]);
      }
      const Type* bound = types_->functionType(params, functionType->returnType());
      if (isClassName(expr.object()) || typeObject) {
        expr.setUnboundMethod(method.llvmName);
      } else {
        expr.setBoundMethod(method.llvmName);
      }
      expr.setResolvedType(bound);
      return bound;
    }
    reportUnknownMember(*diagnostics_, expr.range(), objectType, expr.field(), false);
    return nullptr;
  }
  const bool ownAccessor =
      currentClass_ == objectType->name() && currentPropertyName_ == expr.field();
  const bool initBacking =
      currentClass_ == objectType->name() && currentFunctionName_ == "__init__" && field->stored;
  const bool assignThroughSetter = !expr.propertySet().empty() && !ownAccessor && !initBacking;
  bool useBacking =
      ownAccessor || initBacking || (field->getterLlvm.empty() && !assignThroughSetter);
  if (useBacking && !field->stored) {
    useBacking = !field->getterLlvm.empty() ? false : useBacking;
  }
  expr.setBackingField(useBacking);
  if (useBacking) {
    if (!field->stored) {
      diagnostics_->error(expr.range(), "property '" + expr.field() + "' has no backing field");
      return nullptr;
    }
    if (!field->isPublic && !canAccessPrivate(objectType)) {
      diagnostics_->error(
          expr.range(), "field '" + expr.field() + "' of '" + objectType->name() + "' is private");
      return nullptr;
    }
  } else if (!field->getterLlvm.empty()) {
    if (!field->getterPublic && !canAccessPrivate(objectType)) {
      diagnostics_->error(
          expr.range(), "getter '" + expr.field() + "' of '" + objectType->name() + "' is private");
      return nullptr;
    }
    expr.setPropertyGet(field->getterLlvm);
  }
  if ((isClassName(expr.object()) || typeObject) && !field->isStatic) {
    diagnostics_->error(expr.range(),
                        "instance field '" + expr.field() + "' cannot be accessed on the class");
    return nullptr;
  }
  expr.setResolvedType(field->type);
  return field->type;
}

const Type* TypeChecker::checkIndex(IndexExpr& expr) {
  const Type* objectType = checkExpr(expr.object());
  if (objectType == nullptr) {
    return nullptr;
  }
  objectType = objectType->canonical();
  if (objectType->methodIndex("__getitem__") < 0)
    objectType = objectType->valueType();
  if (expr.isSlice()) {
    if (!(objectType->isSequence() || objectType->isNamed("str") ||
          objectType->methodIndex("__getitem__") >= 0)) {
      diagnostics_->error(expr.range(),
                          "slice requires a list, array, str, or __getitem__, found " +
                              quoteType(objectType));
      return nullptr;
    }
    if (expr.start() != nullptr) {
      const Type* startType = checkExpr(*expr.start());
      if (startType == nullptr || !startType->isInteger()) {
        diagnostics_->error(expr.start()->range(), "slice start must be an integer");
        return nullptr;
      }
    }
    if (expr.stop() != nullptr) {
      const Type* stopType = checkExpr(*expr.stop());
      if (stopType == nullptr || !stopType->isInteger()) {
        diagnostics_->error(expr.stop()->range(), "slice end must be an integer");
        return nullptr;
      }
    }
    expr.setResolvedType(objectType);
    return objectType;
  }
  if (expr.start() == nullptr) {
    diagnostics_->error(expr.range(), "expected index expression");
    return nullptr;
  }
  const Type* indexType = checkExpr(*expr.start());
  if (indexType == nullptr) {
    return nullptr;
  }
  if (objectType->isNamed("str")) {
    if (!indexType->isInteger()) {
      diagnostics_->error(expr.start()->range(),
                          "string index must be an integer, found " + quoteType(indexType));
      return nullptr;
    }
    expr.setResolvedType(types_->strType());
    return types_->strType();
  }
  if (objectType->isSequence()) {
    if (!indexType->isInteger()) {
      diagnostics_->error(expr.start()->range(),
                          "sequence index must be an integer, found " + quoteType(indexType));
      return nullptr;
    }
    expr.setResolvedType(objectType->elementType());
    return objectType->elementType();
  }
  if (objectType->isDict()) {
    if (!isAssignable(indexType, objectType->dictKeyType())) {
      diagnostics_->error(expr.start()->range(),
                          "dict key type mismatch: expected " +
                              quoteType(objectType->dictKeyType()) + ", found " +
                              quoteType(indexType));
      return nullptr;
    }
    expr.setResolvedType(objectType->dictValueType());
    return objectType->dictValueType();
  }
  const int getIndex = objectType->methodIndex("__getitem__");
  if (getIndex >= 0) {
    const RecordMethod& method = objectType->methods()[static_cast<std::size_t>(getIndex)];
    const Type* fn = method.type;
    if (fn == nullptr || fn->paramTypes().size() < 2) {
      diagnostics_->error(expr.range(), "__getitem__ must be def __getitem__(self, index: T) -> U");
      return nullptr;
    }
    if (!isAssignable(indexType, fn->paramTypes()[1])) {
      diagnostics_->error(expr.start()->range(),
                          "index type mismatch: expected " + quoteType(fn->paramTypes()[1]) +
                              ", found " + quoteType(indexType));
      return nullptr;
    }
    expr.setResolvedType(fn->returnType());
    return fn->returnType();
  }
  diagnostics_->error(expr.range(), quoteType(objectType) + " is not indexable");
  diagnostics_->help("define __getitem__ or use a sequence");
  return nullptr;
}

const Type* TypeChecker::checkListLiteral(ListLiteral& expr) {
  if (expr.elements().empty()) {
    diagnostics_->error(expr.range(), "cannot infer type of empty list; annotate the variable");
    return nullptr;
  }
  const Type* element = nullptr;
  for (const std::unique_ptr<Expr>& item : expr.elements()) {
    const Type* itemType = checkExpr(*item);
    if (itemType == nullptr) {
      return nullptr;
    }
    if (element == nullptr) {
      element = itemType;
    } else if (element->isAny() || itemType->isAny()) {
      element = types_->primitive("Any");
    } else if (const Type* numeric = joinNumeric(*types_, element, itemType)) {
      element = numeric;
    } else if (!isAssignable(itemType, element)) {
      element = types_->primitive("Any");
    }
  }
  const Type* result = types_->listType(element);
  expr.setResolvedType(result);
  return result;
}

const Type* TypeChecker::checkDictLiteral(DictLiteral& expr) {
  if (expr.keys().empty()) {
    diagnostics_->error(expr.range(), "cannot infer type of empty dict; annotate the variable");
    return nullptr;
  }
  const Type* keyType = nullptr;
  const Type* valueType = nullptr;
  for (std::size_t index = 0; index < expr.keys().size(); ++index) {
    const Type* nextKey = checkExpr(*expr.keys()[index]);
    const Type* nextValue = checkExpr(*expr.values()[index]);
    if (nextKey == nullptr || nextValue == nullptr) {
      return nullptr;
    }
    if (keyType == nullptr) {
      keyType = nextKey;
      valueType = nextValue;
      continue;
    }
    if (const Type* numeric = joinNumeric(*types_, keyType, nextKey)) {
      keyType = numeric;
    } else if (!isAssignable(nextKey, keyType)) {
      const Type* joinedKey = joinNumeric(*types_, keyType, nextKey);
      if (joinedKey == nullptr || !isAssignable(keyType, joinedKey) ||
          !isAssignable(nextKey, joinedKey)) {
        diagnostics_->error(expr.keys()[index]->range(),
                            "dict entries must share key and value types");
        return nullptr;
      }
      keyType = joinedKey;
    }
    if (valueType->isAny() || nextValue->isAny()) {
      valueType = types_->primitive("Any");
    } else if (const Type* numeric = joinNumeric(*types_, valueType, nextValue)) {
      valueType = numeric;
    } else if (!isAssignable(nextValue, valueType)) {
      valueType = types_->primitive("Any");
    }
  }
  const Type* result = types_->dictType(keyType, valueType);
  expr.setResolvedType(result);
  return result;
}

bool TypeChecker::bindCollectionInit(Expr& init, const Type* dest) {
  if (dest == nullptr) {
    return false;
  }
  dest = dest->canonical();
  const auto checkElement = [&](Expr& value, const Type* expected) -> const Type* {
    if (expected != nullptr && ((value.kind() == NodeKind::ListLiteral && expected->isSequence()) ||
                                (value.kind() == NodeKind::DictLiteral && expected->isDict()))) {
      return bindCollectionInit(value, expected) ? expected : nullptr;
    }
    return checkExpr(value);
  };
  if (init.kind() == NodeKind::ListLiteral && dest->isSequence()) {
    auto& literal = static_cast<ListLiteral&>(init);
    for (const std::unique_ptr<Expr>& item : literal.elements()) {
      const Type* itemType = checkElement(*item, dest->elementType());
      if (itemType == nullptr || !isAssignable(itemType, dest->elementType())) {
        diagnostics_->error(item->range(),
                            "cannot store " + quoteType(itemType) + " in " + quoteType(dest));
        return false;
      }
      if (!ensureLiteralFits(*item, dest->elementType())) {
        return false;
      }
    }
    if (dest->isList() && dest->listSize() >= 0 &&
        static_cast<std::int64_t>(literal.elements().size()) != dest->listSize()) {
      diagnostics_->error(
          init.range(), "list literal must have " + std::to_string(dest->listSize()) + " elements");
      return false;
    }
    init.setResolvedType(dest);
    return true;
  }
  if (init.kind() == NodeKind::DictLiteral && dest->isDict()) {
    auto& literal = static_cast<DictLiteral&>(init);
    for (std::size_t index = 0; index < literal.keys().size(); ++index) {
      const Type* keyType = checkElement(*literal.keys()[index], dest->dictKeyType());
      const Type* valueType = checkElement(*literal.values()[index], dest->dictValueType());
      if (keyType == nullptr || valueType == nullptr ||
          !isAssignable(keyType, dest->dictKeyType()) ||
          !isAssignable(valueType, dest->dictValueType())) {
        diagnostics_->error(literal.keys()[index]->range(),
                            "cannot store this entry in " + quoteType(dest));
        return false;
      }
      if (!ensureLiteralFits(*literal.keys()[index], dest->dictKeyType()) ||
          !ensureLiteralFits(*literal.values()[index], dest->dictValueType())) {
        return false;
      }
    }
    init.setResolvedType(dest);
    return true;
  }
  return false;
}

const Type*
TypeChecker::rewriteDunderBinary(BinaryExpr& expr, const Type* left, const Type* right) {
  const char* name = nullptr;
  const char* reflected = nullptr;
  switch (expr.op()) {
  case BinaryOp::Add:
    name = "__add__";
    reflected = "__radd__";
    break;
  case BinaryOp::Sub:
    name = "__sub__";
    reflected = "__rsub__";
    break;
  case BinaryOp::Mul:
    name = "__mul__";
    reflected = "__rmul__";
    break;
  case BinaryOp::Div:
    name = "__truediv__";
    reflected = "__rtruediv__";
    break;
  case BinaryOp::FloorDiv:
    name = "__floordiv__";
    reflected = "__rfloordiv__";
    break;
  case BinaryOp::Mod:
    name = "__mod__";
    reflected = "__rmod__";
    break;
  case BinaryOp::Pow:
    name = "__pow__";
    reflected = "__rpow__";
    break;
  case BinaryOp::Eq:
    name = "__eq__";
    break;
  case BinaryOp::Ne:
    name = "__ne__";
    break;
  case BinaryOp::Lt:
    name = "__lt__";
    break;
  case BinaryOp::Le:
    name = "__le__";
    break;
  case BinaryOp::Gt:
    name = "__gt__";
    break;
  case BinaryOp::Ge:
    name = "__ge__";
    break;
  case BinaryOp::BitAnd:
    name = "__and__";
    reflected = "__rand__";
    break;
  case BinaryOp::BitOr:
    name = "__or__";
    reflected = "__ror__";
    break;
  case BinaryOp::BitXor:
    name = "__xor__";
    reflected = "__rxor__";
    break;
  case BinaryOp::Shl:
    name = "__lshift__";
    reflected = "__rlshift__";
    break;
  case BinaryOp::Shr:
    name = "__rshift__";
    reflected = "__rrshift__";
    break;
  default:
    break;
  }
  if (name == nullptr) {
    return nullptr;
  }
  if (left->isRecord() && left->methodIndex(name) >= 0) {
    const Type* result = left->dunderReturn(name);
    if (result != nullptr) {
      expr.setResolvedType(result);
      return result;
    }
  }
  if (reflected != nullptr && right->isRecord() && right->methodIndex(reflected) >= 0) {
    const Type* result = right->dunderReturn(reflected);
    if (result != nullptr) {
      expr.setResolvedType(result);
      return result;
    }
  }
  if (expr.op() == BinaryOp::Ne && left->isRecord() && left->methodIndex("__eq__") >= 0) {
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  }
  return nullptr;
}

const Type* TypeChecker::checkBinary(BinaryExpr& expr) {
  const Type* left = checkExpr(expr.left());
  const Type* right = checkExpr(expr.right());
  if (left == nullptr || right == nullptr) {
    return nullptr;
  }
  const BinaryOp op = expr.op();
  if (op == BinaryOp::And || op == BinaryOp::Or) {
    if (!left->isNamed("bool") || !right->isNamed("bool")) {
      diagnostics_->error(expr.range(),
                          "logical operators require bool operands, found " + quoteType(left) +
                              " and " + quoteType(right));
      return nullptr;
    }
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  }
  if (op == BinaryOp::Is || op == BinaryOp::IsNot) {
    if (const Type* typeRhs = resolveTypeFromExpr(expr.right(), false)) {
      expr.right().setResolvedType(typeRhs);
    }
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  }
  if (op == BinaryOp::In || op == BinaryOp::NotIn) {
    if (right->isNamed("str") || right->isSequence() || right->isDict() ||
        right->methodIndex("__contains__") >= 0 ||
        (right->isEnum() && right->isFlags() && left->isIntEnum() &&
         left->canonical() == right->canonical()) ||
        (left->isIntEnum() && right->isInteger())) {
      expr.setResolvedType(types_->boolType());
      return types_->boolType();
    }
    diagnostics_->error(expr.range(), quoteType(right) + " does not support 'in'");
    return nullptr;
  }
  if ((op == BinaryOp::Eq || op == BinaryOp::Ne) &&
      ((left->isPointerLike() && right->isVoidLike()) ||
       (right->isPointerLike() && left->isVoidLike()) ||
       (left->isPointerLike() && right->isPointerLike() &&
        left->pointeeType()->canonical() == right->pointeeType()->canonical()))) {
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  }
  if (const Type* overloaded = rewriteDunderBinary(expr, left, right)) {
    return overloaded;
  }
  left = left->valueType();
  right = right->valueType();
  if (op == BinaryOp::Add && left->isNamed("str") && right->isNamed("str")) {
    expr.setResolvedType(types_->strType());
    return types_->strType();
  }
  if (op == BinaryOp::Mul && left->isNamed("str") && right->isInteger()) {
    expr.setResolvedType(types_->strType());
    return types_->strType();
  }
  if (op == BinaryOp::Mul && left->isInteger() && right->isNamed("str")) {
    expr.setResolvedType(types_->strType());
    return types_->strType();
  }
  if (op == BinaryOp::Add && left->isList() && right->isList() &&
      left->elementType() == right->elementType()) {
    expr.setResolvedType(left);
    return left;
  }
  if (op == BinaryOp::Eq || op == BinaryOp::Ne || op == BinaryOp::Lt || op == BinaryOp::Le ||
      op == BinaryOp::Gt || op == BinaryOp::Ge) {
    const bool same = left->canonical() == right->canonical();
    const bool ints = left->isInteger() && right->isInteger();
    const bool floats = left->isFloat() && right->isFloat();
    const bool mixedNum =
        (left->isInteger() || left->isFloat()) && (right->isInteger() || right->isFloat());
    const bool strs = left->isNamed("str") && right->isNamed("str");
    const bool enums = left->isEnum() && right->isEnum() && left->canonical() == right->canonical();
    const bool enumInt =
        (left->isIntEnum() && right->isInteger()) || (right->isIntEnum() && left->isInteger());
    if (!(same || ints || floats || mixedNum || strs || enums || enumInt)) {
      diagnostics_->error(expr.range(),
                          "cannot compare " + quoteType(left) + " with " + quoteType(right));
      return nullptr;
    }
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  }
  if (op == BinaryOp::BitAnd || op == BinaryOp::BitOr || op == BinaryOp::BitXor ||
      op == BinaryOp::Shl || op == BinaryOp::Shr) {
    if (!left->isInteger() || !right->isInteger()) {
      const bool leftBits = left->isInteger() || left->isIntEnum();
      const bool rightBits = right->isInteger() || right->isIntEnum();
      if (!leftBits || !rightBits) {
        diagnostics_->error(expr.range(), "bitwise operators require integers");
        return nullptr;
      }
      if (left->isIntEnum() && right->isIntEnum() && left->canonical() == right->canonical()) {
        expr.setResolvedType(left);
        return left;
      }
      expr.setResolvedType(types_->i32Type());
      return types_->i32Type();
    }
    const Type* result = left->integerBitWidth() >= right->integerBitWidth() ? left : right;
    expr.setResolvedType(result);
    return result;
  }
  const bool leftNum = left->isInteger() || left->isFloat() || left->isIntEnum();
  const bool rightNum = right->isInteger() || right->isFloat() || right->isIntEnum();
  if (!leftNum || !rightNum) {
    if (left->isEnum() || right->isEnum()) {
      diagnostics_->error(expr.range(),
                          "enum " + quoteType(left->isEnum() ? left : right) +
                              " is not an integer; use .value or `as i32`");
      return nullptr;
    }
    diagnostics_->error(expr.range(),
                        "arithmetic requires numeric types, found " + quoteType(left) + " and " +
                            quoteType(right));
    return nullptr;
  }
  const Type* result = left;
  if (left->isFloat() || right->isFloat()) {
    result = (left->isNamed("f64") || right->isNamed("f64")) ? types_->primitive("f64")
                                                             : types_->primitive("f32");
  } else if (left->isIntEnum() || right->isIntEnum()) {
    if (left->isIntEnum() && right->isIntEnum() && left->canonical() == right->canonical()) {
      result = left;
    } else if (left->isInteger()) {
      result = left;
    } else if (right->isInteger()) {
      result = right;
    } else {
      result = types_->i32Type();
    }
  } else {
    result = left->integerBitWidth() >= right->integerBitWidth() ? left : right;
  }
  expr.setResolvedType(result);
  return result;
}

const Type* TypeChecker::checkUnary(UnaryExpr& expr) {
  const Type* operand = checkExpr(expr.operand());
  if (operand == nullptr) {
    return nullptr;
  }
  if (expr.op() == UnaryOp::Deref) {
    return checkDeref(expr, operand);
  }
  if (expr.op() == UnaryOp::AddrOf) {
    return checkAddrOf(expr, operand);
  }
  operand = operand->valueType();
  if (expr.op() == UnaryOp::Not) {
    if (!operand->isNamed("bool") && operand->methodIndex("__bool__") < 0) {
      diagnostics_->error(expr.range(), "'not' requires a bool, found " + quoteType(operand));
      return nullptr;
    }
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  }
  if (expr.op() == UnaryOp::Invert) {
    if (!operand->isInteger() && !operand->isIntEnum() && operand->methodIndex("__invert__") < 0) {
      diagnostics_->error(expr.range(), "'~' requires an integer");
      return nullptr;
    }
    expr.setResolvedType(
        operand->methodIndex("__invert__") >= 0 ? operand->dunderReturn("__invert__") : operand);
    return expr.resolvedType();
  }
  if (expr.op() == UnaryOp::PreInc || expr.op() == UnaryOp::PreDec ||
      expr.op() == UnaryOp::PostInc || expr.op() == UnaryOp::PostDec) {
    if (!operand->isInteger() && !operand->isFloat()) {
      diagnostics_->error(expr.range(), "++/-- requires a number, found " + quoteType(operand));
      return nullptr;
    }
    expr.setResolvedType(operand);
    return operand;
  }
  if (operand->isRecord() &&
      ((expr.op() == UnaryOp::Neg && operand->methodIndex("__neg__") >= 0) ||
       (expr.op() == UnaryOp::Pos && operand->methodIndex("__pos__") >= 0))) {
    const char* name = expr.op() == UnaryOp::Neg ? "__neg__" : "__pos__";
    expr.setResolvedType(operand->dunderReturn(name));
    return expr.resolvedType();
  }
  if (!operand->isInteger() && !operand->isFloat()) {
    diagnostics_->error(expr.range(),
                        "unary '+'/'-' requires a number, found " + quoteType(operand));
    return nullptr;
  }
  expr.setResolvedType(operand);
  return operand;
}

const Type* TypeChecker::taskType(const Type* inner) const {
  return types_->generic("Task", {inner});
}

const Type* TypeChecker::unwrapTask(const Type* task) const {
  if (task == nullptr) {
    return nullptr;
  }
  const Type* t = task->canonical();
  if (t != nullptr && t->isGenericCtor("Task") && t->args().size() == 1) {
    return t->args()[0];
  }
  return nullptr;
}

const Type* TypeChecker::checkAwait(AwaitExpr& expr) {
  if (!currentFunctionIsAsync_) {
    diagnostics_->error(expr.range(), "'await' may only be used inside an async function");
    diagnostics_->help("declare the enclosing function with 'async def' to use 'await'");
    return nullptr;
  }
  const Type* operand = checkExpr(expr.operand());
  if (operand == nullptr) {
    return nullptr;
  }
  const Type* inner = unwrapTask(operand);
  if (inner == nullptr) {
    diagnostics_->error(expr.range(), "cannot await value of type " + quoteType(operand));
    diagnostics_->help("await expects a Task[T] produced by calling an async function");
    return nullptr;
  }
  expr.setResolvedType(inner);
  return inner;
}

const Type* TypeChecker::checkDeref(UnaryExpr& expr, const Type* operand) {
  if (!operand->isPointerLike()) {
    diagnostics_->error(expr.range(),
                        "unary '*' requires Unique[T], Shared[T], or Ptr[T], found " +
                            quoteType(operand));
    return nullptr;
  }
  const Type* pointee = operand->pointeeType();
  if (pointee == nullptr || pointee->isVoidLike()) {
    diagnostics_->error(expr.range(), "cannot dereference " + quoteType(operand));
    return nullptr;
  }
  expr.setResolvedType(pointee);
  return pointee;
}

const Type* TypeChecker::checkAddrOf(UnaryExpr& expr, const Type* operand) {
  if (!isAddressable(expr.operand())) {
    diagnostics_->error(expr.range(), "cannot take the address of this expression");
    return nullptr;
  }
  if (const NameExpr* name = asName(expr.operand())) {
    const Symbol* symbol = lookup(name->name());
    if (symbol != nullptr && (symbol->kind != SymbolKind::Variable || symbol->readonly)) {
      diagnostics_->error(expr.range(), "cannot take the address of '" + name->name() + "'");
      return nullptr;
    }
  }
  if (expr.operand().kind() == NodeKind::IndexExpr) {
    const auto& index = static_cast<const IndexExpr&>(expr.operand());
    const Type* objectType = index.object().resolvedType();
    if (objectType == nullptr || !objectType->isSequence()) {
      diagnostics_->error(expr.range(), "cannot take the address of this index expression");
      return nullptr;
    }
  }
  const Type* pointer = types_->ptrType(operand);
  expr.setResolvedType(pointer);
  return pointer;
}

std::optional<bool> TypeChecker::constBool(const Expr& expr) const {
  if (expr.kind() == NodeKind::BooleanLiteral) {
    return static_cast<const BooleanLiteral&>(expr).value();
  }
  if (expr.kind() == NodeKind::NameExpr) {
    const auto& name = static_cast<const NameExpr&>(expr);
    if (name.hasCompileTimeBool()) {
      return name.compileTimeBool();
    }
  }
  if (expr.kind() == NodeKind::UnaryExpr) {
    const auto& unary = static_cast<const UnaryExpr&>(expr);
    if (unary.op() == UnaryOp::Not) {
      const std::optional<bool> inner = constBool(unary.operand());
      if (inner.has_value()) {
        return !*inner;
      }
    }
  }
  return std::nullopt;
}

bool TypeChecker::checkIf(IfStmt& statement, const Type* expectedReturn) {
  bool ok = true;
  bool taken = false;
  for (IfBranch& branch : statement.branches()) {
    if (taken) {
      continue;
    }
    if (branch.condition != nullptr) {
      const Type* type = checkExpr(*branch.condition);
      if (type == nullptr) {
        ok = false;
      } else if (!type->isNamed("bool")) {
        diagnostics_->error(branch.condition->range(),
                            "if/elif condition must be bool, found " + quoteType(type));
        ok = false;
      }
      const std::optional<bool> known = constBool(*branch.condition);
      if (known.has_value() && !*known) {
        continue;
      }
      if (known.has_value() && *known) {
        taken = true;
      }
    } else {
      taken = true;
    }
    pushScope(branch.range);
    for (std::unique_ptr<Stmt>& bodyStmt : branch.body) {
      ok = checkStatement(*bodyStmt, expectedReturn) && ok;
    }
    popScope();
  }
  return ok;
}

bool TypeChecker::checkWhile(WhileStmt& statement, const Type* expectedReturn) {
  const Type* type = checkExpr(statement.condition());
  if (type == nullptr) {
    return false;
  }
  if (!type->isNamed("bool")) {
    diagnostics_->error(statement.condition().range(),
                        "while condition must be bool, found " + quoteType(type));
    return false;
  }
  ++loopDepth_;
  bool ok = true;
  for (std::unique_ptr<Stmt>& bodyStmt : statement.body()) {
    ok = checkStatement(*bodyStmt, expectedReturn) && ok;
  }
  --loopDepth_;
  return ok;
}

const Type* TypeChecker::iterableElementType(Expr& iterable) {
  const Type* type = checkExpr(iterable);
  if (type == nullptr) {
    return nullptr;
  }
  if (type->isSequence()) {
    return type->elementType();
  }
  if (type->isNamed("str")) {
    return types_->strType();
  }
  if (type->isDict()) {
    return type->dictKeyType();
  }
  if (type->methodIndex("__iter__") >= 0) {
    const Type* iter = type->dunderReturn("__iter__");
    if (iter != nullptr && iter->methodIndex("__next__") >= 0) {
      return iter->dunderReturn("__next__");
    }
  }
  if (iterable.kind() == NodeKind::CallExpr &&
      static_cast<const CallExpr&>(iterable).intrinsic() == IntrinsicKind::Range) {
    return types_->i32Type();
  }
  diagnostics_->error(iterable.range(),
                      "for-in requires a list, array, str, dict, range(), or __iter__, found " +
                          quoteType(type));
  return nullptr;
}

const Type* TypeChecker::checkComprehension(ComprehensionExpr& expr) {
  const Type* elementType = iterableElementType(expr.iterable());
  if (elementType == nullptr) {
    return nullptr;
  }
  pushScope(expr.range());
  Symbol symbol;
  symbol.kind = SymbolKind::Variable;
  symbol.type = elementType;
  if (!declare(expr.name(), symbol, expr.range().start)) {
    popScope();
    return nullptr;
  }
  const Type* valueType = checkExpr(expr.element());
  popScope();
  if (valueType == nullptr) {
    return nullptr;
  }
  const Type* result = types_->listType(valueType);
  expr.setResolvedType(result);
  return result;
}

bool TypeChecker::checkFor(ForStmt& statement, const Type* expectedReturn) {
  const Type* element = iterableElementType(statement.iterable());
  if (element == nullptr) {
    return false;
  }
  pushScope(statement.range());
  Symbol symbol;
  symbol.kind = SymbolKind::Variable;
  symbol.type = element;
  if (!declare(statement.name(), symbol, statement.range().start)) {
    popScope();
    return false;
  }
  ++loopDepth_;
  bool ok = true;
  for (std::unique_ptr<Stmt>& bodyStmt : statement.body()) {
    ok = checkStatement(*bodyStmt, expectedReturn) && ok;
  }
  --loopDepth_;
  popScope();
  return ok;
}

bool TypeChecker::checkAssert(AssertStmt& statement) {
  const Type* type = checkExpr(statement.condition());
  if (type == nullptr) {
    return false;
  }
  if (!type->isNamed("bool")) {
    diagnostics_->error(statement.condition().range(),
                        "assert condition must be bool, found " + quoteType(type));
    return false;
  }
  if (statement.message() != nullptr) {
    const Type* messageType = checkExpr(const_cast<Expr&>(*statement.message()));
    if (messageType == nullptr) {
      return false;
    }
    if (!messageType->isNamed("str")) {
      diagnostics_->error(statement.message()->range(), "assert message must be str");
      return false;
    }
  }
  return true;
}

bool TypeChecker::checkBreak(const BreakStmt& statement) {
  if (loopDepth_ <= 0) {
    diagnostics_->error(statement.range(), "'break' outside loop");
    return false;
  }
  return true;
}

bool TypeChecker::checkContinue(const ContinueStmt& statement) {
  if (loopDepth_ <= 0) {
    diagnostics_->error(statement.range(), "'continue' outside loop");
    return false;
  }
  return true;
}

const Type* TypeChecker::checkIntrinsicCall(CallExpr& expr, IntrinsicKind kind) {
  if (kind != IntrinsicKind::Print && !expr.keywordArguments().empty()) {
    diagnostics_->error(expr.range(),
                        std::string(intrinsicName(kind)) + "() does not accept keyword arguments");
    return nullptr;
  }
  std::vector<const Type*> typeArgs;
  for (const std::unique_ptr<TypeExpr>& typeArg : expr.typeArgs()) {
    const Type* resolved = resolveTypeExpr(*typeArg);
    if (resolved == nullptr) {
      return nullptr;
    }
    typeArgs.push_back(resolved);
  }
  std::vector<const Type*> valueTypes;
  const bool isinstanceTwoArg =
      kind == IntrinsicKind::IsInstance && expr.typeArgs().empty() && expr.arguments().size() == 2;
  for (std::size_t argIndex = 0; argIndex < expr.arguments().size(); ++argIndex) {
    if (isinstanceTwoArg && argIndex == 1) {
      const Type* target = resolveTypeFromExpr(*expr.arguments()[argIndex], true);
      if (target == nullptr) {
        return nullptr;
      }
      expr.arguments()[argIndex]->setResolvedType(target);
      valueTypes.push_back(target);
      continue;
    }
    const Type* valueType = checkExpr(*expr.arguments()[argIndex]);
    if (valueType == nullptr) {
      return nullptr;
    }
    valueTypes.push_back(valueType);
  }
  const Type* result = nullptr;
  if (kind == IntrinsicKind::UniqueNew || kind == IntrinsicKind::SharedNew) {
    if (valueTypes.size() != 1) {
      diagnostics_->error(expr.range(),
                          std::string(intrinsicName(kind)) + "() takes one value argument, but " +
                              std::to_string(valueTypes.size()) + " provided");
      return nullptr;
    }
    const Type* pointee = typeArgs.empty() ? valueTypes[0] : typeArgs[0];
    if (!isAssignable(valueTypes[0], pointee)) {
      diagnostics_->error(expr.arguments()[0]->range(),
                          "cannot initialize " + quoteType(pointee) + " with " +
                              quoteType(valueTypes[0]));
      return nullptr;
    }
    result = kind == IntrinsicKind::UniqueNew ? types_->uniqueType(pointee)
                                              : types_->sharedType(pointee);
  } else if (kind == IntrinsicKind::Alloc) {
    if (typeArgs.size() != 1 || !valueTypes.empty() || typeArgs[0]->isVoidLike()) {
      diagnostics_->error(expr.range(), "alloc[T]() requires one non-void type argument");
      return nullptr;
    }
    result = types_->ptrType(typeArgs[0]);
  } else if (kind == IntrinsicKind::Free) {
    if (valueTypes.size() != 1 || !valueTypes[0]->isGenericCtor("Ptr")) {
      diagnostics_->error(expr.range(),
                          "free() requires Ptr[T]; owning pointers are released automatically");
      return nullptr;
    }
    if (expr.arguments()[0]->kind() == NodeKind::UnaryExpr &&
        static_cast<const UnaryExpr&>(*expr.arguments()[0]).op() == UnaryOp::AddrOf) {
      diagnostics_->error(expr.range(), "cannot free an address borrowed with '&'");
      return nullptr;
    }
    result = types_->voidType();
  } else if (kind == IntrinsicKind::Load) {
    if (valueTypes.size() != 1 || !valueTypes[0]->isPointerLike()) {
      diagnostics_->error(expr.range(), "load() requires Unique[T], Shared[T], or Ptr[T]");
      return nullptr;
    }
    result = valueTypes[0]->pointeeType();
    if (result == nullptr || result->isVoidLike()) {
      diagnostics_->error(expr.range(), "load() requires a non-void pointee type");
      return nullptr;
    }
  } else if (kind == IntrinsicKind::Store) {
    if (valueTypes.size() != 2 || !valueTypes[0]->isPointerLike() ||
        valueTypes[0]->pointeeType()->isVoidLike() ||
        !isAssignable(valueTypes[1], valueTypes[0]->pointeeType())) {
      diagnostics_->error(expr.range(),
                          "store() requires a pointer and a value of its pointee type");
      return nullptr;
    }
    result = types_->voidType();
  } else if (kind == IntrinsicKind::Len) {
    if (valueTypes.size() != 1) {
      diagnostics_->error(expr.range(), "len() requires one argument");
      return nullptr;
    }
    if (!(valueTypes[0]->valueType()->isSequence() || valueTypes[0]->valueType()->isDict() ||
          valueTypes[0]->valueType()->isStrLayout() ||
          valueTypes[0]->methodIndex("__len__") >= 0)) {
      diagnostics_->error(expr.range(),
                          "len() requires a list, array, dict, str, regex, or __len__");
      return nullptr;
    }
    result = types_->i64Type();
  } else if (kind == IntrinsicKind::Dir) {
    if (valueTypes.size() > 1 || !typeArgs.empty()) {
      diagnostics_->error(expr.range(), "dir() takes zero or one argument");
      return nullptr;
    }
    if (valueTypes.empty()) {
      std::vector<std::string> names;
      if (!scopes_.empty()) {
        for (const auto& entry : scopes_.front()) {
          names.push_back(entry.first);
        }
      }
      expr.setCompileTimeNames(std::move(names));
    }
    expr.setParamNames(valueTypes.empty() ? std::vector<std::string>{}
                                          : std::vector<std::string>{"value"});
    result = types_->listType(types_->strType());
  } else if (kind == IntrinsicKind::SizeOf || kind == IntrinsicKind::AlignOf) {
    if (typeArgs.size() != 1 || !valueTypes.empty()) {
      diagnostics_->error(expr.range(),
                          std::string(intrinsicName(kind)) + "[T]() takes one type argument");
      return nullptr;
    }
    result = types_->i64Type();
  } else if (kind == IntrinsicKind::Panic) {
    if (valueTypes.size() != 1 || !valueTypes[0]->isNamed("str")) {
      diagnostics_->error(expr.range(), "panic() requires a str message");
      return nullptr;
    }
    result = types_->neverType();
  } else if (kind == IntrinsicKind::Append) {
    if (valueTypes.size() != 2 || !valueTypes[0]->isList() ||
        !isAssignable(valueTypes[1], valueTypes[0]->elementType())) {
      diagnostics_->error(expr.range(), "append() requires a list and an element of its type");
      return nullptr;
    }
    result = types_->voidType();
  } else if (kind == IntrinsicKind::ListNew || kind == IntrinsicKind::ArrayNew) {
    if (typeArgs.size() != 1) {
      diagnostics_->error(expr.range(),
                          std::string(intrinsicName(kind)) + "[T](...) requires one type argument");
      return nullptr;
    }
    for (std::size_t index = 0; index < valueTypes.size(); ++index) {
      if (!isAssignable(valueTypes[index], typeArgs[0])) {
        diagnostics_->error(expr.arguments()[index]->range(),
                            "cannot store " + quoteType(valueTypes[index]) + " in " +
                                quoteType(typeArgs[0]));
        return nullptr;
      }
    }
    result = kind == IntrinsicKind::ListNew ? types_->listType(typeArgs[0])
                                            : types_->arrayType(typeArgs[0]);
  } else if (kind == IntrinsicKind::DictNew) {
    if (typeArgs.size() != 2 || !valueTypes.empty()) {
      diagnostics_->error(expr.range(), "dict[K, V]() takes two type arguments and no values");
      return nullptr;
    }
    result = types_->dictType(typeArgs[0], typeArgs[1]);
  } else if (kind == IntrinsicKind::Range) {
    if (valueTypes.empty() || valueTypes.size() > 3) {
      diagnostics_->error(expr.range(), "range() takes 1 to 3 integer arguments");
      return nullptr;
    }
    for (std::size_t index = 0; index < valueTypes.size(); ++index) {
      if (!valueTypes[index]->isInteger()) {
        diagnostics_->error(expr.arguments()[index]->range(), "range() arguments must be integers");
        return nullptr;
      }
    }
    if (valueTypes.size() == 1) {
      expr.setParamNames({"stop"});
    } else if (valueTypes.size() == 2) {
      expr.setParamNames({"start", "stop"});
    } else {
      expr.setParamNames({"start", "stop", "step"});
    }
    result = types_->listType(types_->i32Type());
  } else if (kind == IntrinsicKind::Print) {
    if (!typeArgs.empty()) {
      diagnostics_->error(expr.range(), "print() does not take type arguments");
      return nullptr;
    }
    for (const NamedArgument& kw : expr.keywordArguments()) {
      if (kw.name != "sep" && kw.name != "end") {
        diagnostics_->error(kw.value->range(),
                            "print() got an unexpected keyword argument '" + kw.name + "'");
        return nullptr;
      }
      const Type* kwType = checkExpr(*kw.value);
      if (kwType == nullptr || !kwType->isNamed("str")) {
        diagnostics_->error(kw.value->range(), "print() keyword '" + kw.name + "' must be a str");
        return nullptr;
      }
    }
    for (std::size_t index = 0; index < valueTypes.size(); ++index) {
      if (!isPrintable(valueTypes[index])) {
        diagnostics_->error(expr.arguments()[index]->range(),
                            "print() cannot print values of type " + quoteType(valueTypes[index]));
        return nullptr;
      }
    }
    result = types_->voidType();
  } else if (kind == IntrinsicKind::Str || kind == IntrinsicKind::Repr) {
    if (valueTypes.size() != 1 || !typeArgs.empty() || !isPrintable(valueTypes[0])) {
      diagnostics_->error(expr.range(), "str() requires one printable argument");
      return nullptr;
    }
    expr.setParamNames({"value"});
    result = types_->strType();
  } else if (kind == IntrinsicKind::TypeOf) {
    if (valueTypes.size() != 1 || !typeArgs.empty()) {
      diagnostics_->error(expr.range(), "typeof() requires one argument");
      return nullptr;
    }
    expr.setParamNames({"value"});
    result = types_->strType();
  } else if (kind == IntrinsicKind::IsInstance) {
    if (!((valueTypes.size() == 1 && typeArgs.size() == 1) ||
          (valueTypes.size() == 2 && typeArgs.empty()))) {
      diagnostics_->error(expr.range(), "isinstance[T](value) or isinstance(value, T) expected");
      return nullptr;
    }
    expr.setParamNames(valueTypes.size() == 1 ? std::vector<std::string>{"value"}
                                              : std::vector<std::string>{"value", "type"});
    result = types_->boolType();
  } else if (kind == IntrinsicKind::Inspect) {
    if (valueTypes.size() != 1 || !typeArgs.empty()) {
      diagnostics_->error(expr.range(), "inspect() requires one argument");
      return nullptr;
    }
    expr.setParamNames({"value"});
    result = types_->strType();
  } else if (kind == IntrinsicKind::Parse || kind == IntrinsicKind::TryParse) {
    if (typeArgs.size() != 1 || valueTypes.size() != 1) {
      diagnostics_->error(expr.range(),
                          std::string(intrinsicName(kind)) +
                              "[T](text) takes one type and one string");
      return nullptr;
    }
    if (!valueTypes[0]->isNamed("str")) {
      diagnostics_->error(expr.arguments()[0]->range(),
                          std::string(intrinsicName(kind)) + "() requires a str");
      return nullptr;
    }
    if (!isParseableType(typeArgs[0])) {
      diagnostics_->error(expr.range(), "cannot parse as " + quoteType(typeArgs[0]));
      diagnostics_->help("parse integers, floats, bool, str, or None");
      return nullptr;
    }
    expr.setParamNames({"text"});
    result = kind == IntrinsicKind::TryParse ? types_->unionType({typeArgs[0], types_->noneType()})
                                             : typeArgs[0];
  }
  expr.setIntrinsic(kind);
  expr.setResolvedType(result);
  return result;
}

const Type* TypeChecker::specializeCall(CallExpr& expr, const Symbol& symbol) {
  const Type* functionType = symbol.type;
  if (functionType == nullptr) {
    return nullptr;
  }
  if (symbol.function == nullptr || symbol.function->typeParams().empty()) {
    return functionType;
  }
  std::unordered_map<std::string, const Type*> subst;
  if (!expr.typeArgs().empty()) {
    if (expr.typeArgs().size() != symbol.function->typeParams().size()) {
      diagnostics_->error(expr.range(),
                          "'" + symbol.function->name() + "' requires " +
                              std::to_string(symbol.function->typeParams().size()) +
                              " type arguments");
      return nullptr;
    }
    for (std::size_t index = 0; index < expr.typeArgs().size(); ++index) {
      const Type* resolved = resolveTypeExpr(*expr.typeArgs()[index]);
      if (resolved == nullptr) {
        return nullptr;
      }
      subst[symbol.function->typeParams()[index]] = resolved;
    }
  } else if (expr.arguments().size() == functionType->paramTypes().size()) {
    for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
      const Type* argType = checkExpr(*expr.arguments()[index]);
      if (argType == nullptr) {
        return nullptr;
      }
      const Type* expected = functionType->paramTypes()[index];
      if (!inferTypeBindings(expected, argType, subst)) {
        diagnostics_->error(expr.arguments()[index]->range(), "conflicting generic argument types");
        return nullptr;
      }
    }
  }
  std::vector<const Type*> instArgs;
  for (const std::string& param : symbol.function->typeParams()) {
    const auto found = subst.find(param);
    if (found == subst.end()) {
      diagnostics_->error(expr.range(), "cannot infer type argument '" + param + "'");
      return nullptr;
    }
    instArgs.push_back(found->second);
  }
  if (!checkTypeConstraints(symbol.function->typeParams(),
                            resolvedConstraints(symbol.function->typeConstraints()),
                            instArgs,
                            expr.range()))
    return nullptr;
  const FunctionInstantiation* inst = types_->instantiateFunction(
      symbol.function->name(), symbol.function->typeParams(), functionType, instArgs);
  if (inst == nullptr || inst->specializedType == nullptr) {
    diagnostics_->error(expr.range(), "failed to instantiate '" + symbol.function->name() + "'");
    return nullptr;
  }
  expr.setLoweredName(inst->llvmName);
  return inst->specializedType;
}

const Type* TypeChecker::resolveSuperType(SourceRange range) {
  if (currentClass_.empty()) {
    diagnostics_->error(range, "super() is only valid in a method");
    return nullptr;
  }
  const Type* record = classType(currentClass_);
  if (record == nullptr || record->bases().empty()) {
    diagnostics_->error(range, "'" + currentClass_ + "' has no base class");
    diagnostics_->help("call super() from a class that inherits");
    return nullptr;
  }
  return record->bases()[0];
}

const Type* TypeChecker::checkSuperCall(CallExpr& expr) {
  if (!expr.typeArgs().empty() || !expr.arguments().empty()) {
    diagnostics_->error(expr.range(), "super() takes no arguments");
    return nullptr;
  }
  const Type* base = resolveSuperType(expr.range());
  if (base == nullptr) {
    return nullptr;
  }
  expr.setIntrinsic(IntrinsicKind::Super);
  expr.callee().setResolvedType(base);
  expr.setResolvedType(base);
  return base;
}

const Type* TypeChecker::checkCall(CallExpr& expr) {
  if (expr.callee().kind() == NodeKind::MemberExpr) {
    return checkMethodCall(expr);
  }
  NameExpr* name = asName(expr.callee());
  if (name == nullptr) {
    const Type* calleeType = checkExpr(expr.callee());
    if (calleeType == nullptr) {
      return nullptr;
    }
    calleeType = calleeType->canonical();
    if (calleeType->isTypeObject()) {
      return checkConstructor(expr, calleeType->typeObjectInstance());
    }
    if (calleeType->kind() == TypeKind::Function) {
      return checkIndirectCall(expr, calleeType);
    }
    if (calleeType->isCallableConstraint()) {
      return checkCallableCall(expr, calleeType);
    }
    diagnostics_->error(expr.range(), "callee is not a function");
    diagnostics_->help("a lambda, function value, or Callable is required here");
    return nullptr;
  }
  if (name->name() == "super") {
    return checkSuperCall(expr);
  }
  Symbol* symbol = lookup(name->name());
  if (symbol != nullptr && symbol->kind == SymbolKind::Intrinsic) {
    return checkIntrinsicCall(expr, symbol->intrinsic);
  }
  if (symbol != nullptr && symbol->kind == SymbolKind::Class) {
    const Type* record = symbol->type;
    if (!expr.typeArgs().empty()) {
      record = resolveNamedType(name->name(), expr.typeArgs(), expr.range(), true);
    } else if (record != nullptr && !record->typeParams().empty()) {
      diagnostics_->error(expr.range(), "'" + name->name() + "' requires type arguments");
      return nullptr;
    }
    return checkConstructor(expr, record);
  }
  if (symbol != nullptr && symbol->type != nullptr && symbol->type->isTypeObject()) {
    return checkConstructor(expr, symbol->type->typeObjectInstance());
  }
  if (name->name() == "Shared" || name->name() == "Unique" || name->name() == "Ptr") {
    const Type* target = resolveNamedType(name->name(), expr.typeArgs(), expr.range(), true);
    if (target == nullptr) {
      return nullptr;
    }
    if (expr.arguments().size() > 1) {
      diagnostics_->error(expr.range(),
                          name->name() + "() takes 0 or 1 argument, but " +
                              std::to_string(expr.arguments().size()) + " provided");
      return nullptr;
    }
    if (expr.arguments().size() == 1) {
      const Type* argType = checkExpr(*expr.arguments()[0]);
      if (argType == nullptr) {
        return nullptr;
      }
      const Type* pointee = target->pointeeType();
      if (pointee != nullptr && !isAssignable(argType, pointee)) {
        diagnostics_->error(expr.arguments()[0]->range(),
                            "cannot initialize " + quoteType(target) + " with " +
                                quoteType(argType));
        return nullptr;
      }
    }
    if (name->name() == "Shared") {
      expr.setIntrinsic(IntrinsicKind::SharedNew);
    } else if (name->name() == "Unique") {
      expr.setIntrinsic(IntrinsicKind::UniqueNew);
    } else {
      expr.setIntrinsic(IntrinsicKind::Alloc);
    }
    expr.callee().setResolvedType(target);
    expr.setResolvedType(target);
    return target;
  }
  if (expr.arguments().size() == 1 && (symbol == nullptr || symbol->kind == SymbolKind::Type)) {
    const Type* target = resolveNamedType(name->name(), expr.typeArgs(), expr.range(), false);
    if (target != nullptr) {
      const Type* result = checkCastValue(*expr.arguments()[0], target, expr.range());
      expr.setCast(true);
      expr.setResolvedType(result);
      return result;
    }
  }
  if (symbol == nullptr || symbol->kind == SymbolKind::Type) {
    const Type* target = resolveNamedType(name->name(), expr.typeArgs(), expr.range(), false);
    if (target != nullptr && target->isClass())
      return checkConstructor(expr, target);
  }
  if (symbol == nullptr || symbol->type == nullptr) {
    if (symbol != nullptr && symbol->kind != SymbolKind::Function) {
      diagnostics_->error(expr.range(), "'" + name->name() + "' is not a function");
      return nullptr;
    }
    reportUnknown(expr.range(), "function", name->name());
    return nullptr;
  }
  if (symbol->kind != SymbolKind::Function) {
    // Resolve the callee expression as well as its symbol so closure capture
    // bookkeeping sees calls through an outer local function value.
    if (checkName(*name) == nullptr) {
      return nullptr;
    }
    const Type* calleeType = symbol->type->canonical();
    if (calleeType->kind() == TypeKind::Function) {
      expr.callee().setResolvedType(calleeType);
      return checkIndirectCall(expr, calleeType);
    }
    if (calleeType->isCallableConstraint()) {
      expr.callee().setResolvedType(calleeType);
      return checkCallableCall(expr, calleeType);
    }
    diagnostics_->error(expr.range(), "'" + name->name() + "' is not a function");
    return nullptr;
  }
  const Type* functionType = specializeCall(expr, *symbol);
  if (functionType == nullptr) {
    return nullptr;
  }
  if (symbol->function != nullptr) {
    if (!validateParamList(symbol->function->params(), symbol->function->range())) {
      return nullptr;
    }
    std::vector<std::string> names;
    for (const ParamDecl& param : symbol->function->params()) {
      names.push_back(param.name);
    }
    expr.setParamNames(std::move(names));
    if (!checkFunctionArguments(
            expr, symbol->function->params(), functionType->paramTypes(), name->name())) {
      return nullptr;
    }
  } else if (!expr.keywordArguments().empty()) {
    diagnostics_->error(expr.range(), "keyword arguments require a known function definition");
    return nullptr;
  } else {
    std::size_t required = functionType->paramTypes().size();
    if (expr.arguments().size() < required ||
        expr.arguments().size() > functionType->paramTypes().size()) {
      diagnostics_->error(
          expr.range(),
          "'" + name->name() + "' takes " +
              countLabel(functionType->paramTypes().size(), "argument", "arguments") + ", but " +
              std::to_string(expr.arguments().size()) + " provided");
      return nullptr;
    }
    for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
      const Type* argType = checkExpr(*expr.arguments()[index]);
      if (argType == nullptr) {
        return nullptr;
      }
      if (!isAssignable(argType, functionType->paramTypes()[index])) {
        diagnostics_->error(expr.arguments()[index]->range(),
                            "argument type mismatch: expected " +
                                quoteType(functionType->paramTypes()[index]) + ", found " +
                                quoteType(argType));
        return nullptr;
      }
      if (!ensureLiteralFits(*expr.arguments()[index], functionType->paramTypes()[index])) {
        return nullptr;
      }
    }
  }
  if (expr.loweredName().empty()) {
    expr.setLoweredName(loweredCallName(*name, *symbol));
  }
  expr.callee().setResolvedType(functionType);
  // Calling an async function yields `Task[T]` where `T` is the declared
  // return type; `await` later unwraps that task back to `T`.
  const Type* callResult = functionType->returnType();
  if (symbol->function != nullptr && symbol->function->isAsync()) {
    callResult = taskType(callResult);
  }
  expr.setResolvedType(callResult);
  return callResult;
}

const Type* TypeChecker::checkConstructor(CallExpr& expr, const Type* record) {
  if (record == nullptr) {
    return nullptr;
  }
  record = record->canonical();
  if (record->isEnum()) {
    if (expr.arguments().size() != 1) {
      diagnostics_->error(expr.range(), "enum '" + record->name() + "' takes one integer value");
      return nullptr;
    }
    const Type* argType = checkExpr(*expr.arguments()[0]);
    if (argType == nullptr || !argType->isInteger()) {
      diagnostics_->error(expr.range(), "enum value must be an integer");
      return nullptr;
    }
    expr.setCast(true);
    expr.setParamNames({"value"});
    expr.setResolvedType(record);
    return record;
  }
  if (record->isAbstract()) {
    diagnostics_->error(expr.range(),
                        "cannot construct abstract class '" + record->display() + "'");
    std::string missing;
    for (const RecordMethod& method : record->methods()) {
      if (!method.isAbstract) {
        continue;
      }
      if (!missing.empty()) {
        missing += ", ";
      }
      missing += method.name;
    }
    if (!missing.empty()) {
      diagnostics_->help("implement abstract method(s): " + missing);
    }
    return nullptr;
  }
  const Type* payload = record->valueType();
  if ((!record->isRecord() || payload != record) && record->methodIndex("__init__") < 0) {
    if (!record->isClass() || expr.arguments().size() > 1 || !expr.keywordArguments().empty()) {
      diagnostics_->error(expr.range(), "built-in constructor takes zero or one value");
      return nullptr;
    }
    if (!expr.arguments().empty()) {
      const Type* argument = checkExpr(*expr.arguments()[0]);
      if (argument == nullptr)
        return nullptr;
      if (!payload->isNamed("str") && !isAssignable(argument, payload) &&
          !canCast(argument->valueType(), payload)) {
        diagnostics_->error(
            expr.range(), "cannot construct " + quoteType(record) + " from " + quoteType(argument));
        return nullptr;
      }
    }
    expr.setConstructor(true);
    expr.setParamNames({"value"});
    expr.setResolvedType(record);
    expr.callee().setResolvedType(types_->typeObject(record));
    return record;
  }
  const int initIndex = record->methodIndex("__init__");
  if (initIndex >= 0) {
    const RecordMethod& init = record->methods()[static_cast<std::size_t>(initIndex)];
    const Type* functionType = init.type;
    if (functionType == nullptr || functionType->paramTypes().empty()) {
      return nullptr;
    }
    FunctionDef* initDef = findMethodDef(record, "__init__");
    if (initDef != nullptr) {
      if (!checkFunctionArguments(
              expr, initDef->params(), functionType->paramTypes(), record->display(), 1)) {
        return nullptr;
      }
    } else if (expr.arguments().size() < init.requiredAfterSelf ||
               expr.arguments().size() + 1 > functionType->paramTypes().size()) {
      diagnostics_->error(
          expr.range(),
          "'" + record->display() + "' takes " +
              countLabel(functionType->paramTypes().size() - 1, "argument", "arguments") +
              ", but " + std::to_string(expr.arguments().size()) + " provided");
      return nullptr;
    } else {
      for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
        const Type* argType = checkExpr(*expr.arguments()[index]);
        if (argType == nullptr) {
          return nullptr;
        }
        if (!isAssignable(argType, functionType->paramTypes()[index + 1])) {
          diagnostics_->error(expr.arguments()[index]->range(),
                              "constructor argument type mismatch: expected " +
                                  quoteType(functionType->paramTypes()[index + 1]) + ", found " +
                                  quoteType(argType));
          return nullptr;
        }
      }
    }
    std::vector<std::string> names;
    for (std::size_t index = 1; index < init.paramNames.size(); ++index) {
      names.push_back(init.paramNames[index]);
    }
    expr.setParamNames(std::move(names));
    expr.setConstructor(true);
    expr.setLoweredName(init.llvmName);
    expr.callee().setResolvedType(record);
    expr.setResolvedType(record);
    return record;
  }
  std::vector<ParamDecl> params;
  std::vector<const Type*> paramTypes;
  std::vector<std::string> names;
  for (const RecordField& field : record->fields()) {
    if (!field.isStatic && field.stored) {
      ParamDecl param;
      param.name = field.name;
      params.push_back(std::move(param));
      paramTypes.push_back(field.type);
      names.push_back(field.name);
    }
  }
  if ((!expr.arguments().empty() || !expr.keywordArguments().empty()) &&
      !checkFunctionArguments(expr, params, paramTypes, record->display(), 0)) {
    return nullptr;
  }
  expr.setParamNames(std::move(names));
  expr.setConstructor(true);
  expr.callee().setResolvedType(record);
  expr.setResolvedType(record);
  return record;
}

const Type*
TypeChecker::checkBuiltinMethod(CallExpr& expr, const Type* objectType, const std::string& name) {
  auto finish =
      [&](const Type* result, std::vector<std::string> params, const char* prefix) -> const Type* {
    expr.setIntrinsic(IntrinsicKind::BuiltinMethod);
    expr.setLoweredName(std::string(prefix) + name);
    expr.setParamNames(std::move(params));
    expr.setResolvedType(result);
    return result;
  };
  auto argCount = [&](std::size_t n, const std::string& msg) -> bool {
    if (expr.arguments().size() != n) {
      diagnostics_->error(expr.range(), msg);
      return false;
    }
    return true;
  };
  auto unknown = [&]() -> const Type* {
    diagnostics_->error(expr.range(), "unknown method '" + name + "' on " + quoteType(objectType));
    return nullptr;
  };
  if (objectType->isList()) {
    const Type* elem = objectType->elementType();
    if (name == "append" || name == "push") {
      if (!argCount(1, name + "() takes one argument") ||
          checkExpr(*expr.arguments()[0]) == nullptr ||
          !isAssignable(expr.arguments()[0]->resolvedType(), elem) ||
          !ensureLiteralFits(*expr.arguments()[0], elem)) {
        return nullptr;
      }
      return finish(types_->voidType(), {"value"}, "list.");
    }
    if (name == "insert") {
      if (!argCount(2, "insert() takes index and value")) {
        return nullptr;
      }
      const Type* indexType = checkExpr(*expr.arguments()[0]);
      const Type* valueType = checkExpr(*expr.arguments()[1]);
      if (indexType == nullptr || valueType == nullptr || !indexType->isInteger() ||
          !isAssignable(valueType, elem) || !ensureLiteralFits(*expr.arguments()[1], elem)) {
        diagnostics_->error(expr.range(), "insert() requires an integer index and a list element");
        return nullptr;
      }
      return finish(types_->voidType(), {"index", "value"}, "list.");
    }
    if (name == "pop") {
      if (expr.arguments().size() > 1) {
        diagnostics_->error(expr.range(), "pop() takes at most one index");
        return nullptr;
      }
      if (!expr.arguments().empty()) {
        const Type* indexType = checkExpr(*expr.arguments()[0]);
        if (indexType == nullptr || !indexType->isInteger()) {
          diagnostics_->error(expr.range(), "pop() index must be an integer");
          return nullptr;
        }
        return finish(elem, {"index"}, "list.");
      }
      return finish(elem, {}, "list.");
    }
    if (name == "remove") {
      if (!argCount(1, "remove() takes one value") || checkExpr(*expr.arguments()[0]) == nullptr ||
          !isAssignable(expr.arguments()[0]->resolvedType(), elem)) {
        return nullptr;
      }
      return finish(types_->boolType(), {"value"}, "list.");
    }
    if (name == "find" || name == "index" || name == "count") {
      if (!argCount(1, name + "() takes one value") || checkExpr(*expr.arguments()[0]) == nullptr ||
          !isAssignable(expr.arguments()[0]->resolvedType(), elem)) {
        return nullptr;
      }
      return finish(types_->i64Type(), {"value"}, "list.");
    }
    if (name == "contains" || name == "has") {
      if (!argCount(1, name + "() takes one value") || checkExpr(*expr.arguments()[0]) == nullptr) {
        return nullptr;
      }
      return finish(types_->boolType(), {"value"}, "list.");
    }
    if (name == "clear" || name == "reverse") {
      if (!argCount(0, name + "() takes no arguments")) {
        return nullptr;
      }
      return finish(types_->voidType(), {}, "list.");
    }
    if (name == "copy" || name == "clone") {
      if (!argCount(0, "copy() takes no arguments")) {
        return nullptr;
      }
      return finish(objectType, {}, "list.");
    }
    if (name == "extend") {
      if (!argCount(1, "extend() takes one list") || checkExpr(*expr.arguments()[0]) == nullptr ||
          !isAssignable(expr.arguments()[0]->resolvedType(), objectType)) {
        diagnostics_->error(expr.range(), "extend() requires a list of the same element type");
        return nullptr;
      }
      return finish(types_->voidType(), {"items"}, "list.");
    }
    return unknown();
  }
  if (objectType->isDict()) {
    const Type* key = objectType->dictKeyType();
    const Type* value = objectType->dictValueType();
    if (name == "get" || name == "pop") {
      if (!argCount(1, name + "() takes one key") || checkExpr(*expr.arguments()[0]) == nullptr ||
          !isAssignable(expr.arguments()[0]->resolvedType(), key)) {
        return nullptr;
      }
      return finish(value, {"key"}, "dict.");
    }
    if (name == "set") {
      if (!argCount(2, "set() takes a key and a value")) {
        return nullptr;
      }
      if (checkExpr(*expr.arguments()[0]) == nullptr ||
          checkExpr(*expr.arguments()[1]) == nullptr ||
          !isAssignable(expr.arguments()[0]->resolvedType(), key) ||
          !isAssignable(expr.arguments()[1]->resolvedType(), value)) {
        return nullptr;
      }
      return finish(types_->voidType(), {"key", "value"}, "dict.");
    }
    if (name == "remove" || name == "delete" || name == "contains" || name == "has") {
      if (!argCount(1, name + "() takes one key") || checkExpr(*expr.arguments()[0]) == nullptr ||
          !isAssignable(expr.arguments()[0]->resolvedType(), key)) {
        return nullptr;
      }
      return finish(types_->boolType(), {"key"}, "dict.");
    }
    if (name == "keys") {
      if (!argCount(0, "keys() takes no arguments")) {
        return nullptr;
      }
      return finish(types_->listType(key), {}, "dict.");
    }
    if (name == "values") {
      if (!argCount(0, "values() takes no arguments")) {
        return nullptr;
      }
      return finish(types_->listType(value), {}, "dict.");
    }
    if (name == "clear") {
      if (!argCount(0, "clear() takes no arguments")) {
        return nullptr;
      }
      return finish(types_->voidType(), {}, "dict.");
    }
    if (name == "copy" || name == "clone") {
      if (!argCount(0, "copy() takes no arguments")) {
        return nullptr;
      }
      return finish(objectType, {}, "dict.");
    }
    return unknown();
  }
  if (objectType->isStrLayout()) {
    const Type* str = types_->strType();
    auto strArg = [&](std::size_t index) -> bool {
      const Type* type = checkExpr(*expr.arguments()[index]);
      return type != nullptr && isAssignable(type, str);
    };
    if (name == "upper" || name == "lower" || name == "strip" || name == "lstrip" ||
        name == "rstrip" || name == "capitalize" || name == "title") {
      if (!argCount(0, name + "() takes no arguments")) {
        return nullptr;
      }
      return finish(str, {}, "str.");
    }
    if (name == "starts_with" || name == "endswith" || name == "ends_with" ||
        name == "startswith" || name == "contains" || name == "has") {
      if (!argCount(1, name + "() takes one string") || !strArg(0)) {
        return nullptr;
      }
      return finish(types_->boolType(), {"text"}, "str.");
    }
    if (name == "find" || name == "rfind" || name == "count") {
      if (!argCount(1, name + "() takes one string") || !strArg(0)) {
        return nullptr;
      }
      return finish(types_->i64Type(), {"text"}, "str.");
    }
    if (name == "replace") {
      if (!argCount(2, "replace() takes old and new strings") || !strArg(0) || !strArg(1)) {
        return nullptr;
      }
      return finish(str, {"old", "new"}, "str.");
    }
    if (name == "split") {
      if (!argCount(1, "split() takes a separator") || !strArg(0)) {
        return nullptr;
      }
      return finish(types_->listType(str), {"sep"}, "str.");
    }
    if (name == "join") {
      if (!argCount(1, "join() takes a list of strings")) {
        return nullptr;
      }
      const Type* parts = checkExpr(*expr.arguments()[0]);
      if (parts == nullptr || !parts->isList() || !isAssignable(parts->elementType(), str)) {
        diagnostics_->error(expr.range(), "join() requires list[str]");
        return nullptr;
      }
      return finish(str, {"parts"}, "str.");
    }
    if (name == "repeat") {
      if (!argCount(1, "repeat() takes a count")) {
        return nullptr;
      }
      const Type* count = checkExpr(*expr.arguments()[0]);
      if (count == nullptr || !count->isInteger()) {
        diagnostics_->error(expr.range(), "repeat() count must be an integer");
        return nullptr;
      }
      return finish(str, {"count"}, "str.");
    }
    if (name == "is_empty" || name == "is_digit" || name == "is_alpha" || name == "is_space") {
      if (!argCount(0, name + "() takes no arguments")) {
        return nullptr;
      }
      return finish(types_->boolType(), {}, "str.");
    }
    return unknown();
  }
  return nullptr;
}

const Type* TypeChecker::checkMethodCall(CallExpr& expr) {
  auto& member = static_cast<MemberExpr&>(expr.callee());
  const Type* objectType = checkExpr(member.object());
  if (objectType != nullptr) {
    objectType = objectType->canonical();
    if (objectType->isTypeObject()) {
      const Type* instance = objectType->typeObjectInstance();
      if (instance != nullptr) {
        objectType = instance->canonical();
      }
    }
  }
  if (objectType != nullptr && objectType->isModule()) {
    const RecordField* exported = objectType->findField(member.field());
    if (exported != nullptr && !exported->isPublic) {
      diagnostics_->error(expr.range(), "'" + member.field() + "' is private and is not exported");
      return nullptr;
    }
    const Type* exportedRecord = exported == nullptr ? nullptr : unwrapRecordType(exported->type);
    if (exportedRecord != nullptr) {
      if (!exportedRecord->typeParams().empty()) {
        const NameExpr* moduleName = asName(member.object());
        if (moduleName == nullptr || expr.typeArgs().empty()) {
          diagnostics_->error(expr.range(), "'" + member.field() + "' requires type arguments");
          return nullptr;
        }
        exportedRecord = resolveNamedType(
            moduleName->name() + "." + member.field(), expr.typeArgs(), expr.range(), true);
      }
      return checkConstructor(expr, exportedRecord);
    }
    if (exported == nullptr || exported->type == nullptr ||
        exported->type->kind() != TypeKind::Function) {
      diagnostics_->error(expr.range(), "unknown export '" + member.field() + "'");
      return nullptr;
    }
    const Type* functionType = exported->type;
    if (const NameExpr* moduleName = asName(member.object())) {
      if (const Symbol* symbol = lookup(moduleName->name() + "." + member.field());
          symbol != nullptr && symbol->function != nullptr) {
        functionType = specializeCall(expr, *symbol);
        if (functionType == nullptr)
          return nullptr;
      }
    }
    if (expr.arguments().size() < exported->requiredArgs ||
        expr.arguments().size() > functionType->paramTypes().size()) {
      diagnostics_->error(expr.range(), "argument count mismatch");
      return nullptr;
    }
    for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
      const Type* argType = checkExpr(*expr.arguments()[index]);
      if (argType == nullptr || !isAssignable(argType, functionType->paramTypes()[index])) {
        diagnostics_->error(expr.range(), "argument type mismatch");
        return nullptr;
      }
    }
    if (expr.loweredName().empty()) {
      expr.setLoweredName(exported->llvmName.empty() ? member.field() : exported->llvmName);
    }
    expr.setParamNames(exported->paramNames);
    member.setResolvedType(functionType);
    expr.setResolvedType(functionType->returnType());
    return functionType->returnType();
  }
  if (objectType != nullptr && objectType->valueType() != objectType &&
      objectType->methodIndex(member.field()) < 0) {
    return checkBuiltinMethod(expr, objectType->valueType(), member.field());
  }
  if (objectType != nullptr &&
      (objectType->isList() || objectType->isDict() || objectType->isStrLayout())) {
    return checkBuiltinMethod(expr, objectType, member.field());
  }
  if (objectType != nullptr && objectType->isEnum() && isClassName(member.object())) {
    if (member.field() == "variants") {
      if (!expr.arguments().empty()) {
        diagnostics_->error(expr.range(), "variants() takes no arguments");
        return nullptr;
      }
      std::vector<std::string> names;
      for (const RecordField& field : objectType->fields()) {
        if (field.isStatic) {
          names.push_back(field.name);
        }
      }
      expr.setCompileTimeNames(std::move(names));
      expr.setResolvedType(types_->listType(types_->strType()));
      return expr.resolvedType();
    }
    const RecordField* variant = objectType->findField(member.field());
    if (variant != nullptr && variant->isStatic) {
      if (expr.arguments().size() != variant->payloadTypes.size()) {
        diagnostics_->error(expr.range(),
                            "variant '" + member.field() + "' takes " +
                                std::to_string(variant->payloadTypes.size()) +
                                " payload argument(s)");
        return nullptr;
      }
      std::vector<const Type*> argumentTypes;
      argumentTypes.reserve(expr.arguments().size());
      for (const std::unique_ptr<Expr>& argument : expr.arguments()) {
        const Type* argType = checkExpr(*argument);
        if (argType == nullptr) {
          return nullptr;
        }
        argumentTypes.push_back(argType);
      }
      if (!objectType->typeParams().empty()) {
        if (!expr.typeArgs().empty()) {
          const auto* enumName = asName(member.object());
          objectType =
              enumName == nullptr
                  ? nullptr
                  : resolveNamedType(enumName->name(), expr.typeArgs(), expr.range(), true);
        } else {
          std::unordered_map<std::string, const Type*> bindings;
          for (std::size_t index = 0; index < argumentTypes.size(); ++index) {
            if (!inferTypeBindings(variant->payloadTypes[index], argumentTypes[index], bindings)) {
              diagnostics_->error(expr.arguments()[index]->range(),
                                  "conflicting generic enum payload types");
              return nullptr;
            }
          }
          std::vector<const Type*> inferred;
          for (const std::string& param : objectType->typeParams()) {
            const auto found = bindings.find(param);
            if (found == bindings.end()) {
              diagnostics_->error(expr.range(),
                                  "cannot infer type argument '" + param + "' for enum '" +
                                      objectType->name() + "'");
              diagnostics_->help("provide it explicitly, like " + objectType->name() + "." +
                                 member.field() + "[i32](...)");
              return nullptr;
            }
            inferred.push_back(found->second);
          }
          if (!ensureRecordConstraints(objectType) ||
              !checkTypeConstraints(
                  objectType->typeParams(), objectType->typeConstraints(), inferred, expr.range()))
            return nullptr;
          objectType = types_->instantiate(objectType, inferred);
        }
        if (objectType == nullptr) {
          return nullptr;
        }
        variant = objectType->findField(member.field());
      } else if (!expr.typeArgs().empty()) {
        diagnostics_->error(expr.range(), "enum '" + objectType->name() + "' is not generic");
        return nullptr;
      }
      if (variant == nullptr) {
        return nullptr;
      }
      for (std::size_t index = 0; index < argumentTypes.size(); ++index) {
        const Type* argType = argumentTypes[index];
        if (argType == nullptr || (variant->payloadTypes[index] != nullptr &&
                                   !isAssignable(argType, variant->payloadTypes[index]))) {
          diagnostics_->error(expr.arguments()[index]->range(), "payload type mismatch");
          return nullptr;
        }
      }
      expr.setConstructor(true);
      expr.setParamNames(variant->paramNames);
      expr.setResolvedType(objectType);
      member.setResolvedType(objectType);
      return objectType;
    }
  }
  if (objectType == nullptr || !objectType->isRecord()) {
    diagnostics_->error(expr.range(), "method call requires a class value");
    return nullptr;
  }
  objectType = objectType->canonical();
  if (const RecordField* field = objectType->findField(member.field());
      field != nullptr && field->type != nullptr && field->type->isTypeObject()) {
    member.setResolvedType(field->type);
    return checkConstructor(expr, field->type->typeObjectInstance());
  }
  const int index = objectType->methodIndex(member.field());
  if (index < 0) {
    const RecordField* callableField = objectType->findField(member.field());
    if (callableField != nullptr && callableField->type != nullptr) {
      const Type* fn = callableField->type->canonical();
      if (fn->kind() == TypeKind::Function) {
        member.setResolvedType(fn);
        return checkIndirectCall(expr, fn);
      }
      if (fn->isCallableConstraint()) {
        member.setResolvedType(fn);
        return checkCallableCall(expr, fn);
      }
    }
    reportUnknownMember(*diagnostics_, expr.range(), objectType, member.field(), true);
    return nullptr;
  }
  const RecordMethod& method = objectType->methods()[static_cast<std::size_t>(index)];
  if (!method.isPublic && !canAccessPrivate(objectType)) {
    diagnostics_->error(
        expr.range(), "method '" + member.field() + "' of '" + objectType->name() + "' is private");
    return nullptr;
  }
  const Type* functionType = method.type;
  std::vector<std::string> methodTypeParams = method.typeParams;
  FunctionDef* methodDef = findMethodDef(objectType, member.field());
  if (methodTypeParams.empty() && methodDef != nullptr && !methodDef->typeParams().empty()) {
    methodTypeParams = methodDef->typeParams();
  }
  if (!methodTypeParams.empty()) {
    std::unordered_map<std::string, const Type*> subst;
    if (!expr.typeArgs().empty()) {
      if (expr.typeArgs().size() != methodTypeParams.size()) {
        diagnostics_->error(expr.range(),
                            "'" + member.field() + "' requires " +
                                std::to_string(methodTypeParams.size()) + " type arguments");
        return nullptr;
      }
      for (std::size_t i = 0; i < expr.typeArgs().size(); ++i) {
        const Type* resolved = resolveTypeExpr(*expr.typeArgs()[i]);
        if (resolved == nullptr) {
          return nullptr;
        }
        subst[methodTypeParams[i]] = resolved;
      }
    } else if (functionType != nullptr) {
      for (std::size_t argIdx = 0;
           argIdx < expr.arguments().size() && (argIdx + 1) < functionType->paramTypes().size();
           ++argIdx) {
        const Type* argType = checkExpr(*expr.arguments()[argIdx]);
        if (argType == nullptr) {
          return nullptr;
        }
        (void)inferTypeBindings(functionType->paramTypes()[argIdx + 1], argType, subst);
      }
    }
    std::vector<const Type*> instArgs;
    for (const std::string& param : methodTypeParams) {
      const auto found = subst.find(param);
      if (found == subst.end()) {
        diagnostics_->error(expr.range(),
                            "cannot infer type argument '" + param + "' for method '" +
                                member.field() + "'");
        return nullptr;
      }
      instArgs.push_back(found->second);
    }
    if (!checkTypeConstraints(methodTypeParams, method.typeConstraints, instArgs, expr.range()))
      return nullptr;
    const FunctionInstantiation* inst = types_->instantiateFunction(
        method.llvmName, methodTypeParams, functionType, instArgs, /*isMethod=*/true);
    if (inst == nullptr || inst->specializedType == nullptr) {
      diagnostics_->error(expr.range(), "failed to instantiate method '" + member.field() + "'");
      return nullptr;
    }
    expr.setLoweredName(inst->llvmName);
    functionType = inst->specializedType;
  }
  const bool superObject =
      member.object().kind() == NodeKind::CallExpr &&
      static_cast<const CallExpr&>(member.object()).intrinsic() == IntrinsicKind::Super;
  const bool classLevel =
      !superObject && (isClassName(member.object()) ||
                       (member.object().resolvedType() != nullptr &&
                        member.object().resolvedType()->canonical()->isTypeObject()));
  if (classLevel) {
    expr.setUnboundMethodCall(true);
    if (methodTypeParams.empty()) {
      expr.setLoweredName(method.llvmName);
    }
    std::vector<std::string> names;
    for (std::size_t nameIndex = 1; nameIndex < method.paramNames.size(); ++nameIndex) {
      names.push_back(method.paramNames[nameIndex]);
    }
    expr.setParamNames(std::move(names));
    if (functionType == nullptr || functionType->paramTypes().empty()) {
      return nullptr;
    }
    if (methodDef != nullptr) {
      if (!checkFunctionArguments(
              expr, methodDef->params(), functionType->paramTypes(), member.field(), 1)) {
        return nullptr;
      }
    } else if (expr.arguments().size() < method.requiredAfterSelf ||
               expr.arguments().size() + 1 > functionType->paramTypes().size()) {
      diagnostics_->error(
          expr.range(),
          "'" + member.field() + "' takes " +
              countLabel(functionType->paramTypes().size() - 1, "argument", "arguments") +
              ", but " + std::to_string(expr.arguments().size()) + " provided");
      return nullptr;
    }
    std::vector<const Type*> withoutSelf;
    for (std::size_t paramIndex = 1; paramIndex < functionType->paramTypes().size(); ++paramIndex) {
      withoutSelf.push_back(functionType->paramTypes()[paramIndex]);
    }
    member.setUnboundMethod(expr.loweredName().empty() ? method.llvmName : expr.loweredName());
    member.setResolvedType(types_->functionType(withoutSelf, functionType->returnType()));
    expr.setResolvedType(functionType->returnType());
    return functionType->returnType();
  }
  expr.setMethod(true);
  if (methodTypeParams.empty()) {
    expr.setLoweredName(method.llvmName);
  }
  std::vector<std::string> names;
  for (std::size_t index = 1; index < method.paramNames.size(); ++index) {
    names.push_back(method.paramNames[index]);
  }
  expr.setParamNames(std::move(names));
  if (functionType == nullptr || functionType->paramTypes().empty()) {
    return nullptr;
  }
  if (methodDef != nullptr) {
    if (!checkFunctionArguments(
            expr, methodDef->params(), functionType->paramTypes(), member.field(), 1)) {
      return nullptr;
    }
  } else if (expr.arguments().size() < method.requiredAfterSelf ||
             expr.arguments().size() + 1 > functionType->paramTypes().size()) {
    diagnostics_->error(
        expr.range(),
        "'" + member.field() + "' takes " +
            countLabel(functionType->paramTypes().size() - 1, "argument", "arguments") + ", but " +
            std::to_string(expr.arguments().size()) + " provided");
    return nullptr;
  } else {
    for (std::size_t argIndex = 0; argIndex < expr.arguments().size(); ++argIndex) {
      const Type* argType = checkExpr(*expr.arguments()[argIndex]);
      if (argType == nullptr) {
        return nullptr;
      }
      if (!isAssignable(argType, functionType->paramTypes()[argIndex + 1])) {
        diagnostics_->error(expr.arguments()[argIndex]->range(),
                            "argument type mismatch: expected " +
                                quoteType(functionType->paramTypes()[argIndex + 1]) + ", found " +
                                quoteType(argType));
        return nullptr;
      }
    }
  }
  member.setResolvedType(functionType);
  expr.setResolvedType(functionType->returnType());
  return functionType->returnType();
}

const Type* TypeChecker::checkExpr(Expr& expr) {
  switch (expr.kind()) {
  case NodeKind::IntegerLiteral: {
    const auto& literal = static_cast<const IntegerLiteral&>(expr);
    const Type* type = literal.isByte() ? types_->i8Type() : types_->i32Type();
    expr.setResolvedType(type);
    return type;
  }
  case NodeKind::FloatLiteral: {
    const auto& literal = static_cast<const FloatLiteral&>(expr);
    const Type* type = literal.isF32() ? types_->f32Type() : types_->f64Type();
    expr.setResolvedType(type);
    return type;
  }
  case NodeKind::StringLiteral: {
    const auto& literal = static_cast<const StringLiteral&>(expr);
    const Type* type = literal.isRegex() ? types_->regexType() : types_->strType();
    expr.setResolvedType(type);
    return type;
  }
  case NodeKind::InterpolatedStringExpr:
    return checkInterpolated(static_cast<InterpolatedStringExpr&>(expr));
  case NodeKind::BooleanLiteral:
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  case NodeKind::NoneLiteral:
    expr.setResolvedType(types_->noneType());
    return types_->noneType();
  case NodeKind::NameExpr:
    return checkName(static_cast<NameExpr&>(expr));
  case NodeKind::CallExpr:
    return checkCall(static_cast<CallExpr&>(expr));
  case NodeKind::MemberExpr:
    return checkMember(static_cast<MemberExpr&>(expr));
  case NodeKind::BinaryExpr:
    return checkBinary(static_cast<BinaryExpr&>(expr));
  case NodeKind::UnaryExpr:
    return checkUnary(static_cast<UnaryExpr&>(expr));
  case NodeKind::AwaitExpr:
    return checkAwait(static_cast<AwaitExpr&>(expr));
  case NodeKind::CastExpr:
    return checkCast(static_cast<CastExpr&>(expr));
  case NodeKind::IndexExpr:
    return checkIndex(static_cast<IndexExpr&>(expr));
  case NodeKind::ListLiteral:
    return checkListLiteral(static_cast<ListLiteral&>(expr));
  case NodeKind::DictLiteral:
    return checkDictLiteral(static_cast<DictLiteral&>(expr));
  case NodeKind::ComprehensionExpr:
    return checkComprehension(static_cast<ComprehensionExpr&>(expr));
  case NodeKind::TernaryExpr:
    return checkTernary(static_cast<TernaryExpr&>(expr));
  case NodeKind::TupleExpr:
    return checkTuple(static_cast<TupleExpr&>(expr));
  case NodeKind::WalrusExpr:
    return checkWalrus(static_cast<WalrusExpr&>(expr));
  case NodeKind::LambdaExpr:
    return checkLambda(static_cast<LambdaExpr&>(expr));
  case NodeKind::MacroInvokeExpr:
    diagnostics_->error(expr.range(), "macro was not expanded");
    return nullptr;
  default:
    diagnostics_->error(expr.range(), "unsupported expression");
    return nullptr;
  }
}

const Type* TypeChecker::checkInterpolated(InterpolatedStringExpr& expr) {
  for (StringPart& part : expr.parts()) {
    if (part.value == nullptr) {
      continue;
    }
    const Type* partType = checkExpr(*part.value);
    if (partType == nullptr) {
      return nullptr;
    }
    if (!isPrintable(partType)) {
      diagnostics_->error(part.value->range(),
                          "f-string interpolation cannot print " + quoteType(partType));
      return nullptr;
    }
  }
  expr.setResolvedType(types_->strType());
  return types_->strType();
}

bool TypeChecker::checkVarDecl(VarDecl& decl) {
  const Type* type = nullptr;
  if (decl.hasType()) {
    type = resolveTypeExpr(decl.type());
    if (type == nullptr) {
      return false;
    }
  }
  if (decl.init() != nullptr) {
    Expr& init = const_cast<Expr&>(*decl.init());
    if (type != nullptr) {
      const bool collection = (init.kind() == NodeKind::ListLiteral && type->isSequence()) ||
                              (init.kind() == NodeKind::DictLiteral && type->isDict());
      if (collection) {
        if (!bindCollectionInit(init, type)) {
          return false;
        }
      } else {
        const Type* initType = checkExpr(init);
        if (initType == nullptr) {
          return false;
        }
        if (!isAssignable(initType, type) &&
            !(decl.init()->kind() == NodeKind::NoneLiteral && type->isPointerLike())) {
          diagnostics_->error(decl.init()->range(),
                              "cannot initialize '" + decl.name() + "' with " +
                                  quoteType(initType) + ", expected " + quoteType(type));
          return false;
        }
        if (!ensureLiteralFits(init, type)) {
          return false;
        }
      }
    } else {
      type = checkExpr(init);
      if (type == nullptr) {
        return false;
      }
      if (isVoidLike(type)) {
        diagnostics_->error(decl.range(), "cannot infer a type for '" + decl.name() + "'");
        return false;
      }
    }
  } else if (type == nullptr) {
    diagnostics_->error(decl.range(), "declaration '" + decl.name() + "' needs a type or a value");
    diagnostics_->help("write `name: i32` or `name = 0`");
    return false;
  }
  decl.setResolvedType(type);
  Symbol symbol;
  symbol.kind = SymbolKind::Variable;
  symbol.type = type;
  symbol.readonly = decl.isConst();
  symbol.staticStorage = decl.isStatic();
  return declare(decl.name(), symbol, decl.range().start);
}

bool TypeChecker::bindCallableAlias(AssignStmt& statement, NameExpr& target, const Symbol& source) {
  auto& current = scopes_.back();
  const auto existing = current.find(target.name());
  if (existing != current.end() && !isCallableSymbol(existing->second)) {
    diagnostics_->error(statement.range(), "cannot alias a function onto '" + target.name() + "'");
    return false;
  }
  statement.setNameAlias(true);
  target.setResolvedType(source.type);
  const_cast<Expr&>(statement.value()).setResolvedType(source.type);
  if (existing != current.end()) {
    existing->second = source;
    return true;
  }
  return declare(target.name(), source, statement.range().start);
}

bool TypeChecker::canAccessPrivate(const Type* owner) const {
  const Type* current = classType(currentClass_);
  if (current == nullptr || owner == nullptr || current->qualifier() != owner->qualifier()) {
    return false;
  }
  return current->name() == owner->name() ||
         current->name().starts_with(owner->name() + ".");
}

Symbol* TypeChecker::lookupAssignment(const std::string& name) {
  if (nestedFunction_ == nullptr) {
    return lookup(name);
  }
  // Assignment creates a local in this function, never an implicit nonlocal write.
  for (std::size_t index = scopes_.size(); index > nestedOuterScope_ + 1; --index) {
    auto found = scopes_[index - 1].find(name);
    if (found != scopes_[index - 1].end()) {
      return &found->second;
    }
  }
  return nullptr;
}

bool TypeChecker::checkAssign(AssignStmt& statement) {
  Expr& targetExpr = const_cast<Expr&>(statement.target());
  if (statement.op() == AssignOp::Assign && targetExpr.kind() == NodeKind::TupleExpr) {
    auto& targets = static_cast<TupleExpr&>(targetExpr);
    const Type* valueType = checkExpr(const_cast<Expr&>(statement.value()));
    if (valueType == nullptr) {
      return false;
    }
    if (!valueType->isGenericCtor("tuple") ||
        valueType->args().size() != targets.elements().size()) {
      diagnostics_->error(statement.range(),
                          "tuple unpack expected " + std::to_string(targets.elements().size()) +
                              " values");
      diagnostics_->help("right-hand side must be a tuple of the same length");
      return false;
    }
    bool ok = true;
    for (std::size_t index = 0; index < targets.elements().size(); ++index) {
      Expr& item = *targets.elements()[index];
      const Type* elemType = valueType->args()[index];
      if (item.kind() != NodeKind::NameExpr) {
        diagnostics_->error(item.range(), "tuple unpack target must be a name");
        ok = false;
        continue;
      }
      auto& name = static_cast<NameExpr&>(item);
      Symbol* existing = lookupAssignment(name.name());
      if (existing != nullptr) {
        if (existing->readonly) {
          diagnostics_->error(item.range(), "cannot assign to '" + name.name() + "'");
          ok = false;
          continue;
        }
        name.setResolvedType(existing->type);
        if (elemType != nullptr && existing->type != nullptr &&
            !isAssignable(elemType, existing->type)) {
          diagnostics_->error(
              item.range(), "cannot unpack " + quoteType(elemType) + " into '" + name.name() + "'");
          ok = false;
        }
      } else {
        ok = declareInferred(name, elemType, statement.range().start) && ok;
      }
    }
    statement.setResolvedType(valueType);
    return ok;
  }
  if (statement.op() == AssignOp::Assign) {
    NameExpr* target = asName(targetExpr);
    const NameExpr* value = asName(statement.value());
    if (target != nullptr && value != nullptr) {
      Symbol* source = lookup(value->name());
      if (source != nullptr && isCallableSymbol(*source)) {
        return bindCallableAlias(statement, *target, *source);
      }
    }
  }
  if (!isAssignableTarget(targetExpr)) {
    diagnostics_->error(statement.target().range(), "expression is not assignable");
    return false;
  }
  if (targetExpr.kind() == NodeKind::NameExpr) {
    auto& name = static_cast<NameExpr&>(targetExpr);
    Symbol* symbol = lookupAssignment(name.name());
    if (symbol != nullptr && symbol->readonly) {
      diagnostics_->error(statement.range(),
                          "cannot assign to module constant '" + name.name() + "'");
      return false;
    }
    if (symbol == nullptr) {
      if (statement.op() != AssignOp::Assign) {
        reportUnknown(name.range(), "name", name.name());
        return false;
      }
      const Type* value = checkExpr(const_cast<Expr&>(statement.value()));
      if (value == nullptr) {
        return false;
      }
      return declareInferred(name, value, statement.range().start);
    }
  }
  if (targetExpr.kind() == NodeKind::MemberExpr) {
    auto& member = static_cast<MemberExpr&>(targetExpr);
    const Type* objectType = checkExpr(member.object());
    if (objectType != nullptr) {
      objectType = objectType->canonical();
      const RecordField* field = objectType->findField(member.field());
      const bool ownAccessor =
          currentClass_ == objectType->name() && currentPropertyName_ == member.field();
      const bool initBacking = currentClass_ == objectType->name() &&
                               currentFunctionName_ == "__init__" && field != nullptr &&
                               field->stored;
      if (field != nullptr && !field->setterLlvm.empty() && !ownAccessor && !initBacking) {
        if (!field->setterPublic && !canAccessPrivate(objectType)) {
          diagnostics_->error(statement.range(),
                              "setter '" + member.field() + "' of '" + objectType->name() +
                                  "' is private");
          return false;
        }
        if (statement.op() != AssignOp::Assign && field->getterLlvm.empty()) {
          diagnostics_->error(statement.range(), "property '" + member.field() + "' is write-only");
          return false;
        }
        member.setPropertySet(field->setterLlvm);
        member.setBackingField(false);
      } else if (field != nullptr && field->setterLlvm.empty() && !field->getterLlvm.empty() &&
                 !ownAccessor && !initBacking) {
        diagnostics_->error(statement.range(), "property '" + member.field() + "' is read-only");
        return false;
      }
      if (objectType->isFrozen() && currentFunctionName_ != "__init__" &&
          member.propertySet().empty()) {
        diagnostics_->error(statement.range(),
                            "cannot assign to frozen field '" + member.field() + "'");
        return false;
      }
    }
  }
  const Type* target = checkExpr(targetExpr);
  if (target == nullptr) {
    return false;
  }
  Expr& valueExpr = const_cast<Expr&>(statement.value());
  const bool collection = (valueExpr.kind() == NodeKind::ListLiteral && target->isSequence()) ||
                          (valueExpr.kind() == NodeKind::DictLiteral && target->isDict());
  if (collection) {
    return bindCollectionInit(valueExpr, target);
  }
  const Type* value = checkExpr(valueExpr);
  if (value == nullptr) {
    return false;
  }
  if (statement.op() != AssignOp::Assign) {
    if (!(target->isInteger() || target->isFloat()) || !(value->isInteger() || value->isFloat())) {
      diagnostics_->error(statement.range(), "compound assignment requires numeric operands");
      return false;
    }
    return true;
  }
  if (!isAssignable(value, target) &&
      !(statement.value().kind() == NodeKind::NoneLiteral && target->isPointerLike())) {
    diagnostics_->error(statement.value().range(),
                        "cannot assign " + quoteType(value) + " to " + quoteType(target));
    return false;
  }
  return ensureLiteralFits(valueExpr, target);
}

bool TypeChecker::checkReturn(ReturnStmt& statement, const Type* expectedReturn) {
  if (expectedReturn == nullptr) {
    return false;
  }
  if (statement.value() == nullptr) {
    if (!expectedReturn->isVoidLike()) {
      diagnostics_->error(statement.range(),
                          "missing return value, expected " + quoteType(expectedReturn));
      return false;
    }
    return true;
  }
  Expr& value = const_cast<Expr&>(*statement.value());
  if ((value.kind() == NodeKind::ListLiteral && expectedReturn->isSequence()) ||
      (value.kind() == NodeKind::DictLiteral && expectedReturn->isDict())) {
    return bindCollectionInit(value, expectedReturn);
  }
  const Type* actual = checkExpr(value);
  if (actual == nullptr) {
    return false;
  }
  if (inferredNestedReturns_ != nullptr) {
    inferredNestedReturns_->push_back(actual);
  }
  if (value.kind() == NodeKind::UnaryExpr &&
      static_cast<const UnaryExpr&>(value).op() == UnaryOp::AddrOf) {
    const Expr* target = &static_cast<const UnaryExpr&>(value).operand();
    while (target->kind() == NodeKind::MemberExpr)
      target = &static_cast<const MemberExpr&>(*target).object();
    if (const NameExpr* name = asName(*target); name != nullptr && name->name() != "self") {
      for (std::size_t index = scopes_.size(); index > 1; --index) {
        const auto found = scopes_[index - 1].find(name->name());
        if (found != scopes_[index - 1].end()) {
          if (!found->second.staticStorage) {
            diagnostics_->error(value.range(),
                                "cannot return the address of local '" + name->name() + "'");
            return false;
          }
          break;
        }
      }
    }
  }
  if (expectedReturn->isVoidLike()) {
    if (!isVoidLike(actual)) {
      diagnostics_->error(statement.value()->range(),
                          "return type mismatch: expected " + quoteType(expectedReturn) +
                              ", found " + quoteType(actual));
      return false;
    }
    return true;
  }
  if (!isAssignable(actual, expectedReturn)) {
    diagnostics_->error(statement.value()->range(),
                        "return type mismatch: expected " + quoteType(expectedReturn) + ", found " +
                            quoteType(actual));
    return false;
  }
  return ensureLiteralFits(*statement.value(), expectedReturn);
}

bool TypeChecker::checkStatement(Stmt& statement, const Type* expectedReturn) {
  switch (statement.kind()) {
  case NodeKind::VarDecl:
    return checkVarDecl(static_cast<VarDecl&>(statement));
  case NodeKind::AssignStmt:
    return checkAssign(static_cast<AssignStmt&>(statement));
  case NodeKind::ReturnStmt:
    return checkReturn(static_cast<ReturnStmt&>(statement), expectedReturn);
  case NodeKind::ExprStmt:
    return checkExpr(const_cast<Expr&>(static_cast<ExprStmt&>(statement).expression())) != nullptr;
  case NodeKind::IfStmt:
    return checkIf(static_cast<IfStmt&>(statement), expectedReturn);
  case NodeKind::WhileStmt:
    return checkWhile(static_cast<WhileStmt&>(statement), expectedReturn);
  case NodeKind::ForStmt:
    return checkFor(static_cast<ForStmt&>(statement), expectedReturn);
  case NodeKind::AssertStmt:
    return checkAssert(static_cast<AssertStmt&>(statement));
  case NodeKind::RaiseStmt:
    return checkRaise(static_cast<RaiseStmt&>(statement));
  case NodeKind::TryStmt:
    return checkTry(static_cast<TryStmt&>(statement), expectedReturn);
  case NodeKind::MatchStmt:
    return checkMatch(static_cast<MatchStmt&>(statement), expectedReturn);
  case NodeKind::DelStmt:
    return checkDel(static_cast<DelStmt&>(statement));
  case NodeKind::DeferStmt:
    return checkDefer(static_cast<DeferStmt&>(statement), expectedReturn);
  case NodeKind::WithStmt:
    return checkWith(static_cast<WithStmt&>(statement), expectedReturn);
  case NodeKind::BreakStmt:
    return checkBreak(static_cast<BreakStmt&>(statement));
  case NodeKind::ContinueStmt:
    return checkContinue(static_cast<ContinueStmt&>(statement));
  case NodeKind::FunctionDef: {
    auto& function = static_cast<FunctionDef&>(statement);
    if (function.resolvedType() == nullptr) {
      std::vector<const Type*> params;
      for (const ParamDecl& param : function.params()) {
        const Type* type = resolveParamDeclType(param);
        if (type == nullptr) {
          return false;
        }
        params.push_back(type);
      }
      const Type* result = resolveTypeExpr(function.returnType());
      if (result == nullptr || !validateParamList(function.params(), function.range())) {
        return false;
      }
      if (function.hasInferredReturn()) {
        result = types_->anyType();
      }
      function.setResolvedType(types_->functionType(params, result));
      function.setModulePrefix("__sere_nested_" + std::to_string(++nestedFunctionCounter_));
    }
    Symbol symbol;
    symbol.kind = SymbolKind::Function;
    symbol.type = function.resolvedType();
    symbol.function = &function;
    if (!declare(function.name(), symbol, function.range().start)) {
      return false;
    }
    FunctionDef* savedNested = nestedFunction_;
    const std::size_t savedBoundary = nestedOuterScope_;
    if (savedNested != nullptr) {
      enclosingFunctions_.emplace_back(savedNested, savedBoundary);
    }
    nestedFunction_ = &function;
    nestedOuterScope_ = scopes_.empty() ? 0 : scopes_.size() - 1;
    const bool ok = checkFunctionBody(function);
    if (savedNested != nullptr) {
      enclosingFunctions_.pop_back();
    }
    nestedFunction_ = savedNested;
    nestedOuterScope_ = savedBoundary;
    if (Symbol* bound = lookup(function.name())) {
      bound->type = function.resolvedType();
    }
    return ok;
  }
  case NodeKind::PassStmt:
  case NodeKind::ClassDef:
  case NodeKind::EnumDef:
  case NodeKind::TypeAlias:
  case NodeKind::ImportStmt:
  case NodeKind::MacroDef:
    return true;
  case NodeKind::MacroInvokeStmt:
    diagnostics_->error(statement.range(), "macro was not expanded");
    return false;
  default:
    diagnostics_->error(statement.range(), "unsupported statement");
    return false;
  }
}

bool TypeChecker::checkFunctionBody(FunctionDef& function) {
  const Type* owner = function.isMethod() ? classType(function.ownerClass()) : nullptr;
  TypeConstraintScope classConstraints(
      activeTypeConstraints_,
      owner == nullptr ? std::vector<std::string>{} : owner->typeParams(),
      owner == nullptr ? std::vector<const Type*>{} : owner->typeConstraints());
  if (!resolveTypeConstraints(function.typeConstraints()))
    return false;
  TypeConstraintScope functionConstraints(activeTypeConstraints_,
                                          function.typeParams(),
                                          resolvedConstraints(function.typeConstraints()));
  if (function.isExtern()) {
    return true;
  }
  for (const std::string& param : function.typeParams()) {
    (void)types_->defineTypeParam(param);
  }
  if (!validateParamList(function.params(), function.range())) {
    return false;
  }
  const std::string savedClass = currentClass_;
  const std::string savedFunction = currentFunctionName_;
  const std::string savedProperty = currentPropertyName_;
  const bool savedAsync = currentFunctionIsAsync_;
  currentClass_ = function.ownerClass().empty() && nestedFunction_ != nullptr
                      ? savedClass
                      : function.ownerClass();
  currentFunctionName_ = function.name();
  currentPropertyName_ = function.propertyName();
  currentFunctionIsAsync_ = function.isAsync();
  pushScope(function.range());
  for (std::size_t index = 0; index < function.params().size(); ++index) {
    const ParamDecl& param = function.params()[index];
    const Type* type = resolveParamType(function, index);
    Symbol symbol;
    symbol.kind = SymbolKind::Variable;
    symbol.type = type;
    if (type == nullptr || !declare(param.name, symbol, param.range.start)) {
      popScope();
      currentClass_ = savedClass;
      currentFunctionName_ = savedFunction;
      currentPropertyName_ = savedProperty;
      currentFunctionIsAsync_ = savedAsync;
      return false;
    }
    if (param.defaultValue != nullptr) {
      const Type* defaultType = checkExpr(*param.defaultValue);
      if (defaultType == nullptr || !isAssignable(defaultType, type)) {
        diagnostics_->error(param.range, "default value type mismatch for '" + param.name + "'");
        popScope();
        currentClass_ = savedClass;
        currentFunctionName_ = savedFunction;
        currentPropertyName_ = savedProperty;
        currentFunctionIsAsync_ = savedAsync;
        return false;
      }
    }
  }
  const Type* returnType = resolveTypeExpr(function.returnType());
  if (function.propertyKind() == PropertyKind::Get && function.hasInferredReturn()) {
    const Type* record = classType(function.ownerClass());
    const RecordField* field =
        record == nullptr ? nullptr : record->findField(function.propertyName());
    if (field != nullptr && field->type != nullptr) {
      returnType = field->type;
    }
  }
  std::vector<const Type*> inferredReturns;
  auto* savedReturns = inferredNestedReturns_;
  const bool inferNested = nestedFunction_ == &function && function.hasInferredReturn() &&
                           !function.isAsync();
  inferredNestedReturns_ = inferNested ? &inferredReturns : nullptr;
  bool ok = returnType != nullptr;
  for (const std::unique_ptr<Stmt>& statement : function.body()) {
    if (statement == nullptr) {
      continue;
    }
    ok = checkStatement(*statement, returnType) && ok;
  }
  inferredNestedReturns_ = savedReturns;
  // Preserve a concrete nested return ABI when all exits return the same type.
  // Mixed/recursive inference continues to use Any until it can be resolved.
  if (ok && inferNested && !inferredReturns.empty() && !function.body().empty() &&
      function.body().back()->kind() == NodeKind::ReturnStmt) {
    const Type* inferred = inferredReturns.front()->canonical();
    if (!inferred->isAny() &&
        std::all_of(inferredReturns.begin(), inferredReturns.end(),
                    [&](const Type* type) { return type->canonical() == inferred; })) {
      function.setResolvedType(types_->functionType(function.resolvedType()->paramTypes(), inferred));
    }
  }
  popScope();
  currentClass_ = savedClass;
  currentFunctionName_ = savedFunction;
  currentPropertyName_ = savedProperty;
  currentFunctionIsAsync_ = savedAsync;
  currentFunctionIsAsync_ = savedAsync;
  return ok;
}

const Type* TypeChecker::resolveParamDeclType(const ParamDecl& param) {
  if (param.kind == ParamKind::VarArg) {
    if (param.type == nullptr) {
      return types_->listType(types_->anyType());
    }
    const Type* type = resolveTypeExpr(*param.type);
    if (type == nullptr) {
      return nullptr;
    }
    if (!type->isList()) {
      diagnostics_->error(param.range, "*args must be annotated as list[T]");
      return nullptr;
    }
    return type;
  }
  if (param.kind == ParamKind::KwArg) {
    if (param.type == nullptr) {
      return types_->dictType(types_->strType(), types_->anyType());
    }
    const Type* type = resolveTypeExpr(*param.type);
    if (type == nullptr) {
      return nullptr;
    }
    if (!type->isDict()) {
      diagnostics_->error(param.range, "**kwargs must be annotated as dict[str, T]");
      return nullptr;
    }
    return type;
  }
  if (param.type == nullptr) {
    return types_->anyType();
  }
  return resolveTypeExpr(*param.type);
}

bool TypeChecker::validateParamList(const std::vector<ParamDecl>& params, SourceRange range) {
  bool sawDefault = false;
  bool sawVarArg = false;
  bool sawKwArg = false;
  for (const ParamDecl& param : params) {
    if (param.kind == ParamKind::KwArg) {
      if (sawKwArg) {
        diagnostics_->error(param.range, "**kwargs may appear only once");
        return false;
      }
      sawKwArg = true;
    } else if (param.kind == ParamKind::VarArg) {
      if (sawVarArg || sawKwArg) {
        diagnostics_->error(param.range, "*args must appear before **kwargs");
        return false;
      }
      sawVarArg = true;
      sawDefault = false;
    } else if (param.defaultValue != nullptr) {
      sawDefault = true;
    } else if (sawDefault && !sawVarArg) {
      diagnostics_->error(param.range, "parameter without a default follows a default");
      return false;
    } else if (sawKwArg) {
      diagnostics_->error(range, "parameters may not follow **kwargs");
      return false;
    }
    if (param.defaultValue != nullptr && param.kind != ParamKind::Normal) {
      diagnostics_->error(param.range, "vararg parameters cannot have defaults");
      return false;
    }
  }
  return true;
}

void TypeChecker::importMethod(const Type* record, FunctionDef& method) {
  if (record != nullptr) {
    importedMethods_[record->canonical()][method.name()] = &method;
  }
}

FunctionDef* TypeChecker::findMethodDef(const Type* record, std::string_view methodName) {
  if (record == nullptr) {
    return nullptr;
  }
  record = record->canonical();
  const Type* definitionRecord = record;
  for (const auto& [generic, instance] : types_->instantiations()) {
    if (instance == record) {
      definitionRecord = generic;
      break;
    }
  }
  if (const auto imported = importedMethods_.find(definitionRecord);
      imported != importedMethods_.end()) {
    const auto method = imported->second.find(std::string(methodName));
    if (method != imported->second.end()) {
      return method->second;
    }
  }
  std::string baseClassName = record->name();
  if (const auto bracket = baseClassName.find('['); bracket != std::string::npos) {
    baseClassName = baseClassName.substr(0, bracket);
  }
  for (const auto& entry : classes_) {
    ClassDef* classDef = entry.second;
    if (classDef == nullptr || classDef->resolvedType() == nullptr) {
      continue;
    }
    if (classDef->resolvedType()->canonical() == record || classDef->name() == baseClassName ||
        entry.first == baseClassName) {
      for (const std::unique_ptr<FunctionDef>& method : classDef->methods()) {
        if (method != nullptr && method->name() == methodName) {
          return method.get();
        }
      }
    }
  }
  const int methodIndex = record->methodIndex(methodName);
  for (const Type* base : record->bases()) {
    const int baseIndex = base->methodIndex(methodName);
    // Imported overrides have no local AST. Do not bind their arguments using
    // the unrelated signature of an ancestor's method.
    if (methodIndex >= 0 && baseIndex >= 0 &&
        record->methods()[static_cast<std::size_t>(methodIndex)].llvmName !=
            base->methods()[static_cast<std::size_t>(baseIndex)].llvmName)
      continue;
    if (FunctionDef* found = findMethodDef(base, methodName))
      return found;
  }
  return nullptr;
}

bool TypeChecker::checkFunctionArguments(CallExpr& expr,
                                         const std::vector<ParamDecl>& params,
                                         const std::vector<const Type*>& paramTypes,
                                         std::string_view calleeLabel,
                                         const std::size_t selfSkip) {
  if (params.size() != paramTypes.size()) {
    diagnostics_->error(expr.range(), "internal: parameter metadata mismatch");
    return false;
  }
  std::unordered_map<std::string, std::size_t> keywordIndexes;
  std::vector<Expr*> kwSplats;
  for (std::size_t index = 0; index < expr.keywordArguments().size(); ++index) {
    const NamedArgument& kw = expr.keywordArguments()[index];
    if (kw.splat || kw.name.empty()) {
      if (kw.value != nullptr) {
        kwSplats.push_back(kw.value.get());
      }
      continue;
    }
    if (keywordIndexes.contains(kw.name)) {
      diagnostics_->error(kw.value->range(), "duplicate keyword argument '" + kw.name + "'");
      return false;
    }
    keywordIndexes[kw.name] = index;
  }

  std::optional<std::size_t> varArgIndex;
  std::optional<std::size_t> kwArgIndex;
  for (std::size_t index = selfSkip; index < params.size(); ++index) {
    if (params[index].kind == ParamKind::VarArg) {
      varArgIndex = index;
    } else if (params[index].kind == ParamKind::KwArg) {
      kwArgIndex = index;
    }
  }

  const auto paramLabel = [&](std::size_t index) -> std::string {
    return std::string(calleeLabel) + " parameter '" + params[index].name + "'";
  };
  const auto takeKeyword = [&](const std::string& name) -> Expr* {
    const auto found = keywordIndexes.find(name);
    if (found == keywordIndexes.end()) {
      return nullptr;
    }
    Expr* value = expr.keywordArguments()[found->second].value.get();
    keywordIndexes.erase(found);
    return value;
  };
  const auto checkArg =
      [&](Expr& argument, const Type* expected, const std::string& label) -> bool {
    if (expected != nullptr &&
        ((argument.kind() == NodeKind::ListLiteral && expected->isSequence()) ||
         (argument.kind() == NodeKind::DictLiteral && expected->isDict()))) {
      return bindCollectionInit(argument, expected);
    }
    const Type* argType = checkExpr(argument);
    if (argType == nullptr) {
      return false;
    }
    if (!isAssignable(argType, expected)) {
      diagnostics_->error(argument.range(),
                          label + " type mismatch: expected " + quoteType(expected) + ", found " +
                              quoteType(argType));
      return false;
    }
    return ensureLiteralFits(argument, expected);
  };

  std::size_t positionalIndex = 0;
  std::vector<const Expr*> bound(params.size(), nullptr);
  std::vector<std::unique_ptr<Expr>> owned;
  const auto useArgument = [&](std::size_t paramIndex, Expr* argument) {
    bound[paramIndex] = argument;
  };

  const std::size_t preVarArgEnd =
      varArgIndex.has_value() ? varArgIndex.value()
                              : (kwArgIndex.has_value() ? kwArgIndex.value() : params.size());
  for (std::size_t index = selfSkip; index < preVarArgEnd; ++index) {
    if (params[index].kind != ParamKind::Normal) {
      continue;
    }
    Expr* argument = nullptr;
    if (positionalIndex < expr.arguments().size()) {
      argument = expr.arguments()[positionalIndex++].get();
    } else {
      argument = takeKeyword(params[index].name);
    }
    if (argument == nullptr) {
      if (params[index].defaultValue != nullptr) {
        continue;
      }
      diagnostics_->error(expr.range(),
                          "missing argument '" + params[index].name + "' to " +
                              std::string(calleeLabel));
      return false;
    }
    if (!checkArg(*argument, paramTypes[index], paramLabel(index))) {
      return false;
    }
    useArgument(index, argument);
  }

  if (varArgIndex.has_value()) {
    const std::size_t index = varArgIndex.value();
    const Type* listType = paramTypes[index];
    const Type* elementType =
        listType != nullptr && listType->isList() ? listType->elementType() : types_->anyType();
    if (Expr* explicitArg = takeKeyword(params[index].name)) {
      if (!checkArg(*explicitArg, listType, paramLabel(index))) {
        return false;
      }
      useArgument(index, explicitArg);
    } else {
      while (positionalIndex < expr.arguments().size()) {
        if (!checkArg(
                *expr.arguments()[positionalIndex], elementType, paramLabel(index) + " element")) {
          return false;
        }
        ++positionalIndex;
      }
    }
  } else if (positionalIndex < expr.arguments().size()) {
    diagnostics_->error(expr.arguments()[positionalIndex]->range(),
                        "too many positional arguments to " + std::string(calleeLabel));
    return false;
  }

  const std::size_t keywordOnlyStart =
      varArgIndex.has_value() ? varArgIndex.value() + 1 : preVarArgEnd;
  const std::size_t keywordOnlyEnd = kwArgIndex.has_value() ? kwArgIndex.value() : params.size();
  for (std::size_t index = keywordOnlyStart; index < keywordOnlyEnd; ++index) {
    if (params[index].kind != ParamKind::Normal) {
      continue;
    }
    Expr* argument = takeKeyword(params[index].name);
    if (argument == nullptr) {
      if (params[index].defaultValue != nullptr) {
        continue;
      }
      diagnostics_->error(expr.range(),
                          "missing keyword argument '" + params[index].name + "' to " +
                              std::string(calleeLabel));
      return false;
    }
    if (!checkArg(*argument, paramTypes[index], paramLabel(index))) {
      return false;
    }
    useArgument(index, argument);
  }

  if (kwArgIndex.has_value()) {
    const std::size_t index = kwArgIndex.value();
    const Type* dictType = paramTypes[index];
    const Type* valueType =
        dictType != nullptr && dictType->isDict() ? dictType->dictValueType() : types_->anyType();
    if (Expr* explicitArg = takeKeyword(params[index].name)) {
      if (!checkArg(*explicitArg, dictType, paramLabel(index))) {
        return false;
      }
      useArgument(index, explicitArg);
    } else {
      for (const auto& entry : keywordIndexes) {
        if (!checkArg(*expr.keywordArguments()[entry.second].value,
                      valueType,
                      "unexpected keyword argument '" + entry.first + "'")) {
          return false;
        }
      }
      keywordIndexes.clear();
    }
    for (Expr* splat : kwSplats) {
      if (splat == nullptr) {
        continue;
      }
      if (!checkArg(
              *splat, dictType != nullptr ? dictType : types_->anyType(), paramLabel(index))) {
        return false;
      }
    }
    kwSplats.clear();
  }

  for (Expr* splat : kwSplats) {
    if (splat == nullptr) {
      continue;
    }
    const Type* splatType = checkExpr(*splat);
    if (splatType == nullptr) {
      return false;
    }
    if (!splatType->isDict() && !splatType->isAny()) {
      diagnostics_->error(splat->range(), "keyword unpack argument must be a dict");
      return false;
    }
  }
  if (!kwSplats.empty()) {
    keywordIndexes.clear();
  }

  if (!keywordIndexes.empty()) {
    const NamedArgument& extra = expr.keywordArguments()[keywordIndexes.begin()->second];
    diagnostics_->error(extra.value->range(),
                        "unexpected keyword argument '" + extra.name + "' to " +
                            std::string(calleeLabel));
    return false;
  }

  for (std::size_t index = selfSkip; index < params.size(); ++index) {
    if (bound[index] == nullptr && params[index].defaultValue != nullptr) {
      bound[index] = params[index].defaultValue.get();
    }
  }

  expr.setBoundArguments(std::move(bound), std::move(owned));
  return true;
}

const Type* TypeChecker::resolveParamType(const FunctionDef& function, std::size_t index) {
  const ParamDecl& param = function.params()[index];
  if (function.isMethod() && index == 0) {
    if (param.name != "self") {
      diagnostics_->error(param.range, "first method parameter must be named self");
      return nullptr;
    }
    return classType(function.ownerClass());
  }
  return resolveParamDeclType(param);
}

bool TypeChecker::collectClassNames(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::ClassDef) {
      continue;
    }
    auto& classDef = static_cast<ClassDef&>(*statement);
    classes_[classDef.name()] = &classDef;
    const std::string qualifier = classDef.fromPrelude() ? "prelude" : moduleName_;
    const Type* existing =
        classDef.fromPrelude() ? types_->record("prelude." + classDef.name()) : nullptr;
    const Type* record =
        existing != nullptr ? existing : types_->defineRecord(classDef.name(), {}, qualifier);
    types_->setRecordTypeParams(record, classDef.typeParams());
    recordConstraintExprs_[record] = &classDef.typeConstraints();
    for (const std::string& param : classDef.typeParams()) {
      (void)types_->defineTypeParam(param);
    }
    classDef.setResolvedType(record);
    types_->setRecordStruct(record, classDef.isStruct());
    types_->setRecordFrozen(record, classDef.isFrozen());
    Symbol symbol;
    symbol.kind = SymbolKind::Class;
    symbol.type = record;
    if (!declare(classDef.name(), symbol, classDef.range().start, !classDef.fromPrelude())) {
      return false;
    }
  }
  return true;
}

bool TypeChecker::collectEnumNames(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::EnumDef)
      continue;
    auto& enumDef = static_cast<EnumDef&>(*statement);
    const std::string qualifier = enumDef.fromPrelude() ? "prelude" : moduleName_;
    const Type* existing =
        enumDef.fromPrelude() ? types_->record("prelude." + enumDef.name()) : nullptr;
    const Type* record =
        existing != nullptr ? existing : types_->defineRecord(enumDef.name(), {}, qualifier);
    types_->setRecordTypeParams(record, enumDef.typeParams());
    recordConstraintExprs_[record] = &enumDef.typeConstraints();
    for (const std::string& param : enumDef.typeParams()) {
      (void)types_->defineTypeParam(param);
    }
    types_->setRecordEnum(record, true);
    types_->setRecordFlags(record, enumDef.isFlags());
    enumDef.setResolvedType(record);
    Symbol symbol;
    symbol.kind = SymbolKind::Class;
    symbol.type = record;
    if (!declare(enumDef.name(), symbol, enumDef.range().start, !enumDef.fromPrelude())) {
      return false;
    }
  }
  return true;
}

bool TypeChecker::collectEnums(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::EnumDef) {
      continue;
    }
    auto& enumDef = static_cast<EnumDef&>(*statement);
    const Type* record = enumDef.resolvedType();
    if (!ensureRecordConstraints(record))
      return false;
    TypeConstraintScope constraintScope(
        activeTypeConstraints_, enumDef.typeParams(), record->typeConstraints());
    types_->setRecordEnum(record, true);
    types_->setRecordFlags(record, enumDef.isFlags());
    std::vector<RecordField> fields;
    std::int64_t next = 0;
    for (EnumVariant& variant : enumDef.variants()) {
      if (variant.value != nullptr) {
        const std::optional<std::int64_t> value = evalEnumInt(*variant.value);
        if (!value.has_value()) {
          diagnostics_->error(variant.range, "enum values must be integer literals");
          return false;
        }
        next = *value;
      }
      RecordField field;
      field.name = variant.name;
      field.type = nullptr;
      field.isPublic = true;
      field.isStatic = true;
      field.llvmName = std::to_string(next);
      for (EnumPayloadField& payload : variant.payload) {
        const Type* payloadType =
            payload.type == nullptr ? nullptr : resolveTypeExpr(*payload.type);
        if (payloadType == nullptr && payload.type != nullptr) {
          return false;
        }
        field.payloadTypes.push_back(payloadType);
        field.paramNames.push_back(payload.name);
      }
      fields.push_back(std::move(field));
      ++next;
    }
    for (RecordField& field : fields) {
      field.type = record;
    }
    types_->setRecordFields(record, std::move(fields));
    for (const EnumVariant& variant : enumDef.variants()) {
      recordSymbol(variant.name, "enumMember", record, variant.range.start, enumDef.name());
    }
  }
  return true;
}

bool TypeChecker::collectAliases(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement == nullptr || statement->kind() != NodeKind::TypeAlias) {
      continue;
    }
    auto& alias = static_cast<TypeAlias&>(*statement);
    const Type* underlying = resolveTypeExpr(alias.type());
    if (underlying == nullptr) {
      return false;
    }
    const Type* type = types_->defineAlias(alias.name(), underlying);
    alias.setResolvedType(type);
    Symbol symbol;
    symbol.kind = SymbolKind::Type;
    symbol.type = type;
    if (!declare(alias.name(), symbol, alias.range().start, !alias.fromPrelude())) {
      return false;
    }
  }
  return true;
}

bool TypeChecker::flattenClass(ClassDef& classDef) {
  if (!ensureRecordConstraints(classDef.resolvedType()))
    return false;
  TypeConstraintScope constraintScope(
      activeTypeConstraints_, classDef.typeParams(), classDef.resolvedType()->typeConstraints());
  if (flattened_[classDef.name()]) {
    return true;
  }
  flattened_[classDef.name()] = true;
  std::vector<RecordField> fields;
  std::vector<const Type*> bases;
  const auto inheritFields = [&](const Type* base) {
    if (!base->isRecord()) {
      bool exists = false;
      for (const RecordField& field : fields)
        exists = exists || field.name == "$value";
      if (!exists)
        fields.push_back(RecordField{"$value", base, false, false});
    }
    for (const RecordField& field : base->fields()) {
      bool exists = false;
      for (const RecordField& seen : fields) {
        exists = exists || seen.name == field.name;
      }
      if (!exists) {
        fields.push_back(field);
      }
    }
  };
  const auto resolveBase =
      [&](const std::string& baseName, SourceRange range, TypeExpr* typeExpr) -> const Type* {
    if (classDef.isStruct()) {
      diagnostics_->error(classDef.range(), "structs cannot inherit; use class");
      return nullptr;
    }
    const auto found = classes_.find(baseName);
    if (found != classes_.end() && !flattenClass(*found->second)) {
      return nullptr;
    }
    const Type* base = typeExpr != nullptr ? resolveTypeExpr(*typeExpr) : classType(baseName);
    if (base == nullptr) {
      const std::vector<std::unique_ptr<TypeExpr>> noArgs;
      base = resolveNamedType(baseName, noArgs, range, false);
    }
    if (const Type* record = unwrapRecordType(base)) {
      return record;
    }
    if (base != nullptr)
      return base;
    diagnostics_->error(range, "unknown base class '" + baseName + "'");
    return nullptr;
  };
  if (!classDef.baseTypes().empty()) {
    for (std::unique_ptr<TypeExpr>& baseType : classDef.baseTypes()) {
      if (baseType == nullptr) {
        continue;
      }
      const Type* base = resolveBase(baseType->name(), baseType->range(), baseType.get());
      if (base == nullptr) {
        return false;
      }
      bases.push_back(base);
      inheritFields(base);
    }
  } else {
    for (const std::string& baseName : classDef.bases()) {
      const Type* base = resolveBase(baseName, classDef.range(), nullptr);
      if (base == nullptr) {
        return false;
      }
      bases.push_back(base);
      inheritFields(base);
    }
  }
  for (const FieldDecl& field : classDef.fields()) {
    const Type* type = resolveTypeExpr(*field.type);
    if (type == nullptr) {
      continue;
    }
    RecordField created{field.name, type, field.isPublic, field.isStatic};
    if (field.isStatic) {
      const Type* record = classDef.resolvedType();
      const std::string owner = record != nullptr && !record->qualifier().empty()
                                    ? record->qualifier() + "." + record->name()
                                    : classDef.name();
      created.llvmName = owner + "." + field.name;
    }
    bool replaced = false;
    for (RecordField& existing : fields) {
      if (existing.name == field.name) {
        existing = created;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      fields.push_back(std::move(created));
    }
    recordSymbol(field.name, "field", type, field.range.start, classDef.name());
  }
  types_->setRecordFields(classDef.resolvedType(), std::move(fields));
  types_->setRecordBases(classDef.resolvedType(), std::move(bases));
  return true;
}

bool TypeChecker::collectClassFields(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() == NodeKind::ClassDef &&
        !flattenClass(static_cast<ClassDef&>(*statement))) {
      return false;
    }
  }
  return true;
}

void TypeChecker::inheritBaseMethods(const Type* record, const Type* base) {
  if (record == nullptr || base == nullptr) {
    return;
  }
  for (const Type* parent : base->bases()) {
    inheritBaseMethods(record, parent);
  }
  for (const RecordMethod& method : base->methods()) {
    if (record->methodIndex(method.name) < 0) {
      types_->addRecordMethod(record, method);
    }
  }
}

bool TypeChecker::collectFunctions(Module& module) {
  bool ok = true;
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::FunctionDef) {
      continue;
    }
    auto& function = static_cast<FunctionDef&>(*statement);
    if (!resolveTypeConstraints(function.typeConstraints()))
      return false;
    TypeConstraintScope constraintScope(activeTypeConstraints_,
                                        function.typeParams(),
                                        resolvedConstraints(function.typeConstraints()));
    for (const std::string& param : function.typeParams()) {
      (void)types_->defineTypeParam(param);
    }
    std::vector<const Type*> params;
    bool paramsOk = true;
    for (const ParamDecl& param : function.params()) {
      const Type* type = resolveParamDeclType(param);
      if (type == nullptr) {
        paramsOk = false;
        break;
      }
      params.push_back(type);
    }
    if (!paramsOk || !validateParamList(function.params(), function.range())) {
      ok = false;
      continue;
    }
    const Type* returnType = resolveTypeExpr(function.returnType());
    if (returnType == nullptr) {
      ok = false;
      continue;
    }
    if (function.hasInferredReturn() && function.name() != "main" &&
        function.name() != "__init__") {
      returnType = types_->anyType();
    }
    const Type* fnType = types_->functionType(params, returnType);
    function.setResolvedType(fnType);
    Symbol symbol;
    symbol.kind = SymbolKind::Function;
    symbol.type = fnType;
    symbol.function = &function;
    if (!declare(function.name(), symbol, function.range().start, !function.fromPrelude())) {
      ok = false;
    }
  }
  (void)ok;
  return true;
}

bool TypeChecker::collectMacros(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::MacroDef) {
      continue;
    }
    auto& def = static_cast<MacroDef&>(*statement);
    Symbol symbol;
    symbol.kind = SymbolKind::Macro;
    symbol.paramNames = def.params();
    symbol.typeDisplay = formatMacro(def);
    symbol.snippet = macroSnippet(def);
    const SourceLocation location = def.nameRange().end.offset > def.nameRange().start.offset
                                        ? def.nameRange().start
                                        : def.range().start;
    if (!declare(def.name(), symbol, location, !def.fromPrelude())) {
      return false;
    }
  }
  return true;
}

bool TypeChecker::collectMethods(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::ClassDef) {
      continue;
    }
    auto& classDef = static_cast<ClassDef&>(*statement);
    const Type* record = classDef.resolvedType();
    if (record == nullptr) {
      continue;
    }
    TypeConstraintScope classConstraints(
        activeTypeConstraints_, classDef.typeParams(), record->typeConstraints());
    for (std::unique_ptr<FunctionDef>& method : classDef.methods()) {
      if (!resolveTypeConstraints(method->typeConstraints()))
        return false;
      TypeConstraintScope methodConstraints(activeTypeConstraints_,
                                            method->typeParams(),
                                            resolvedConstraints(method->typeConstraints()));
      for (const std::string& param : method->typeParams()) {
        (void)types_->defineTypeParam(param);
      }
      if (method->params().empty() || method->params()[0].name != "self") {
        diagnostics_->error(method->range(), "methods must take self as the first parameter");
        continue;
      }
      std::vector<const Type*> params;
      bool paramsOk = true;
      for (std::size_t index = 0; index < method->params().size(); ++index) {
        const Type* type = resolveParamType(*method, index);
        if (type == nullptr) {
          paramsOk = false;
          break;
        }
        params.push_back(type);
      }
      if (!paramsOk) {
        continue;
      }
      const Type* returnType = resolveTypeExpr(method->returnType());
      if (returnType == nullptr) {
        continue;
      }
      if (method->propertyKind() == PropertyKind::Get && method->hasInferredReturn()) {
        const RecordField* field = record->findField(method->propertyName());
        if (field != nullptr && field->type != nullptr) {
          returnType = field->type;
        }
      }
      if (method->propertyKind() == PropertyKind::Set) {
        if (params.size() != 2) {
          diagnostics_->error(method->range(),
                              "setter '" + method->propertyName() +
                                  "' takes one value besides self");
          continue;
        }
        if (!returnType->isVoidLike()) {
          diagnostics_->error(method->range(),
                              "setter '" + method->propertyName() + "' must return void");
          continue;
        }
      }
      if (method->name() == "__init__" && !returnType->isVoidLike()) {
        diagnostics_->error(method->range(), "__init__ must return void or None");
        continue;
      }
      if (method->name() == "__str__") {
        if (params.size() != 1) {
          diagnostics_->error(method->range(), "__str__ takes only self");
          continue;
        }
        if (!returnType->isNamed("str")) {
          diagnostics_->error(method->range(), "__str__ must return str");
          continue;
        }
      }
      if (method->name() == "__repr__" && !returnType->isNamed("str")) {
        diagnostics_->error(method->range(), "__repr__ must return str");
        continue;
      }
      if (method->name() == "__len__" && (params.size() != 1 || !returnType->isNamed("i64"))) {
        diagnostics_->error(method->range(), "__len__ must be def __len__(self) -> i64");
        continue;
      }
      if (method->name() == "__bool__" && (params.size() != 1 || !returnType->isNamed("bool"))) {
        diagnostics_->error(method->range(), "__bool__ must be def __bool__(self) -> bool");
        continue;
      }
      if (method->name() == "__contains__" &&
          (params.size() != 2 || !returnType->isNamed("bool"))) {
        diagnostics_->error(method->range(),
                            "__contains__ must be def __contains__(self, item: T) -> bool");
        continue;
      }
      if (method->name() == "__getitem__" && params.size() != 2) {
        diagnostics_->error(method->range(),
                            "__getitem__ must be def __getitem__(self, index: T) -> U");
        continue;
      }
      if (method->name() == "__setitem__" && (params.size() != 3 || !returnType->isVoidLike())) {
        diagnostics_->error(
            method->range(),
            "__setitem__ must be def __setitem__(self, index: K, value: V) -> void");
        continue;
      }
      const Type* fnType = types_->functionType(params, returnType);
      method->setResolvedType(fnType);
      RecordMethod info;
      info.name = method->name();
      info.type = fnType;
      const std::string methodModule = classDef.fromPrelude() ? "prelude" : moduleName_;
      if (methodModule != "__main__" && !methodModule.empty()) {
        method->setModulePrefix(methodModule);
        info.llvmName = methodModule + "_" + classDef.name() + "_" + method->name();
      } else {
        info.llvmName = classDef.name() + "_" + method->name();
      }
      info.isAbstract = abstractMethodNeedsOverride(*method);
      info.isPublic = !method->isPrivate();
      info.typeParams = method->typeParams();
      info.typeConstraints = resolvedConstraints(method->typeConstraints());
      bool sawDefault = false;
      for (std::size_t index = 0; index < method->params().size(); ++index) {
        const ParamDecl& param = method->params()[index];
        info.paramNames.push_back(param.name);
        if (index == 0) {
          continue;
        }
        if (param.defaultValue != nullptr) {
          sawDefault = true;
        } else if (sawDefault) {
          diagnostics_->error(param.range, "parameter without a default follows a default");
          continue;
        } else {
          ++info.requiredAfterSelf;
        }
      }
      std::vector<std::string> methodParams = info.paramNames;
      types_->replaceRecordMethod(record, std::move(info));
      recordSymbol(method->name(),
                   "method",
                   fnType,
                   method->range().start,
                   classDef.name(),
                   std::move(methodParams));
    }
    std::vector<RecordField> fields = record->fields();
    for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
      if (method->propertyKind() == PropertyKind::None) {
        continue;
      }
      const std::string llvmName = (moduleName_ != "__main__" && !moduleName_.empty())
                                       ? moduleName_ + "_" + classDef.name() + "_" + method->name()
                                       : classDef.name() + "_" + method->name();
      RecordField* found = nullptr;
      for (RecordField& field : fields) {
        if (field.name == method->propertyName()) {
          found = &field;
          break;
        }
      }
      if (found == nullptr) {
        RecordField created;
        created.name = method->propertyName();
        created.stored = false;
        created.isPublic = false;
        fields.push_back(std::move(created));
        found = &fields.back();
      }
      if (method->propertyKind() == PropertyKind::Get) {
        if (!found->getterLlvm.empty()) {
          diagnostics_->error(method->range(),
                              "duplicate getter for '" + method->propertyName() + "'");
          continue;
        }
        found->getterLlvm = llvmName;
        found->getterPublic = !method->isPrivate();
        if (found->type == nullptr && method->resolvedType() != nullptr) {
          found->type = method->resolvedType()->returnType();
        }
      } else {
        if (!found->setterLlvm.empty()) {
          diagnostics_->error(method->range(),
                              "duplicate setter for '" + method->propertyName() + "'");
          continue;
        }
        found->setterLlvm = llvmName;
        found->setterPublic = !method->isPrivate();
        if (found->type == nullptr && method->resolvedType() != nullptr &&
            method->resolvedType()->paramTypes().size() > 1) {
          found->type = method->resolvedType()->paramTypes()[1];
        }
      }
    }
    types_->setRecordFields(record, std::move(fields));
  }
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::ClassDef) {
      continue;
    }
    auto& classDef = static_cast<ClassDef&>(*statement);
    const Type* record = classDef.resolvedType();
    if (record == nullptr) {
      continue;
    }
    for (const Type* base : record->bases()) {
      inheritBaseMethods(record, base);
    }
    bool abstractClass = false;
    for (const RecordMethod& method : record->methods()) {
      abstractClass = abstractClass || method.isAbstract;
    }
    types_->setRecordAbstract(record, abstractClass);
  }
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::EnumDef) {
      continue;
    }
    auto& enumDef = static_cast<EnumDef&>(*statement);
    const Type* record = enumDef.resolvedType();
    if (record == nullptr) {
      continue;
    }
    TypeConstraintScope classConstraints(
        activeTypeConstraints_, enumDef.typeParams(), record->typeConstraints());
    for (std::unique_ptr<FunctionDef>& method : enumDef.methods()) {
      if (!resolveTypeConstraints(method->typeConstraints()))
        return false;
      TypeConstraintScope methodConstraints(activeTypeConstraints_,
                                            method->typeParams(),
                                            resolvedConstraints(method->typeConstraints()));
      for (const std::string& param : method->typeParams())
        (void)types_->defineTypeParam(param);
      if (method->params().empty() || method->params()[0].name != "self") {
        diagnostics_->error(method->range(), "methods must take self as the first parameter");
        continue;
      }
      std::vector<const Type*> params;
      bool paramsOk = true;
      for (std::size_t index = 0; index < method->params().size(); ++index) {
        const Type* type = resolveParamType(*method, index);
        if (type == nullptr) {
          paramsOk = false;
          break;
        }
        params.push_back(type);
      }
      if (!paramsOk) {
        continue;
      }
      const Type* returnType = resolveTypeExpr(method->returnType());
      if (returnType == nullptr) {
        continue;
      }
      const Type* fnType = types_->functionType(params, returnType);
      method->setResolvedType(fnType);
      RecordMethod info;
      info.name = method->name();
      info.type = fnType;
      info.llvmName = enumDef.name() + "_" + method->name();
      info.typeParams = method->typeParams();
      info.typeConstraints = resolvedConstraints(method->typeConstraints());
      info.isPublic = !method->isPrivate();
      for (std::size_t index = 0; index < method->params().size(); ++index) {
        info.paramNames.push_back(method->params()[index].name);
        if (index > 0 && method->params()[index].defaultValue == nullptr) {
          ++info.requiredAfterSelf;
        }
      }
      types_->replaceRecordMethod(record, std::move(info));
    }
  }
  return true;
}

bool TypeChecker::bindLocalClassImports(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::ImportStmt) {
      continue;
    }
    const auto& import = static_cast<const ImportStmt&>(*statement);
    if (!import.isFrom() || import.star() || import.modulePath().size() != 1) {
      continue;
    }
    const std::string className = import.modulePath()[0];
    if (lookup(className) != nullptr && lookup(className)->kind == SymbolKind::Module) {
      continue;
    }
    ClassDef* classDef = nullptr;
    const auto found = classes_.find(className);
    if (found != classes_.end()) {
      classDef = found->second;
    }
    if (classDef == nullptr) {
      bool imported = false;
      for (std::size_t index = 0; index < import.names().size(); ++index) {
        if (lookup(import.boundName(index)) != nullptr) {
          imported = true;
          break;
        }
      }
      if (!imported) {
        diagnostics_->error(import.range(), "cannot find module or class '" + className + "'");
        return false;
      }
      continue;
    }
    for (std::size_t index = 0; index < import.names().size(); ++index) {
      const std::string& name = import.names()[index];
      const std::string bound = import.boundName(index);
      FunctionDef* method = nullptr;
      for (std::unique_ptr<FunctionDef>& item : classDef->methods()) {
        if (item->name() == name || item->propertyName() == name) {
          method = item.get();
          break;
        }
      }
      if (method == nullptr) {
        diagnostics_->error(import.range(),
                            "class '" + className + "' has no member '" + name + "'");
        return false;
      }
      Symbol symbol;
      symbol.kind = SymbolKind::Function;
      symbol.type = method->resolvedType();
      symbol.function = method;
      if (!declare(bound, symbol, import.range().start)) {
        return false;
      }
      RecordField alias;
      alias.name = bound;
      alias.type = method->resolvedType();
      alias.isPublic = true;
      alias.llvmName = (moduleName_ != "__main__" && !moduleName_.empty())
                           ? moduleName_ + "_" + classDef->name() + "_" + method->name()
                           : classDef->name() + "_" + method->name();
      for (const ParamDecl& param : method->params()) {
        alias.paramNames.push_back(param.name);
      }
      alias.requiredArgs = method->params().size();
      module.addExportAlias(std::move(alias));
    }
  }
  return true;
}

bool TypeChecker::collectExports(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::AssignStmt) {
      continue;
    }
    auto& assign = static_cast<AssignStmt&>(*statement);
    const NameExpr* target = asName(assign.target());
    if (target == nullptr || target->name() != "__exports__") {
      continue;
    }
    if (assign.value().kind() != NodeKind::ListLiteral) {
      diagnostics_->error(assign.range(), "__exports__ must be a list of names");
      return false;
    }
    std::vector<std::string> names;
    bool namesOk = true;
    for (const std::unique_ptr<Expr>& item :
         static_cast<const ListLiteral&>(assign.value()).elements()) {
      const NameExpr* name = asName(*item);
      if (name == nullptr) {
        diagnostics_->error(item->range(), "__exports__ entries must be names");
        return false;
      }
      names.push_back(name->name());
      const Symbol* symbol = lookup(name->name());
      if (symbol == nullptr) {
        diagnostics_->error(name->range(),
                            "__exports__ names unknown symbol '" + name->name() + "'");
        namesOk = false;
        continue;
      }
      RecordField alias;
      alias.name = name->name();
      alias.type = symbol->type;
      alias.isPublic = true;
      alias.paramNames = symbol->paramNames;
      if (symbol->function != nullptr) {
        alias.requiredArgs = symbol->function->params().size();
      }
      module.addExportAlias(std::move(alias));
    }
    const ModuleExportMode mode =
        assign.op() == AssignOp::Add ? ModuleExportMode::Append : ModuleExportMode::Replace;
    if (assign.op() != AssignOp::Assign && assign.op() != AssignOp::Add) {
      diagnostics_->error(assign.range(), "use __exports__ = [...] or __exports__ += [...]");
      return false;
    }
    module.setExportList(mode, std::move(names));
    if (!namesOk) {
      return false;
    }
  }
  return true;
}

bool TypeChecker::checkBodies(Module& module) {
  bool ok = true;
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() == NodeKind::VarDecl) {
      ok = checkVarDecl(static_cast<VarDecl&>(*statement)) && ok;
    } else if (statement->kind() == NodeKind::AssignStmt) {
      const NameExpr* target = asName(static_cast<AssignStmt&>(*statement).target());
      if (target != nullptr && target->name() == "__exports__") {
        continue;
      }
      ok = checkAssign(static_cast<AssignStmt&>(*statement)) && ok;
    }
  }
  // Decorator expressions can reference module variables (for example @app.route).
  // Bind those variables first, then expose decorated signatures to body checking.
  if (!ok || !applyDecorators(module)) {
    return false;
  }
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::ClassDef) {
      continue;
    }
    auto& classDef = static_cast<ClassDef&>(*statement);
    currentClass_ = classDef.name();
    for (const FieldDecl& field : classDef.fields()) {
      if (field.init == nullptr) {
        continue;
      }
      const Type* dest = resolveTypeExpr(*field.type);
      Expr& init = const_cast<Expr&>(*field.init);
      const bool collection =
          (init.kind() == NodeKind::ListLiteral && dest != nullptr && dest->isSequence()) ||
          (init.kind() == NodeKind::DictLiteral && dest != nullptr && dest->isDict());
      if (collection) {
        ok = bindCollectionInit(init, dest) && ok;
        continue;
      }
      const Type* initType = checkExpr(init);
      if (dest == nullptr || initType == nullptr || !isAssignable(initType, dest)) {
        diagnostics_->error(init.range(), "static field initializer type mismatch");
        ok = false;
      }
    }
    currentClass_.clear();
  }
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() == NodeKind::FunctionDef) {
      ok = checkFunctionBody(static_cast<FunctionDef&>(*statement)) && ok;
    } else if (statement->kind() == NodeKind::ClassDef) {
      auto& classDef = static_cast<ClassDef&>(*statement);
      for (const std::string& param : classDef.typeParams()) {
        (void)types_->defineTypeParam(param);
      }
      for (std::unique_ptr<FunctionDef>& method : classDef.methods()) {
        ok = checkFunctionBody(*method) && ok;
      }
    } else if (statement->kind() == NodeKind::EnumDef) {
      for (std::unique_ptr<FunctionDef>& method : static_cast<EnumDef&>(*statement).methods()) {
        ok = checkFunctionBody(*method) && ok;
      }
    }
  }
  return ok;
}

const Type* TypeChecker::checkTernary(TernaryExpr& expr) {
  const Type* thenType = checkExpr(expr.thenValue());
  const Type* condType = checkExpr(expr.condition());
  const Type* elseType = checkExpr(expr.elseValue());
  if (thenType == nullptr || condType == nullptr || elseType == nullptr) {
    return nullptr;
  }
  if (!condType->isNamed("bool")) {
    diagnostics_->error(expr.condition().range(), "ternary condition must be bool");
    return nullptr;
  }
  if (thenType->canonical() == elseType->canonical()) {
    expr.setResolvedType(thenType);
    return thenType;
  }
  const Type* unified = types_->unionType({thenType, elseType});
  expr.setResolvedType(unified);
  return unified;
}

const Type* TypeChecker::checkTuple(TupleExpr& expr) {
  std::vector<const Type*> elems;
  for (std::unique_ptr<Expr>& item : expr.elements()) {
    const Type* type = checkExpr(*item);
    if (type == nullptr) {
      return nullptr;
    }
    elems.push_back(type);
  }
  const Type* result = types_->generic("tuple", elems);
  expr.setResolvedType(result);
  return result;
}

bool TypeChecker::checkRaise(RaiseStmt& statement) {
  if (statement.value() == nullptr) {
    return true;
  }
  const Type* raised = checkExpr(const_cast<Expr&>(*statement.value()));
  if (raised == nullptr)
    return false;
  const Type* type = unwrapRecordType(raised);
  auto isException = [](auto&& self, const Type* candidate) -> bool {
    if (candidate == nullptr)
      return false;
    if (candidate->name() == "Exception")
      return true;
    for (const Type* base : candidate->bases())
      if (self(self, base->canonical()))
        return true;
    return false;
  };
  if (!isException(isException, type)) {
    diagnostics_->error(statement.range(), "raise requires an Exception subclass or instance");
    return false;
  }
  if (raised->isTypeObject()) {
    const int init = type->methodIndex("__init__");
    if (init >= 0 && type->methods()[static_cast<std::size_t>(init)].requiredAfterSelf != 0) {
      diagnostics_->error(statement.range(),
                          "raised exception class requires constructor arguments");
      return false;
    }
  }
  return true;
}

bool TypeChecker::checkTry(TryStmt& statement, const Type* expectedReturn) {
  bool ok = true;
  for (std::unique_ptr<Stmt>& item : statement.body()) {
    ok = checkStatement(*item, expectedReturn) && ok;
  }
  for (ExceptHandler& handler : statement.handlers()) {
    pushScope(handler.range);
    if (handler.type != nullptr) {
      const Type* type = resolveTypeExpr(*handler.type);
      auto isException = [](auto&& self, const Type* candidate) -> bool {
        if (candidate == nullptr || !candidate->isRecord())
          return false;
        if (candidate->name() == "Exception")
          return true;
        for (const Type* base : candidate->bases())
          if (self(self, base->canonical()))
            return true;
        return false;
      };
      if (type == nullptr || !isException(isException, type->canonical())) {
        diagnostics_->error(handler.range, "except requires an Exception subclass");
        ok = false;
      } else {
        handler.type->setResolvedType(type);
      }
      if (!handler.name.empty() && type != nullptr) {
        Symbol symbol;
        symbol.kind = SymbolKind::Variable;
        symbol.type = type;
        ok = declare(handler.name, symbol, handler.range.start) && ok;
      }
    }
    for (std::unique_ptr<Stmt>& item : handler.body) {
      ok = checkStatement(*item, expectedReturn) && ok;
    }
    popScope();
  }
  for (std::unique_ptr<Stmt>& item : statement.elseBody()) {
    ok = checkStatement(*item, expectedReturn) && ok;
  }
  for (std::unique_ptr<Stmt>& item : statement.finallyBody()) {
    ok = checkStatement(*item, expectedReturn) && ok;
  }
  return ok;
}

bool TypeChecker::checkMatch(MatchStmt& statement, const Type* expectedReturn) {
  const Type* subject = checkExpr(statement.subject());
  if (subject == nullptr) {
    return false;
  }
  bool ok = true;
  bool sawWildcard = false;
  std::unordered_map<std::string, bool> covered;
  if (subject->isEnum()) {
    for (const RecordField& field : subject->fields()) {
      if (field.isStatic) {
        covered[field.name] = false;
      }
    }
  }
  for (MatchArm& arm : statement.arms()) {
    const RecordField* payloadVariant = nullptr;
    if (arm.pattern != nullptr) {
      if (arm.pattern->kind() == NodeKind::NameExpr &&
          static_cast<NameExpr&>(*arm.pattern).name() == "_") {
        sawWildcard = true;
        arm.pattern->setResolvedType(subject);
      } else if (arm.pattern->kind() == NodeKind::CallExpr) {
        auto& call = static_cast<CallExpr&>(*arm.pattern);
        if (call.callee().kind() == NodeKind::MemberExpr) {
          auto& member = static_cast<MemberExpr&>(call.callee());
          const Type* enumType = checkExpr(member.object());
          if (enumType != nullptr) {
            enumType = enumType->canonical();
            if (enumType->isTypeObject() && enumType->typeObjectInstance() != nullptr) {
              enumType = enumType->typeObjectInstance()->canonical();
            }
          }
          if (enumType != nullptr && enumType->isEnum()) {
            covered[member.field()] = true;
            if (subject != nullptr && subject->isEnum() &&
                (subject == enumType || subject->name().starts_with(enumType->name()))) {
              payloadVariant = subject->findField(member.field());
            } else {
              payloadVariant = enumType->findField(member.field());
            }
            member.setResolvedType(enumType);
            call.setResolvedType(enumType);
            arm.pattern->setResolvedType(enumType);
          } else {
            ok = false;
          }
        } else {
          ok = checkExpr(*arm.pattern) != nullptr && ok;
        }
      } else {
        if (subject->isEnum() && arm.pattern->kind() == NodeKind::MemberExpr) {
          auto& member = static_cast<MemberExpr&>(*arm.pattern);
          const Type* objType = checkExpr(member.object());
          if (objType != nullptr) {
            objType = objType->canonical();
            if (objType->isTypeObject() && objType->typeObjectInstance() != nullptr) {
              objType = objType->typeObjectInstance()->canonical();
            }
          }
          if (objType != nullptr && objType->isEnum()) {
            covered[member.field()] = true;
            member.setResolvedType(objType);
            arm.pattern->setResolvedType(objType);
          } else {
            ok = false;
          }
        } else {
          const Type* patternType = checkExpr(*arm.pattern);
          if (patternType == nullptr) {
            ok = false;
          }
        }
      }
    }
    if (arm.guard != nullptr) {
      const Type* guard = checkExpr(*arm.guard);
      if (guard == nullptr || !guard->isNamed("bool")) {
        diagnostics_->error(arm.range, "match guard must be bool");
        ok = false;
      }
    }
    pushScope(arm.range);
    if (payloadVariant != nullptr && arm.pattern != nullptr &&
        arm.pattern->kind() == NodeKind::CallExpr) {
      auto& call = static_cast<CallExpr&>(*arm.pattern);
      for (std::size_t index = 0;
           index < call.arguments().size() && index < payloadVariant->payloadTypes.size();
           ++index) {
        if (call.arguments()[index]->kind() != NodeKind::NameExpr) {
          continue;
        }
        auto& binding = static_cast<NameExpr&>(*call.arguments()[index]);
        if (binding.name().empty() || binding.name() == "_") {
          continue;
        }
        Symbol symbol;
        symbol.kind = SymbolKind::Variable;
        symbol.type = payloadVariant->payloadTypes[index];
        binding.setResolvedType(symbol.type);
        ok = declare(binding.name(), symbol, binding.range().start) && ok;
      }
    }
    for (std::unique_ptr<Stmt>& item : arm.body) {
      ok = checkStatement(*item, expectedReturn) && ok;
    }
    popScope();
  }
  if (subject->isEnum() && !sawWildcard) {
    for (const auto& entry : covered) {
      if (!entry.second) {
        diagnostics_->error(statement.range(),
                            "match is not exhaustive; missing " + subject->name() + "." +
                                entry.first);
        ok = false;
      }
    }
  }
  return ok;
}

bool TypeChecker::checkDel(DelStmt& statement) {
  Expr& target = statement.target();
  const Type* type = checkExpr(target);
  if (type == nullptr) {
    return false;
  }
  if (target.kind() == NodeKind::IndexExpr) {
    const Type* object = static_cast<IndexExpr&>(target).object().resolvedType();
    if (object != nullptr && (object->isList() || object->isDict())) {
      return true;
    }
    diagnostics_->error(statement.range(), "del target must be a list or dict index");
    diagnostics_->help("write `del xs[i]` or `del table[key]`");
    return false;
  }
  diagnostics_->error(statement.range(), "del of a name is not supported");
  diagnostics_->help("assign a default value instead of deleting a binding");
  return false;
}

bool TypeChecker::checkDefer(DeferStmt& statement, const Type* expectedReturn) {
  bool ok = true;
  for (std::unique_ptr<Stmt>& item : statement.body()) {
    ok = checkStatement(*item, expectedReturn) && ok;
  }
  return ok;
}

bool TypeChecker::checkWith(WithStmt& statement, const Type* expectedReturn) {
  const Type* contextType = checkExpr(statement.context());
  if (contextType == nullptr) {
    return false;
  }
  contextType = contextType->canonical();
  if (contextType->methodIndex("__enter__") < 0 || contextType->methodIndex("__exit__") < 0) {
    diagnostics_->error(statement.context().range(),
                        quoteType(contextType) + " is not a context manager");
    diagnostics_->help("define __enter__ and __exit__ on the type used with `with`");
    return false;
  }
  const Type* entered = contextType->dunderReturn("__enter__");
  if (entered == nullptr) {
    entered = contextType;
  }
  bool ok = true;
  pushScope(statement.range());
  if (!statement.name().empty()) {
    Symbol symbol;
    symbol.kind = SymbolKind::Variable;
    symbol.type = entered;
    ok = declare(statement.name(), symbol, statement.range().start);
  }
  for (std::unique_ptr<Stmt>& item : statement.body()) {
    ok = checkStatement(*item, expectedReturn) && ok;
  }
  popScope();
  return ok;
}

const Type* TypeChecker::checkWalrus(WalrusExpr& expr) {
  const Type* value = checkExpr(expr.value());
  if (value == nullptr) {
    return nullptr;
  }
  if (isVoidLike(value)) {
    diagnostics_->error(expr.range(), "walrus assignment needs a value");
    return nullptr;
  }
  Symbol* existing = lookup(expr.name());
  if (existing != nullptr) {
    if (existing->readonly) {
      diagnostics_->error(expr.range(), "cannot assign to '" + expr.name() + "'");
      return nullptr;
    }
    if (existing->type != nullptr && !isAssignable(value, existing->type)) {
      diagnostics_->error(expr.range(),
                          "cannot assign " + quoteType(value) + " to '" + expr.name() + "'");
      return nullptr;
    }
  } else {
    NameExpr name(expr.range(), expr.name());
    if (!declareInferred(name, value, expr.range().start)) {
      return nullptr;
    }
  }
  expr.setResolvedType(value);
  return value;
}

const Type* TypeChecker::checkLambda(LambdaExpr& expr) {
  ++lambdaCounter_;
  expr.setLlvmName("__sere_lambda_" + std::to_string(lambdaCounter_));
  pushScope(expr.range());
  ++lambdaDepth_;
  if (!validateParamList(expr.params(), expr.range())) {
    --lambdaDepth_;
    popScope();
    return nullptr;
  }
  std::vector<const Type*> params;
  bool ok = true;
  for (ParamDecl& param : expr.params()) {
    const Type* type = resolveParamDeclType(param);
    if (type == nullptr) {
      ok = false;
      type = types_->anyType();
    }
    params.push_back(type);
    Symbol symbol;
    symbol.kind = SymbolKind::Variable;
    symbol.type = type;
    ok = declare(param.name, symbol, param.range.start) && ok;
  }
  const Type* bodyType = checkExpr(expr.body());
  --lambdaDepth_;
  popScope();
  if (!ok || bodyType == nullptr) {
    return nullptr;
  }
  const Type* returnType = bodyType;
  if (expr.returnType() != nullptr) {
    returnType = resolveTypeExpr(*expr.returnType());
    if (returnType == nullptr) {
      return nullptr;
    }
    if (!isAssignable(bodyType, returnType)) {
      diagnostics_->error(expr.body().range(),
                          "lambda body type " + quoteType(bodyType) + " does not match " +
                              quoteType(returnType));
      return nullptr;
    }
  }
  const Type* fnType = types_->functionType(params, returnType);
  expr.setResolvedType(fnType);
  return fnType;
}

const Type* TypeChecker::checkCallableCall(CallExpr& expr, const Type* constraint) {
  CallableShape shape;
  if (!fillCallableShape(constraint, shape)) {
    return nullptr;
  }
  if (!expr.keywordArguments().empty()) {
    diagnostics_->error(expr.range(), "keyword arguments are not supported for indirect calls");
    return nullptr;
  }
  if (shape.hasParams && !shape.variadic && expr.arguments().size() != shape.params.size()) {
    diagnostics_->error(expr.range(),
                        "call takes " + std::to_string(shape.params.size()) + " argument(s), but " +
                            std::to_string(expr.arguments().size()) + " provided");
    return nullptr;
  }
  if (shape.hasParams && shape.variadic && expr.arguments().size() < shape.params.size()) {
    diagnostics_->error(expr.range(),
                        "call takes at least " + std::to_string(shape.params.size()) +
                            " argument(s), but " + std::to_string(expr.arguments().size()) +
                            " provided");
    return nullptr;
  }
  for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
    const Type* argType = checkExpr(*expr.arguments()[index]);
    if (argType == nullptr) {
      return nullptr;
    }
    if (index < shape.params.size() && !isAssignable(argType, shape.params[index])) {
      diagnostics_->error(expr.arguments()[index]->range(),
                          "argument type mismatch: expected " + quoteType(shape.params[index]) +
                              ", found " + quoteType(argType));
      return nullptr;
    }
  }
  const Type* returnType =
      shape.hasReturn && shape.returnType != nullptr ? shape.returnType : types_->anyType();
  expr.callee().setResolvedType(constraint);
  expr.setResolvedType(returnType);
  return returnType;
}

const Type* TypeChecker::checkIndirectCall(CallExpr& expr, const Type* functionType) {
  if (functionType == nullptr || functionType->kind() != TypeKind::Function) {
    return nullptr;
  }
  if (!expr.keywordArguments().empty()) {
    diagnostics_->error(expr.range(), "keyword arguments are not supported for indirect calls");
    return nullptr;
  }
  if (expr.arguments().size() != functionType->paramTypes().size()) {
    diagnostics_->error(expr.range(),
                        "call takes " + std::to_string(functionType->paramTypes().size()) +
                            " argument(s), but " + std::to_string(expr.arguments().size()) +
                            " provided");
    return nullptr;
  }
  for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
    const Type* argType = checkExpr(*expr.arguments()[index]);
    if (argType == nullptr) {
      return nullptr;
    }
    if (!isAssignable(argType, functionType->paramTypes()[index])) {
      diagnostics_->error(expr.arguments()[index]->range(),
                          "argument type mismatch: expected " +
                              quoteType(functionType->paramTypes()[index]) + ", found " +
                              quoteType(argType));
      return nullptr;
    }
  }
  expr.callee().setResolvedType(functionType);
  expr.setResolvedType(functionType->returnType());
  return functionType->returnType();
}

bool TypeChecker::applyDecorators(Module& module) {
  bool ok = true;
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement == nullptr) {
      continue;
    }
    if (statement->kind() == NodeKind::FunctionDef) {
      ok = applyFunctionDecorators(static_cast<FunctionDef&>(*statement)) && ok;
    } else if (statement->kind() == NodeKind::ClassDef) {
      auto& classDef = static_cast<ClassDef&>(*statement);
      for (std::unique_ptr<FunctionDef>& method : classDef.methods()) {
        if (method != nullptr) {
          ok = applyFunctionDecorators(*method) && ok;
        }
      }
      ok = applyClassDecorators(classDef) && ok;
    } else if (statement->kind() == NodeKind::EnumDef) {
      for (std::unique_ptr<FunctionDef>& method : static_cast<EnumDef&>(*statement).methods()) {
        if (method != nullptr) {
          ok = applyFunctionDecorators(*method) && ok;
        }
      }
    }
  }
  return ok;
}

bool TypeChecker::applyFunctionDecorators(FunctionDef& function) {
  if (!hasRuntimeDecorators(function.decoratorExprs())) {
    return true;
  }
  const Type* current = function.resolvedType();
  if (current == nullptr) {
    diagnostics_->error(function.range(), "cannot decorate an untyped function");
    return false;
  }
  const Type* wrapped = applyDecoratorChain(function.decoratorExprs(), current, function.range());
  if (wrapped == nullptr) {
    return false;
  }
  function.setDecoratedType(wrapped);
  if (wrapped->kind() != TypeKind::Function && !wrapped->isCallableConstraint()) {
    if (!function.isMethod()) {
      if (Symbol* symbol = lookup(function.name())) {
        symbol->type = wrapped;
      }
    }
    return true;
  }
  if (wrapped->kind() == TypeKind::Function && !function.isMethod()) {
    if (Symbol* symbol = lookup(function.name())) {
      symbol->type = wrapped;
    }
    return true;
  }
  if (!function.isMethod()) {
    return true;
  }
  if (wrapped->kind() != TypeKind::Function) {
    return true;
  }
  const Type* record = classType(function.ownerClass());
  if (record == nullptr) {
    return true;
  }
  const int index = record->methodIndex(function.name());
  if (index < 0) {
    return true;
  }
  RecordMethod info = record->methods()[static_cast<std::size_t>(index)];
  info.type = wrapped;
  types_->replaceRecordMethod(record, std::move(info));
  return true;
}

bool TypeChecker::applyClassDecorators(ClassDef& classDef) {
  if (!hasRuntimeDecorators(classDef.decoratorExprs())) {
    return true;
  }
  const Type* record = classDef.resolvedType();
  if (record == nullptr) {
    diagnostics_->error(classDef.range(), "cannot decorate an untyped type");
    return false;
  }
  const Type* target = types_->typeObject(record);
  const Type* wrapped = applyDecoratorChain(classDef.decoratorExprs(), target, classDef.range());
  if (wrapped == nullptr) {
    return false;
  }
  if (wrapped->isAny()) {
    return true;
  }
  classDef.setDecoratedType(wrapped);
  Symbol* symbol = lookup(classDef.name());
  if (symbol == nullptr) {
    return true;
  }
  if (wrapped->isTypeObject()) {
    if (wrapped->typeObjectInstance() != nullptr) {
      symbol->type = wrapped->typeObjectInstance();
    }
    return true;
  }
  if (wrapped->kind() == TypeKind::Function || wrapped->isCallableConstraint()) {
    symbol->kind = SymbolKind::Function;
    symbol->type = wrapped;
    symbol->function = nullptr;
  } else {
    symbol->type = wrapped;
  }
  return true;
}

const Type* TypeChecker::applyDecoratorChain(std::vector<std::unique_ptr<Expr>>& exprs,
                                             const Type* target,
                                             SourceRange range) {
  const Type* current = target;
  (void)range;
  for (int index = static_cast<int>(exprs.size()) - 1; index >= 0; --index) {
    if (exprs[static_cast<std::size_t>(index)] == nullptr) {
      continue;
    }
    Expr& expr = *exprs[static_cast<std::size_t>(index)];
    if (isReservedDecoratorExpr(expr)) {
      continue;
    }
    const Type* wrapper = checkExpr(expr);
    if (wrapper == nullptr) {
      return nullptr;
    }
    current = callDecorator(wrapper, current, expr.range());
    if (current == nullptr) {
      return nullptr;
    }
  }
  return current;
}

const Type* TypeChecker::callDecorator(const Type* wrapper, const Type* target, SourceRange range) {
  if (wrapper == nullptr || target == nullptr) {
    diagnostics_->error(range, "decorator is not callable");
    return nullptr;
  }
  wrapper = wrapper->canonical();
  if (wrapper->isAny()) {
    return types_->anyType();
  }
  if (wrapper->kind() == TypeKind::Function) {
    if (wrapper->paramTypes().empty()) {
      diagnostics_->error(range, "decorator must accept the decorated object");
      diagnostics_->help("def decorator(obj): return obj");
      return nullptr;
    }
    if (!isAssignable(target, wrapper->paramTypes()[0]) && !wrapper->paramTypes()[0]->isAny()) {
      diagnostics_->error(range,
                          "decorator argument type mismatch: expected " +
                              quoteType(wrapper->paramTypes()[0]) + ", found " + quoteType(target));
      return nullptr;
    }
    return wrapper->returnType() == nullptr ? types_->anyType() : wrapper->returnType();
  }
  if (wrapper->isCallableConstraint()) {
    CallableShape shape;
    if (!fillCallableShape(wrapper, shape)) {
      return nullptr;
    }
    if (shape.hasParams && !shape.params.empty() && !shape.params[0]->isAny() &&
        !isAssignable(target, shape.params[0])) {
      diagnostics_->error(range,
                          "decorator argument type mismatch: expected " +
                              quoteType(shape.params[0]) + ", found " + quoteType(target));
      return nullptr;
    }
    // A decorator whose static signature is unknown (for example a factory such
    // as `@route("/x")` that returns a bare `Callable`) is treated as returning
    // the decorated object unchanged. This is the Python-like behaviour: the
    // decorated name keeps its original signature so existing call sites still
    // type-check, instead of degrading the symbol to `Any`.
    if (shape.hasReturn && shape.returnType != nullptr) {
      return shape.returnType;
    }
    return target;
  }
  diagnostics_->error(range, "decorator is not callable");
  diagnostics_->help("@name or @name(...) must evaluate to a callable");
  return nullptr;
}

bool TypeChecker::check(Module& module) {
  std::vector<std::unique_ptr<Stmt>>& statements = module.statements();
  statements.erase(
      std::remove_if(statements.begin(),
                     statements.end(),
                     [](const std::unique_ptr<Stmt>& item) { return item == nullptr; }),
      statements.end());
  injectModuleGlobals();
  if (!collectClassNames(module)) {
    return false;
  }
  if (!collectEnumNames(module)) {
    return false;
  }
  if (!collectAliases(module)) {
    return false;
  }
  if (!collectEnums(module)) {
    return false;
  }
  if (!collectClassFields(module)) {
    return false;
  }
  if (!collectFunctions(module)) {
    return false;
  }
  if (!collectMacros(module)) {
    return false;
  }
  if (!collectMethods(module)) {
    return false;
  }
  if (!bindLocalClassImports(module)) {
    return false;
  }
  if (!collectExports(module)) {
    return false;
  }
  if (!checkBodies(module)) {
    return false;
  }
  return !diagnostics_->hasErrors();
}

} // namespace sere
