/// @file TypeChecker.cpp
/// Semantic analysis for the typed Sere frontend.

#include "sere/sema/TypeChecker.h"

#include "sere/Version.h"
#include "sere/ast/Query.h"
#include "sere/diag/DiagnosticEngine.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sere {
namespace {

[[nodiscard]] const NameExpr* asName(const Expr& expr) {
  if (expr.kind() != NodeKind::NameExpr) {
    return nullptr;
  }
  return static_cast<const NameExpr*>(&expr);
}

[[nodiscard]] NameExpr* asName(Expr& expr) {
  return const_cast<NameExpr*>(asName(static_cast<const Expr&>(expr)));
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

void TypeChecker::pushScope() {
  scopes_.emplace_back();
}

void TypeChecker::popScope() {
  scopes_.pop_back();
}

bool TypeChecker::declare(const std::string& name,
                          Symbol symbol,
                          SourceLocation location,
                          bool navigable) {
  auto& scope = scopes_.back();
  if (scope.contains(name)) {
    diagnostics_->error(SourceRange{location, location}, "redeclaration of '" + name + "'");
    for (const SemanticSymbol& existing : symbols_) {
      if (existing.name == name) {
        diagnostics_->note(existing.location, "'" + name + "' previously declared here");
        break;
      }
    }
    return false;
  }
  SemanticSymbol collected;
  collected.name = name;
  collected.location = location;
  collected.range = SourceRange{location, location};
  collected.snippet = symbol.snippet;
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
  scope.emplace(name, symbol);
  return true;
}

const std::vector<SemanticSymbol>& TypeChecker::symbols() const {
  return symbols_;
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
        "void", "None", "Any", "bool", "i8",    "i16",  "i32",    "i64",    "u8",  "u16",  "u32",   "u64", "f32",
        "f64",  "str",  "regex", "byte", "Unique", "Shared", "Ptr", "list", "array", "dict"};
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
  const IntrinsicKind kinds[] = {
      IntrinsicKind::UniqueNew,  IntrinsicKind::SharedNew, IntrinsicKind::Alloc,
      IntrinsicKind::Free,       IntrinsicKind::Load,      IntrinsicKind::Store,
      IntrinsicKind::Len,        IntrinsicKind::Print,     IntrinsicKind::Str,
      IntrinsicKind::Append,     IntrinsicKind::ListNew,   IntrinsicKind::ArrayNew,
      IntrinsicKind::DictNew,    IntrinsicKind::Range,     IntrinsicKind::TypeOf,
      IntrinsicKind::IsInstance, IntrinsicKind::Dir,       IntrinsicKind::Inspect,
      IntrinsicKind::SizeOf,     IntrinsicKind::AlignOf,   IntrinsicKind::Panic,
      IntrinsicKind::Parse,      IntrinsicKind::TryParse,
  };
  for (const IntrinsicKind kind : kinds) {
    Symbol symbol;
    symbol.kind = SymbolKind::Intrinsic;
    symbol.intrinsic = kind;
    switch (kind) {
    case IntrinsicKind::Print:
      symbol.paramNames = {"*values"};
      break;
    case IntrinsicKind::Len:
      symbol.paramNames = {"items"};
      break;
    case IntrinsicKind::Str:
    case IntrinsicKind::TypeOf:
    case IntrinsicKind::Inspect:
    case IntrinsicKind::SizeOf:
    case IntrinsicKind::AlignOf:
    case IntrinsicKind::Panic:
    case IntrinsicKind::Free:
    case IntrinsicKind::Load:
    case IntrinsicKind::UniqueNew:
    case IntrinsicKind::SharedNew:
      symbol.paramNames = {"value"};
      break;
    case IntrinsicKind::Store:
      symbol.paramNames = {"pointer", "value"};
      break;
    case IntrinsicKind::Append:
      symbol.paramNames = {"value"};
      break;
    case IntrinsicKind::Range:
      symbol.paramNames = {"start", "stop", "step"};
      break;
    case IntrinsicKind::IsInstance:
      symbol.paramNames = {"value", "type"};
      break;
    case IntrinsicKind::Dir:
      symbol.paramNames = {"value"};
      break;
    case IntrinsicKind::Parse:
    case IntrinsicKind::TryParse:
      symbol.paramNames = {"text"};
      break;
    default:
      break;
    }
    if (!declare(std::string(intrinsicName(kind)), symbol, {}, false)) {
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
  if (to->isAny()) {
    return true;
  }
  if (from->isVoidLike() && to->isVoidLike()) {
    return true;
  }
  if (from->isNamed("null") && to->isPointerLike()) {
    return true;
  }
  if (from->isRecord() && to->isRecord() && from->isSubtypeOf(to)) {
    return true;
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
  if (from->isNamed("f32") && to->isNamed("f64")) {
    return true;
  }
  if ((from->isEnum() && to->isInteger()) || (from->isInteger() && to->isEnum())) {
    return false;
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
  if (name == "Unique" || name == "Shared" || name == "Ptr" || name == "list" || name == "array") {
    if (resolvedArgs.size() != 1) {
      diagnostics_->error(range, name + " requires exactly one type argument");
      return nullptr;
    }
    return types_->generic(name, resolvedArgs);
  }
  if (name == "dict") {
    if (resolvedArgs.size() != 2) {
      diagnostics_->error(range, "dict requires key and value type arguments");
      return nullptr;
    }
    return types_->generic(name, resolvedArgs);
  }
  if (const Type* param = types_->typeParam(name)) {
    if (!resolvedArgs.empty()) {
      diagnostics_->error(range, "type parameter '" + name + "' is not generic");
      return nullptr;
    }
    return param;
  }
  if (const Type* record = types_->record(name)) {
    if (!record->typeParams().empty()) {
      if (resolvedArgs.size() != record->typeParams().size()) {
        diagnostics_->error(range,
                            "'" + name + "' requires " +
                                std::to_string(record->typeParams().size()) + " type arguments");
        return nullptr;
      }
      return types_->instantiate(record, resolvedArgs);
    }
    if (!resolvedArgs.empty()) {
      diagnostics_->error(range, "type '" + name + "' is not generic");
      return nullptr;
    }
    return record;
  }
  if (Symbol* symbol = lookup(name);
      symbol != nullptr && symbol->type != nullptr &&
      (symbol->kind == SymbolKind::Class || symbol->kind == SymbolKind::Type)) {
    const Type* imported = symbol->type;
    if (!imported->typeParams().empty()) {
      if (resolvedArgs.size() != imported->typeParams().size()) {
        diagnostics_->error(range,
                            "'" + name + "' requires " +
                                std::to_string(imported->typeParams().size()) + " type arguments");
        return nullptr;
      }
      return types_->instantiate(imported, resolvedArgs);
    }
    if (!resolvedArgs.empty()) {
      diagnostics_->error(range, "type '" + name + "' is not generic");
      return nullptr;
    }
    return imported;
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

const Type* TypeChecker::resolveTypeExpr(const TypeExpr& expr) {
  if (expr.name() == "|") {
    std::vector<const Type*> members;
    for (const std::unique_ptr<TypeExpr>& arg : expr.args()) {
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
    expr.setResolvedType(types_->anyType());
    return types_->anyType();
  }
  if (symbol == nullptr && expr.name() == "super") {
    diagnostics_->error(expr.range(), "super must be called");
    diagnostics_->help("write super().__init__(...) or super().method(...)");
    return nullptr;
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
  expr.setResolvedType(symbol->type);
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
      diagnostics_->error(expr.range(),
                          "'" + expr.field() + "' is private and is not exported");
      return nullptr;
    }
    expr.setResolvedType(exportField->type);
    return exportField->type;
  }
  const RecordField* field = objectType->findField(expr.field());
  if (field == nullptr) {
    if (objectType->methodIndex(expr.field()) >= 0) {
      diagnostics_->error(expr.range(), "method '" + expr.field() + "' must be called");
      diagnostics_->help("write '" + expr.field() + "(...)'");
      return nullptr;
    }
    reportUnknownMember(*diagnostics_, expr.range(), objectType, expr.field(), false);
    return nullptr;
  }
  if (!field->isPublic && currentClass_ != objectType->name()) {
    diagnostics_->error(expr.range(),
                        "field '" + expr.field() + "' of '" + objectType->name() + "' is private");
    return nullptr;
  }
  if (isClassName(expr.object()) && !field->isStatic) {
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
    } else if (!isAssignable(itemType, element)) {
      const Type* joined = joinNumeric(*types_, element, itemType);
      if (joined == nullptr || !isAssignable(element, joined) || !isAssignable(itemType, joined)) {
        diagnostics_->error(item->range(),
                            "list elements must share a type, found " + quoteType(itemType) +
                                " then " + quoteType(element));
        return nullptr;
      }
      element = joined;
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
    if (!isAssignable(nextKey, keyType)) {
      const Type* joinedKey = joinNumeric(*types_, keyType, nextKey);
      if (joinedKey == nullptr || !isAssignable(keyType, joinedKey) ||
          !isAssignable(nextKey, joinedKey)) {
        diagnostics_->error(expr.keys()[index]->range(),
                            "dict entries must share key and value types");
        return nullptr;
      }
      keyType = joinedKey;
    }
    if (!isAssignable(nextValue, valueType)) {
      const Type* joinedValue = joinNumeric(*types_, valueType, nextValue);
      if (joinedValue == nullptr || !isAssignable(valueType, joinedValue) ||
          !isAssignable(nextValue, joinedValue)) {
        diagnostics_->error(expr.keys()[index]->range(),
                            "dict entries must share key and value types");
        return nullptr;
      }
      valueType = joinedValue;
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
  if (init.kind() == NodeKind::ListLiteral && dest->isSequence()) {
    auto& literal = static_cast<ListLiteral&>(init);
    for (const std::unique_ptr<Expr>& item : literal.elements()) {
      const Type* itemType = checkExpr(*item);
      if (itemType == nullptr || !isAssignable(itemType, dest->elementType())) {
        diagnostics_->error(item->range(),
                            "cannot store " + quoteType(itemType) + " in " + quoteType(dest));
        return false;
      }
    }
    init.setResolvedType(dest);
    return true;
  }
  if (init.kind() == NodeKind::DictLiteral && dest->isDict()) {
    auto& literal = static_cast<DictLiteral&>(init);
    for (std::size_t index = 0; index < literal.keys().size(); ++index) {
      const Type* keyType = checkExpr(*literal.keys()[index]);
      const Type* valueType = checkExpr(*literal.values()[index]);
      if (keyType == nullptr || valueType == nullptr ||
          !isAssignable(keyType, dest->dictKeyType()) ||
          !isAssignable(valueType, dest->dictValueType())) {
        diagnostics_->error(literal.keys()[index]->range(),
                            "cannot store this entry in " + quoteType(dest));
        return false;
      }
    }
    init.setResolvedType(dest);
    return true;
  }
  return false;
}

const Type* TypeChecker::rewriteDunderBinary(BinaryExpr& expr, const Type* left,
                                             const Type* right) {
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
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  }
  if (op == BinaryOp::In || op == BinaryOp::NotIn) {
    if (right->isNamed("str") || right->isSequence() || right->isDict() ||
        right->methodIndex("__contains__") >= 0 ||
        (right->isEnum() && right->isFlags() && left->isEnum() &&
         left->canonical() == right->canonical())) {
      expr.setResolvedType(types_->boolType());
      return types_->boolType();
    }
    diagnostics_->error(expr.range(), quoteType(right) + " does not support 'in'");
    return nullptr;
  }
  if (const Type* overloaded = rewriteDunderBinary(expr, left, right)) {
    return overloaded;
  }
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
    if (!(same || ints || floats || mixedNum || strs || enums)) {
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
      if (!(left->isEnum() && right->isEnum() && left->canonical() == right->canonical())) {
        diagnostics_->error(expr.range(), "bitwise operators require integers");
        return nullptr;
      }
      expr.setResolvedType(left);
      return left;
    }
    const Type* result = left->integerBitWidth() >= right->integerBitWidth() ? left : right;
    expr.setResolvedType(result);
    return result;
  }
  const bool leftNum = left->isInteger() || left->isFloat();
  const bool rightNum = right->isInteger() || right->isFloat();
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
  if (expr.op() == UnaryOp::Not) {
    if (!operand->isNamed("bool") && operand->methodIndex("__bool__") < 0) {
      diagnostics_->error(expr.range(), "'not' requires a bool, found " + quoteType(operand));
      return nullptr;
    }
    expr.setResolvedType(types_->boolType());
    return types_->boolType();
  }
  if (expr.op() == UnaryOp::Invert) {
    if (!operand->isInteger() && operand->methodIndex("__invert__") < 0) {
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

const Type* TypeChecker::checkDeref(UnaryExpr& expr, const Type* operand) {
  if (!operand->isPointerLike()) {
    diagnostics_->error(expr.range(),
                        "unary '*' requires Unique[T], Shared[T], or Ptr[T], found " +
                            quoteType(operand));
    return nullptr;
  }
  const Type* pointee = operand->pointeeType();
  if (pointee == nullptr) {
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
    if (symbol != nullptr && symbol->kind != SymbolKind::Variable) {
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
    for (std::unique_ptr<Stmt>& bodyStmt : branch.body) {
      ok = checkStatement(*bodyStmt, expectedReturn) && ok;
    }
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
  pushScope();
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
  pushScope();
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
      const NameExpr* typeName = asName(*expr.arguments()[argIndex]);
      if (typeName == nullptr) {
        diagnostics_->error(expr.arguments()[argIndex]->range(),
                            "isinstance() type argument must be a type name");
        return nullptr;
      }
      const Type* target =
          resolveNamedType(typeName->name(), {}, expr.arguments()[argIndex]->range(), true);
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
    if (typeArgs.size() != 1 || !valueTypes.empty()) {
      diagnostics_->error(expr.range(), "alloc[T]() takes one type argument");
      return nullptr;
    }
    result = types_->ptrType(typeArgs[0]);
  } else if (kind == IntrinsicKind::Free) {
    if (valueTypes.size() != 1 || !valueTypes[0]->isPointerLike()) {
      diagnostics_->error(expr.range(), "free() requires Unique[T], Shared[T], or Ptr[T]");
      return nullptr;
    }
    result = types_->voidType();
  } else if (kind == IntrinsicKind::Load) {
    if (valueTypes.size() != 1 || !valueTypes[0]->isPointerLike()) {
      diagnostics_->error(expr.range(), "load() requires Unique[T], Shared[T], or Ptr[T]");
      return nullptr;
    }
    result = valueTypes[0]->pointeeType();
  } else if (kind == IntrinsicKind::Store) {
    if (valueTypes.size() != 2 || !valueTypes[0]->isPointerLike() ||
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
    if (!(valueTypes[0]->isSequence() || valueTypes[0]->isDict() || valueTypes[0]->isStrLayout() ||
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
    for (std::size_t index = 0; index < valueTypes.size(); ++index) {
      if (!isPrintable(valueTypes[index])) {
        diagnostics_->error(expr.arguments()[index]->range(),
                            "print() cannot print values of type " + quoteType(valueTypes[index]));
        return nullptr;
      }
    }
    result = types_->voidType();
  } else if (kind == IntrinsicKind::Str) {
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
                          std::string(intrinsicName(kind)) + "[T](text) takes one type and one string");
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
    result = kind == IntrinsicKind::TryParse
                 ? types_->unionType({typeArgs[0], types_->noneType()})
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
      if (expected != nullptr && expected->isTypeParam() && !subst.contains(expected->name())) {
        subst[expected->name()] = argType;
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
  const Type* record = types_->record(currentClass_);
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
    if (calleeType->kind() != TypeKind::Function) {
      diagnostics_->error(expr.range(), "callee is not a function");
      diagnostics_->help("a lambda or function value is required here");
      return nullptr;
    }
    return checkIndirectCall(expr, calleeType);
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
  if (expr.arguments().size() == 1 && (symbol == nullptr || symbol->kind == SymbolKind::Type)) {
    const Type* target = resolveNamedType(name->name(), expr.typeArgs(), expr.range(), false);
    if (target != nullptr) {
      const Type* result = checkCastValue(*expr.arguments()[0], target, expr.range());
      expr.setCast(true);
      expr.setResolvedType(result);
      return result;
    }
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
    const Type* calleeType = symbol->type->canonical();
    if (calleeType->kind() == TypeKind::Function) {
      expr.callee().setResolvedType(calleeType);
      return checkIndirectCall(expr, calleeType);
    }
    diagnostics_->error(expr.range(), "'" + name->name() + "' is not a function");
    return nullptr;
  }
  const Type* functionType = specializeCall(expr, *symbol);
  if (functionType == nullptr) {
    return nullptr;
  }
  std::size_t required = functionType->paramTypes().size();
  if (symbol->function != nullptr) {
    required = 0;
    bool sawDefault = false;
    for (const ParamDecl& param : symbol->function->params()) {
      if (param.defaultValue != nullptr) {
        sawDefault = true;
      } else if (sawDefault) {
        diagnostics_->error(param.range, "parameter without a default follows a default");
        return nullptr;
      } else {
        ++required;
      }
    }
  }
  if (symbol->function != nullptr) {
    std::vector<std::string> names;
    for (const ParamDecl& param : symbol->function->params()) {
      names.push_back(param.name);
    }
    expr.setParamNames(std::move(names));
  }
  if (expr.arguments().size() < required ||
      expr.arguments().size() > functionType->paramTypes().size()) {
    diagnostics_->error(expr.range(),
                        "'" + name->name() + "' takes " +
                            countLabel(functionType->paramTypes().size(), "argument", "arguments") +
                            ", but " + std::to_string(expr.arguments().size()) + " provided");
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
  if (symbol->function != nullptr) {
    std::vector<std::string> names;
    for (const ParamDecl& param : symbol->function->params()) {
      names.push_back(param.name);
    }
    expr.setParamNames(std::move(names));
  }
  if (expr.loweredName().empty()) {
    expr.setLoweredName(loweredCallName(*name, *symbol));
  }
  expr.callee().setResolvedType(functionType);
  expr.setResolvedType(functionType->returnType());
  return functionType->returnType();
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
    diagnostics_->error(expr.range(), "cannot construct abstract class '" + record->name() + "'");
    return nullptr;
  }
  const int initIndex = record->methodIndex("__init__");
  if (initIndex >= 0) {
    const RecordMethod& init = record->methods()[static_cast<std::size_t>(initIndex)];
    const Type* functionType = init.type;
    if (functionType == nullptr || functionType->paramTypes().empty()) {
      return nullptr;
    }
    if (expr.arguments().size() < init.requiredAfterSelf ||
        expr.arguments().size() + 1 > functionType->paramTypes().size()) {
      diagnostics_->error(
          expr.range(),
          "'" + record->name() + "' takes " +
              countLabel(functionType->paramTypes().size() - 1, "argument", "arguments") +
              ", but " + std::to_string(expr.arguments().size()) + " provided");
      return nullptr;
    }
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
  std::vector<const RecordField*> instanceFields;
  for (const RecordField& field : record->fields()) {
    if (!field.isStatic) {
      instanceFields.push_back(&field);
    }
  }
  if (!expr.arguments().empty() && expr.arguments().size() != instanceFields.size()) {
    diagnostics_->error(expr.range(),
                        "'" + record->name() + "' takes " +
                            countLabel(instanceFields.size(), "argument", "arguments") + ", but " +
                            std::to_string(expr.arguments().size()) + " provided");
    return nullptr;
  }
  std::vector<std::string> names;
  for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
    const Type* argType = checkExpr(*expr.arguments()[index]);
    if (argType == nullptr) {
      return nullptr;
    }
    if (!isAssignable(argType, instanceFields[index]->type)) {
      diagnostics_->error(expr.arguments()[index]->range(),
                          "constructor argument type mismatch: expected " +
                              quoteType(instanceFields[index]->type) + ", found " +
                              quoteType(argType));
      return nullptr;
    }
    names.push_back(instanceFields[index]->name);
  }
  expr.setParamNames(std::move(names));
  expr.setConstructor(true);
  expr.callee().setResolvedType(record);
  expr.setResolvedType(record);
  return record;
}

const Type* TypeChecker::checkMethodCall(CallExpr& expr) {
  auto& member = static_cast<MemberExpr&>(expr.callee());
  const Type* objectType = checkExpr(member.object());
  if (objectType != nullptr && objectType->isModule()) {
    const RecordField* exported = objectType->findField(member.field());
    if (exported != nullptr && !exported->isPublic) {
      diagnostics_->error(expr.range(),
                          "'" + member.field() + "' is private and is not exported");
      return nullptr;
    }
    if (exported != nullptr && exported->type != nullptr && exported->type->isRecord()) {
      return checkConstructor(expr, exported->type);
    }
    if (exported == nullptr || exported->type == nullptr ||
        exported->type->kind() != TypeKind::Function) {
      diagnostics_->error(expr.range(), "unknown export '" + member.field() + "'");
      return nullptr;
    }
    const Type* functionType = exported->type;
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
    expr.setLoweredName(exported->llvmName.empty() ? member.field() : exported->llvmName);
    expr.setParamNames(exported->paramNames);
    member.setResolvedType(functionType);
    expr.setResolvedType(functionType->returnType());
    return functionType->returnType();
  }
  if (objectType != nullptr && objectType->isList() && member.field() == "append") {
    if (expr.arguments().size() != 1) {
      diagnostics_->error(expr.range(), "append() takes one argument");
      return nullptr;
    }
    const Type* valueType = checkExpr(*expr.arguments()[0]);
    if (valueType == nullptr || !isAssignable(valueType, objectType->elementType())) {
      diagnostics_->error(expr.range(),
                          "cannot append " + quoteType(valueType) + " to " + quoteType(objectType));
      return nullptr;
    }
    expr.setIntrinsic(IntrinsicKind::Append);
    expr.setResolvedType(types_->voidType());
    return types_->voidType();
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
      for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
        const Type* argType = checkExpr(*expr.arguments()[index]);
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
  const int index = objectType->methodIndex(member.field());
  if (index < 0) {
    reportUnknownMember(*diagnostics_, expr.range(), objectType, member.field(), true);
    return nullptr;
  }
  const RecordMethod& method = objectType->methods()[static_cast<std::size_t>(index)];
  if (!method.isPublic && currentClass_ != objectType->name()) {
    diagnostics_->error(expr.range(),
                        "method '" + member.field() + "' of '" + objectType->name() +
                            "' is private");
    return nullptr;
  }
  const Type* functionType = method.type;
  expr.setMethod(true);
  expr.setLoweredName(method.llvmName);
  std::vector<std::string> names;
  for (std::size_t index = 1; index < method.paramNames.size(); ++index) {
    names.push_back(method.paramNames[index]);
  }
  expr.setParamNames(std::move(names));
  if (functionType == nullptr || functionType->paramTypes().empty()) {
    return nullptr;
  }
  if (expr.arguments().size() < method.requiredAfterSelf ||
      expr.arguments().size() + 1 > functionType->paramTypes().size()) {
    diagnostics_->error(
        expr.range(),
        "'" + member.field() + "' takes " +
            countLabel(functionType->paramTypes().size() - 1, "argument", "arguments") + ", but " +
            std::to_string(expr.arguments().size()) + " provided");
    return nullptr;
  }
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
  expr.setMethod(true);
  expr.setLoweredName(method.llvmName);
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
                              "cannot initialize '" + decl.name() + "' with " + quoteType(initType) +
                                  ", expected " + quoteType(type));
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
      diagnostics_->error(statement.range(), "tuple unpack expected " +
                                                 std::to_string(targets.elements().size()) +
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
      Symbol* existing = lookup(name.name());
      if (existing != nullptr) {
        if (existing->readonly) {
          diagnostics_->error(item.range(), "cannot assign to '" + name.name() + "'");
          ok = false;
          continue;
        }
        name.setResolvedType(existing->type);
        if (elemType != nullptr && existing->type != nullptr &&
            !isAssignable(elemType, existing->type)) {
          diagnostics_->error(item.range(), "cannot unpack " + quoteType(elemType) + " into '" +
                                                name.name() + "'");
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
    Symbol* symbol = lookup(name.name());
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
    if (objectType != nullptr && objectType->isFrozen() && currentFunctionName_ != "__init__") {
      diagnostics_->error(statement.range(),
                          "cannot assign to frozen field '" + member.field() + "'");
      return false;
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
  return true;
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
  const Type* actual = checkExpr(const_cast<Expr&>(*statement.value()));
  if (actual == nullptr) {
    return false;
  }
  if (expectedReturn->isVoidLike()) {
    if (!isVoidLike(actual)) {
      diagnostics_->error(statement.value()->range(),
                          "return type mismatch: expected 'void', found " + quoteType(actual));
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
  return true;
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
  case NodeKind::PassStmt:
  case NodeKind::FunctionDef:
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
  if (function.isExtern() || function.isAbstract()) {
    return true;
  }
  currentClass_ = function.ownerClass();
  currentFunctionName_ = function.name();
  pushScope();
  for (std::size_t index = 0; index < function.params().size(); ++index) {
    const ParamDecl& param = function.params()[index];
    const Type* type = resolveParamType(function, index);
    Symbol symbol;
    symbol.kind = SymbolKind::Variable;
    symbol.type = type;
    if (type == nullptr || !declare(param.name, symbol, param.range.start)) {
      popScope();
      currentClass_.clear();
      return false;
    }
    if (param.defaultValue != nullptr) {
      const Type* defaultType = checkExpr(*param.defaultValue);
      if (defaultType == nullptr || !isAssignable(defaultType, type)) {
        diagnostics_->error(param.range, "default value type mismatch for '" + param.name + "'");
        popScope();
        currentClass_.clear();
        return false;
      }
    }
  }
  const Type* returnType = resolveTypeExpr(function.returnType());
  bool ok = returnType != nullptr;
  for (const std::unique_ptr<Stmt>& statement : function.body()) {
    if (statement == nullptr) {
      continue;
    }
    ok = checkStatement(*statement, returnType) && ok;
  }
  popScope();
  currentClass_.clear();
  return ok;
}

const Type* TypeChecker::resolveParamType(const FunctionDef& function, std::size_t index) {
  const ParamDecl& param = function.params()[index];
  if (function.isMethod() && index == 0) {
    if (param.name != "self") {
      diagnostics_->error(param.range, "first method parameter must be named self");
      return nullptr;
    }
    return types_->record(function.ownerClass());
  }
  if (param.type == nullptr) {
    return types_->anyType();
  }
  return resolveTypeExpr(*param.type);
}

bool TypeChecker::collectClassNames(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::ClassDef) {
      continue;
    }
    auto& classDef = static_cast<ClassDef&>(*statement);
    classes_[classDef.name()] = &classDef;
    const Type* record = types_->defineRecord(classDef.name(), {});
    types_->setRecordTypeParams(record, classDef.typeParams());
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

bool TypeChecker::collectEnums(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::EnumDef) {
      continue;
    }
    auto& enumDef = static_cast<EnumDef&>(*statement);
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
    const Type* record = types_->defineRecord(enumDef.name(), {});
    types_->setRecordEnum(record, true);
    types_->setRecordFlags(record, enumDef.isFlags());
    for (RecordField& field : fields) {
      field.type = record;
    }
    types_->setRecordFields(record, std::move(fields));
    enumDef.setResolvedType(record);
    Symbol symbol;
    symbol.kind = SymbolKind::Class;
    symbol.type = record;
    if (!declare(enumDef.name(), symbol, enumDef.range().start, !enumDef.fromPrelude())) {
      return false;
    }
    for (const EnumVariant& variant : enumDef.variants()) {
      recordSymbol(variant.name, "enumMember", record, variant.range.start, enumDef.name());
    }
  }
  return true;
}

bool TypeChecker::collectAliases(Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::TypeAlias) {
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
  if (flattened_[classDef.name()]) {
    return true;
  }
  flattened_[classDef.name()] = true;
  std::vector<RecordField> fields;
  std::vector<const Type*> bases;
  for (const std::string& baseName : classDef.bases()) {
    if (classDef.isStruct()) {
      diagnostics_->error(classDef.range(), "structs cannot inherit; use class");
      return false;
    }
    const auto found = classes_.find(baseName);
    if (found != classes_.end() && !flattenClass(*found->second)) {
      return false;
    }
    const Type* base = types_->record(baseName);
    if (base == nullptr) {
      diagnostics_->error(classDef.range(), "unknown base class '" + baseName + "'");
      return false;
    }
    bases.push_back(base);
    for (const RecordField& field : base->fields()) {
      bool exists = false;
      for (const RecordField& seen : fields) {
        exists = exists || seen.name == field.name;
      }
      if (!exists) {
        fields.push_back(field);
      }
    }
  }
  for (const FieldDecl& field : classDef.fields()) {
    const Type* type = resolveTypeExpr(*field.type);
    if (type == nullptr) {
      return false;
    }
    bool replaced = false;
    for (RecordField& existing : fields) {
      if (existing.name == field.name) {
        existing = RecordField{field.name, type, field.isPublic, field.isStatic};
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      fields.push_back(RecordField{field.name, type, field.isPublic, field.isStatic});
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
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::FunctionDef) {
      continue;
    }
    auto& function = static_cast<FunctionDef&>(*statement);
    for (const std::string& param : function.typeParams()) {
      (void)types_->defineTypeParam(param);
    }
    std::vector<const Type*> params;
    for (const ParamDecl& param : function.params()) {
      const Type* type = param.type == nullptr ? types_->anyType() : resolveTypeExpr(*param.type);
      if (type == nullptr) {
        return false;
      }
      params.push_back(type);
    }
    const Type* returnType = resolveTypeExpr(function.returnType());
    if (returnType == nullptr) {
      return false;
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
      return false;
    }
  }
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
    for (std::unique_ptr<FunctionDef>& method : classDef.methods()) {
      if (method->params().empty() || method->params()[0].name != "self") {
        diagnostics_->error(method->range(), "methods must take self as the first parameter");
        return false;
      }
      std::vector<const Type*> params;
      for (std::size_t index = 0; index < method->params().size(); ++index) {
        const Type* type = resolveParamType(*method, index);
        if (type == nullptr) {
          return false;
        }
        params.push_back(type);
      }
      const Type* returnType = resolveTypeExpr(method->returnType());
      if (returnType == nullptr) {
        return false;
      }
      if (method->name() == "__init__" && !returnType->isNamed("void")) {
        diagnostics_->error(method->range(), "__init__ must return void");
        return false;
      }
      if (method->name() == "__str__") {
        if (params.size() != 1) {
          diagnostics_->error(method->range(), "__str__ takes only self");
          return false;
        }
        if (!returnType->isNamed("str")) {
          diagnostics_->error(method->range(), "__str__ must return str");
          return false;
        }
      }
      if (method->name() == "__repr__" && !returnType->isNamed("str")) {
        diagnostics_->error(method->range(), "__repr__ must return str");
        return false;
      }
      if (method->name() == "__len__" && (params.size() != 1 || !returnType->isNamed("i64"))) {
        diagnostics_->error(method->range(), "__len__ must be def __len__(self) -> i64");
        return false;
      }
      if (method->name() == "__bool__" && (params.size() != 1 || !returnType->isNamed("bool"))) {
        diagnostics_->error(method->range(), "__bool__ must be def __bool__(self) -> bool");
        return false;
      }
      if (method->name() == "__contains__" &&
          (params.size() != 2 || !returnType->isNamed("bool"))) {
        diagnostics_->error(method->range(),
                            "__contains__ must be def __contains__(self, item: T) -> bool");
        return false;
      }
      if (method->name() == "__getitem__" && params.size() != 2) {
        diagnostics_->error(method->range(),
                            "__getitem__ must be def __getitem__(self, index: T) -> U");
        return false;
      }
      if (method->name() == "__setitem__" && (params.size() != 3 || !returnType->isNamed("void"))) {
        diagnostics_->error(
            method->range(),
            "__setitem__ must be def __setitem__(self, index: K, value: V) -> void");
        return false;
      }
      const Type* fnType = types_->functionType(params, returnType);
      method->setResolvedType(fnType);
      RecordMethod info;
      info.name = method->name();
      info.type = fnType;
      info.llvmName = classDef.name() + "_" + method->name();
      info.isAbstract = method->isAbstract();
      info.isPublic = !method->isPrivate();
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
          return false;
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
  }
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() != NodeKind::ClassDef) {
      continue;
    }
    auto& classDef = static_cast<ClassDef&>(*statement);
    const Type* record = classDef.resolvedType();
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
    for (std::unique_ptr<FunctionDef>& method : enumDef.methods()) {
      if (method->params().empty() || method->params()[0].name != "self") {
        diagnostics_->error(method->range(), "methods must take self as the first parameter");
        return false;
      }
      std::vector<const Type*> params;
      for (std::size_t index = 0; index < method->params().size(); ++index) {
        const Type* type = resolveParamType(*method, index);
        if (type == nullptr) {
          return false;
        }
        params.push_back(type);
      }
      const Type* returnType = resolveTypeExpr(method->returnType());
      if (returnType == nullptr) {
        return false;
      }
      const Type* fnType = types_->functionType(params, returnType);
      method->setResolvedType(fnType);
      RecordMethod info;
      info.name = method->name();
      info.type = fnType;
      info.llvmName = enumDef.name() + "_" + method->name();
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

bool TypeChecker::checkBodies(Module& module) {
  bool ok = true;
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() == NodeKind::VarDecl) {
      ok = checkVarDecl(static_cast<VarDecl&>(*statement)) && ok;
    } else if (statement->kind() == NodeKind::AssignStmt) {
      ok = checkAssign(static_cast<AssignStmt&>(*statement)) && ok;
    }
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
      for (std::unique_ptr<FunctionDef>& method : static_cast<ClassDef&>(*statement).methods()) {
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
  return checkExpr(const_cast<Expr&>(*statement.value())) != nullptr;
}

bool TypeChecker::checkTry(TryStmt& statement, const Type* expectedReturn) {
  bool ok = true;
  for (std::unique_ptr<Stmt>& item : statement.body()) {
    ok = checkStatement(*item, expectedReturn) && ok;
  }
  for (ExceptHandler& handler : statement.handlers()) {
    pushScope();
    if (handler.type != nullptr) {
      const Type* type = resolveTypeExpr(*handler.type);
      if (type != nullptr) {
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
          if (enumType != nullptr && enumType->isEnum()) {
            covered[member.field()] = true;
            payloadVariant = enumType->findField(member.field());
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
        const Type* patternType = checkExpr(*arm.pattern);
        if (patternType == nullptr) {
          ok = false;
        } else if (subject->isEnum() && arm.pattern->kind() == NodeKind::MemberExpr) {
          const auto& member = static_cast<MemberExpr&>(*arm.pattern);
          covered[member.field()] = true;
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
    pushScope();
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
  pushScope();
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
      diagnostics_->error(expr.range(), "cannot assign " + quoteType(value) + " to '" +
                                            expr.name() + "'");
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
  pushScope();
  ++lambdaDepth_;
  std::vector<const Type*> params;
  bool ok = true;
  for (ParamDecl& param : expr.params()) {
    const Type* type = param.type == nullptr ? types_->anyType() : resolveTypeExpr(*param.type);
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
      diagnostics_->error(expr.body().range(), "lambda body type " + quoteType(bodyType) +
                                                   " does not match " + quoteType(returnType));
      return nullptr;
    }
  }
  const Type* fnType = types_->functionType(params, returnType);
  expr.setResolvedType(fnType);
  return fnType;
}

const Type* TypeChecker::checkIndirectCall(CallExpr& expr, const Type* functionType) {
  if (functionType == nullptr || functionType->kind() != TypeKind::Function) {
    return nullptr;
  }
  if (expr.arguments().size() != functionType->paramTypes().size()) {
    diagnostics_->error(expr.range(),
                        "call takes " +
                            std::to_string(functionType->paramTypes().size()) +
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

bool TypeChecker::check(Module& module) {
  std::vector<std::unique_ptr<Stmt>>& statements = module.statements();
  statements.erase(std::remove_if(statements.begin(), statements.end(),
                                  [](const std::unique_ptr<Stmt>& item) { return item == nullptr; }),
                   statements.end());
  injectModuleGlobals();
  return collectClassNames(module) && collectEnums(module) && collectAliases(module) &&
         collectClassFields(module) && collectFunctions(module) && collectMacros(module) &&
         collectMethods(module) && checkBodies(module) && !diagnostics_->hasErrors();
}

} // namespace sere
