/// @file AstBridge.cpp
/// C ABI over the compiler's lexer, parser, and syntax AST (see sere_ast.h).
///
/// A tree owns the parsed module, the source text, and every handle it hands
/// out, so a caller keeps walking handles until it frees the tree. One table
/// entry per node kind describes that node's syntax fields in source order; the
/// table is the only place that knows the tree's shape, which keeps the stdlib
/// `ast` module free of per-node boilerplate and lets a node kind that the
/// compiler gains later show up in `ast` as soon as it is listed here.

#include "sere/api/sere_ast.h"

#include "sere/ast/Syntax.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sere {
namespace {

// ---------------------------------------------------------------------------
// Field values
// ---------------------------------------------------------------------------

/// Syntax that is not itself a node: a parameter, case arm, handler, keyword
/// argument, interpolated part, class field, or enum variant. These live in
/// vectors owned by their parent node, so a pointer to one stays valid as long
/// as the tree does.
struct HelperRef {
  const void* pointer = nullptr;
  int32_t kind = 0;
};

enum HelperKind {
  kHelperNone = 0,
  kHelperPart,
  kHelperKeyword,
  kHelperParam,
  kHelperIfBranch,
  kHelperHandler,
  kHelperCase,
  kHelperField,
  kHelperVariant,
  kHelperPayload,
  kHelperMacroArm,
};

struct FieldValue {
  int32_t kind = SERE_AST_VALUE_NONE;
  const Node* node = nullptr;
  std::vector<const Node*> nodes;
  std::vector<HelperRef> helpers;
  std::string text;
  std::vector<std::string> texts;
  int64_t integer = 0;
  double real = 0.0;
  bool boolean = false;
  bool present = false;
  /// Set once the value has been materialised into a handle's field cache.
  bool read = false;
};

using FieldGetter = FieldValue (*)(const Node&);

template <typename T> struct IsStdVector : std::false_type {};
template <typename E, typename A> struct IsStdVector<std::vector<E, A>> : std::true_type {};

template <typename T> struct IsUniquePtrToNode : std::false_type {};
template <typename E, typename D>
struct IsUniquePtrToNode<std::unique_ptr<E, D>> : std::bool_constant<std::is_base_of_v<Node, E>> {};

// Node lists, text lists, and helper lists arrive as vectors with different
// element types, so each shape gets one appender that `makeValue` dispatches to.

void appendVector(const std::vector<std::unique_ptr<Expr>>& list, FieldValue& out) {
  out.kind = SERE_AST_VALUE_NODES;
  out.present = true;
  out.nodes.reserve(list.size());
  for (const std::unique_ptr<Expr>& item : list)
    out.nodes.push_back(item.get());
}

void appendVector(const std::vector<std::unique_ptr<Stmt>>& list, FieldValue& out) {
  out.kind = SERE_AST_VALUE_NODES;
  out.present = true;
  out.nodes.reserve(list.size());
  for (const std::unique_ptr<Stmt>& item : list)
    out.nodes.push_back(item.get());
}

void appendVector(const std::vector<std::unique_ptr<TypeExpr>>& list, FieldValue& out) {
  out.kind = SERE_AST_VALUE_NODES;
  out.present = true;
  out.nodes.reserve(list.size());
  for (const std::unique_ptr<TypeExpr>& item : list)
    out.nodes.push_back(item.get());
}

/// Methods of a class, enum, or type alias are statements like any other.
void appendVector(const std::vector<std::unique_ptr<FunctionDef>>& list, FieldValue& out) {
  out.kind = SERE_AST_VALUE_NODES;
  out.present = true;
  out.nodes.reserve(list.size());
  for (const std::unique_ptr<FunctionDef>& item : list)
    out.nodes.push_back(item.get());
}

void appendVector(const std::vector<std::string>& list, FieldValue& out) {
  out.kind = SERE_AST_VALUE_TEXTS;
  out.texts = list;
  out.present = true;
}

template <typename T>
void appendHelpers(const std::vector<T>& list, int32_t kind, FieldValue& out) {
  out.kind = SERE_AST_VALUE_NODES;
  out.present = true;
  out.helpers.reserve(list.size());
  for (const T& item : list)
    out.helpers.push_back(HelperRef{&item, kind});
}

void appendVector(const std::vector<ParamDecl>& list, FieldValue& out) {
  appendHelpers(list, kHelperParam, out);
}

void appendVector(const std::vector<StringPart>& list, FieldValue& out) {
  appendHelpers(list, kHelperPart, out);
}

void appendVector(const std::vector<NamedArgument>& list, FieldValue& out) {
  appendHelpers(list, kHelperKeyword, out);
}

void appendVector(const std::vector<IfBranch>& list, FieldValue& out) {
  appendHelpers(list, kHelperIfBranch, out);
}

void appendVector(const std::vector<ExceptHandler>& list, FieldValue& out) {
  appendHelpers(list, kHelperHandler, out);
}

void appendVector(const std::vector<MatchArm>& list, FieldValue& out) {
  appendHelpers(list, kHelperCase, out);
}

void appendVector(const std::vector<FieldDecl>& list, FieldValue& out) {
  appendHelpers(list, kHelperField, out);
}

void appendVector(const std::vector<EnumVariant>& list, FieldValue& out) {
  appendHelpers(list, kHelperVariant, out);
}

void appendVector(const std::vector<EnumPayloadField>& list, FieldValue& out) {
  appendHelpers(list, kHelperPayload, out);
}

void appendVector(const std::vector<MacroMatchArm>& list, FieldValue& out) {
  appendHelpers(list, kHelperMacroArm, out);
}

/// Operator and modifier keywords. The AST keeps these as enum members rather
/// than nodes, so the bridge reports the word `ast` turns back into a node.
std::string tokenName(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add: return "add";
  case BinaryOp::Sub: return "sub";
  case BinaryOp::Mul: return "mul";
  case BinaryOp::Div: return "div";
  case BinaryOp::FloorDiv: return "floordiv";
  case BinaryOp::Mod: return "mod";
  case BinaryOp::Pow: return "pow";
  case BinaryOp::Eq: return "eq";
  case BinaryOp::Ne: return "ne";
  case BinaryOp::Lt: return "lt";
  case BinaryOp::Le: return "le";
  case BinaryOp::Gt: return "gt";
  case BinaryOp::Ge: return "ge";
  case BinaryOp::And: return "and";
  case BinaryOp::Or: return "or";
  case BinaryOp::Is: return "is";
  case BinaryOp::IsNot: return "isnot";
  case BinaryOp::In: return "in";
  case BinaryOp::NotIn: return "notin";
  case BinaryOp::BitAnd: return "bitand";
  case BinaryOp::BitOr: return "bitor";
  case BinaryOp::BitXor: return "bitxor";
  case BinaryOp::Shl: return "shl";
  case BinaryOp::Shr: return "shr";
  }
  return "unknown";
}

std::string tokenName(UnaryOp op) {
  switch (op) {
  case UnaryOp::Neg: return "neg";
  case UnaryOp::Pos: return "pos";
  case UnaryOp::Not: return "not";
  case UnaryOp::Invert: return "invert";
  case UnaryOp::PreInc: return "preinc";
  case UnaryOp::PreDec: return "predec";
  case UnaryOp::PostInc: return "postinc";
  case UnaryOp::PostDec: return "postdec";
  case UnaryOp::Deref: return "deref";
  case UnaryOp::AddrOf: return "addrof";
  }
  return "unknown";
}

std::string tokenName(AssignOp op) {
  switch (op) {
  case AssignOp::Assign: return "assign";
  case AssignOp::Add: return "add";
  case AssignOp::Sub: return "sub";
  case AssignOp::Mul: return "mul";
  case AssignOp::Div: return "div";
  case AssignOp::FloorDiv: return "floordiv";
  case AssignOp::Pow: return "pow";
  case AssignOp::Mod: return "mod";
  case AssignOp::BitAnd: return "bitand";
  case AssignOp::BitOr: return "bitor";
  case AssignOp::BitXor: return "bitxor";
  case AssignOp::Shl: return "shl";
  case AssignOp::Shr: return "shr";
  }
  return "unknown";
}

std::string tokenName(ParamKind kind) {
  switch (kind) {
  case ParamKind::Normal: return "normal";
  case ParamKind::VarArg: return "vararg";
  case ParamKind::KwArg: return "kwarg";
  }
  return "unknown";
}

std::string tokenName(PropertyKind kind) {
  switch (kind) {
  case PropertyKind::None: return "none";
  case PropertyKind::Get: return "getter";
  case PropertyKind::Set: return "setter";
  }
  return "none";
}

std::string tokenName(MacroSyntaxMode mode) {
  switch (mode) {
  case MacroSyntaxMode::Sere: return "sere";
  case MacroSyntaxMode::Tokens: return "tokens";
  case MacroSyntaxMode::Raw: return "raw";
  case MacroSyntaxMode::Pipeline: return "pipeline";
  }
  return "sere";
}

std::string tokenName(MacroInterpolate mode) {
  switch (mode) {
  case MacroInterpolate::None: return "none";
  case MacroInterpolate::Brace: return "brace";
  case MacroInterpolate::Dollar: return "dollar";
  }
  return "none";
}

std::string tokenName(MacroDelimiter delimiter) {
  switch (delimiter) {
  case MacroDelimiter::BangParen: return "bang_paren";
  case MacroDelimiter::BangBrace: return "bang_brace";
  case MacroDelimiter::BangBracket: return "bang_bracket";
  case MacroDelimiter::Indent: return "indent";
  }
  return "bang_paren";
}

std::string tokenName(ModuleExportMode mode) {
  switch (mode) {
  case ModuleExportMode::Default: return "default";
  case ModuleExportMode::Replace: return "replace";
  case ModuleExportMode::Append: return "append";
  }
  return "default";
}

template <typename V> FieldValue makeValue(const V& value) {
  using T = std::remove_cvref_t<V>;
  FieldValue out;
  if constexpr (std::is_base_of_v<Node, T>) {
    out.kind = SERE_AST_VALUE_NODE;
    out.node = &value;
    out.present = true;
  } else if constexpr (std::is_pointer_v<T> && std::is_base_of_v<Node, std::remove_pointer_t<T>>) {
    out.kind = SERE_AST_VALUE_NODE;
    out.node = value;
    out.present = value != nullptr;
  } else if constexpr (IsUniquePtrToNode<T>::value) {
    // The helper structs hold their children by unique_ptr, unlike nodes, whose
    // accessors hand back raw pointers.
    out.kind = SERE_AST_VALUE_NODE;
    out.node = value.get();
    out.present = value != nullptr;
  } else if constexpr (IsStdVector<T>::value) {
    appendVector(value, out);
  } else if constexpr (std::is_same_v<T, std::string>) {
    out.kind = SERE_AST_VALUE_TEXT;
    out.text = value;
    out.present = true;
  } else if constexpr (std::is_same_v<T, bool>) {
    out.kind = SERE_AST_VALUE_BOOL;
    out.boolean = value;
    out.present = true;
  } else if constexpr (std::is_enum_v<T>) {
    out.kind = SERE_AST_VALUE_TOKEN;
    out.text = tokenName(value);
    out.present = true;
  } else if constexpr (std::is_integral_v<T>) {
    out.kind = SERE_AST_VALUE_INT;
    out.integer = static_cast<int64_t>(value);
    out.present = true;
  } else if constexpr (std::is_floating_point_v<T>) {
    out.kind = SERE_AST_VALUE_FLOAT;
    out.real = static_cast<double>(value);
    out.present = true;
  } else {
    static_assert(std::is_void_v<T>, "unsupported AST field type");
  }
  return out;
}

FieldValue emptyValue() { return FieldValue{}; }

FieldGetter falseField() {
  return +[](const Node&) {
    FieldValue out;
    out.kind = SERE_AST_VALUE_BOOL;
    out.boolean = false;
    out.present = true;
    return out;
  };
}

} // namespace
} // namespace sere

/// A handle: either a syntax node or one of the helper structs that carry syntax
/// inside a node.
///
/// Field values are materialised on demand into `fields`, which is sized to the
/// field count before the first read so a pointer into an entry (the characters
/// of a string, the elements of a string list) keeps pointing at the same slot
/// for as long as the tree lives. Reading a field twice is a cache hit, and the
/// callers that only want a child handle or a count never copy a string.
struct SereAstNode {
  const sere::Node* node = nullptr;
  sere::HelperRef helper{};
  SereAstTree* tree = nullptr;
  std::vector<sere::FieldValue> fields;
  bool fieldsReady = false;
};

/// A parsed tree. Owns the module, the source text, and every handle, so a
/// handle stays valid until `sere_ast_free`.
struct SereAstTree {
  std::string source;
  std::string filename;
  std::unique_ptr<sere::SourceManager> sourceManager;
  sere::DiagnosticEngine diagnostics;
  std::unique_ptr<sere::Module> module;
  std::unique_ptr<sere::Expr> expression;
  std::vector<std::unique_ptr<SereAstNode>> handles;
  std::vector<std::unique_ptr<std::vector<SereAstNode*>>> lists;
  std::map<std::pair<const SereAstNode*, std::size_t>, std::vector<SereAstNode*>*> listCache;

  SereAstNode* makeHandle(const sere::Node* node);
  SereAstNode* makeHelper(const sere::HelperRef& ref);
  /// Child handles of a list field, built once per (node, field) so repeated
  /// element reads stay O(1).
  std::vector<SereAstNode*>& listFor(const SereAstNode* handle, std::size_t index);
};

namespace sere {
namespace {

// ---------------------------------------------------------------------------
// Field tables
// ---------------------------------------------------------------------------

struct FieldSpec {
  const char* name;
  FieldGetter get;
};

struct KindSpec {
  const char* name;
  int32_t category;
  const FieldSpec* fields;
  std::size_t fieldCount;
};

constexpr FieldSpec field(const char* name, FieldGetter get) { return FieldSpec{name, get}; }

/// Reads one accessor of `TYPE` from any node. Optional members are read through
/// the accessor that returns a pointer, so a getter is safe on every instance.
#define FIELD(NAME, TYPE, ACCESSOR)                                                            \
  sere::field(NAME, +[](const sere::Node& node) {                                              \
    return sere::makeValue(static_cast<const TYPE&>(node).ACCESSOR());                          \
  })

/// A field whose value is a fixed word, such as a literal's flavor.
#define TOKEN_FIELD(NAME, WORD)                                                                \
  sere::field(NAME, +[](const sere::Node&) {                                                   \
    sere::FieldValue out;                                                                       \
    out.kind = SERE_AST_VALUE_TOKEN;                                                            \
    out.text = WORD;                                                                            \
    out.present = true;                                                                         \
    return out;                                                                                 \
  })

#define COUNT_OF(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

const FieldSpec kModuleFields[] = {
    FIELD("body", Module, statements),
    FIELD("export_mode", Module, exportMode),
    FIELD("export_names", Module, exportNames),
};

const FieldSpec kTypeFields[] = {
    FIELD("name", TypeExpr, name),
    FIELD("args", TypeExpr, args),
};

// The five literal kinds share the `Constant` shape: `value` carries the literal
// and `flavor` says which spelling produced it.
const FieldSpec kIntegerFields[] = {
    FIELD("value", IntegerLiteral, value),
    TOKEN_FIELD("flavor", "int"),
    FIELD("is_byte", IntegerLiteral, isByte),
    sere::field("is_f32", sere::falseField()),
    sere::field("is_regex", sere::falseField()),
    sere::field("is_bytes", sere::falseField()),
};

const FieldSpec kFloatFields[] = {
    FIELD("value", FloatLiteral, value),
    TOKEN_FIELD("flavor", "float"),
    sere::field("is_byte", sere::falseField()),
    FIELD("is_f32", FloatLiteral, isF32),
    sere::field("is_regex", sere::falseField()),
    sere::field("is_bytes", sere::falseField()),
};

const FieldSpec kStringFields[] = {
    FIELD("value", StringLiteral, value),
    TOKEN_FIELD("flavor", "str"),
    sere::field("is_byte", sere::falseField()),
    sere::field("is_f32", sere::falseField()),
    FIELD("is_regex", StringLiteral, isRegex),
    FIELD("is_bytes", StringLiteral, isBytes),
};

const FieldSpec kBooleanFields[] = {
    FIELD("value", BooleanLiteral, value),
    TOKEN_FIELD("flavor", "bool"),
    sere::field("is_byte", sere::falseField()),
    sere::field("is_f32", sere::falseField()),
    sere::field("is_regex", sere::falseField()),
    sere::field("is_bytes", sere::falseField()),
};

const FieldSpec kNoneFields[] = {
    sere::field("value", +[](const sere::Node&) { return sere::emptyValue(); }),
    TOKEN_FIELD("flavor", "none"),
    sere::field("is_byte", sere::falseField()),
    sere::field("is_f32", sere::falseField()),
    sere::field("is_regex", sere::falseField()),
    sere::field("is_bytes", sere::falseField()),
};

const FieldSpec kJoinedStrFields[] = {
    FIELD("parts", InterpolatedStringExpr, parts),
};

const FieldSpec kNameFields[] = {
    FIELD("id", NameExpr, name),
};

const FieldSpec kCallFields[] = {
    FIELD("func", CallExpr, callee),
    FIELD("args", CallExpr, arguments),
    FIELD("keywords", CallExpr, keywordArguments),
    FIELD("type_args", CallExpr, typeArgs),
};

const FieldSpec kAttributeFields[] = {
    FIELD("value", MemberExpr, object),
    FIELD("attr", MemberExpr, field),
};

const FieldSpec kBinOpFields[] = {
    FIELD("left", BinaryExpr, left),
    FIELD("op", BinaryExpr, op),
    FIELD("right", BinaryExpr, right),
};

const FieldSpec kUnaryOpFields[] = {
    FIELD("op", UnaryExpr, op),
    FIELD("operand", UnaryExpr, operand),
};

const FieldSpec kAwaitFields[] = {
    FIELD("value", AwaitExpr, operand),
};

const FieldSpec kCastFields[] = {
    FIELD("value", CastExpr, value),
    FIELD("target", CastExpr, target),
};

const FieldSpec kSubscriptFields[] = {
    FIELD("value", IndexExpr, object),
    sere::field("index", +[](const sere::Node& node) {
      const auto& index = static_cast<const IndexExpr&>(node);
      return index.isSlice() ? sere::emptyValue() : sere::makeValue(index.index());
    }),
    sere::field("lower", +[](const sere::Node& node) {
      return sere::makeValue(static_cast<const IndexExpr&>(node).start());
    }),
    sere::field("upper", +[](const sere::Node& node) {
      return sere::makeValue(static_cast<const IndexExpr&>(node).stop());
    }),
    FIELD("is_slice", IndexExpr, isSlice),
};

const FieldSpec kListFields[] = {
    FIELD("elts", ListLiteral, elements),
};

const FieldSpec kDictFields[] = {
    FIELD("keys", DictLiteral, keys),
    FIELD("values", DictLiteral, values),
};

const FieldSpec kComprehensionFields[] = {
    FIELD("element", ComprehensionExpr, element),
    FIELD("name", ComprehensionExpr, name),
    FIELD("iterable", ComprehensionExpr, iterable),
};

const FieldSpec kIfExpFields[] = {
    FIELD("test", TernaryExpr, condition),
    FIELD("body", TernaryExpr, thenValue),
    FIELD("orelse", TernaryExpr, elseValue),
};

const FieldSpec kTupleFields[] = {
    FIELD("elts", TupleExpr, elements),
};

const FieldSpec kNamedExprFields[] = {
    FIELD("target", WalrusExpr, name),
    FIELD("value", WalrusExpr, value),
};

const FieldSpec kDoFields[] = {
    FIELD("body", DoExpr, body),
};

const FieldSpec kLambdaFields[] = {
    FIELD("args", LambdaExpr, params),
    FIELD("body", LambdaExpr, body),
    FIELD("returns", LambdaExpr, returnType),
};

const FieldSpec kAnnAssignFields[] = {
    FIELD("target", VarDecl, name),
    sere::field("annotation", +[](const sere::Node& node) {
      const auto& declaration = static_cast<const VarDecl&>(node);
      return declaration.hasType() ? sere::makeValue(declaration.type()) : sere::emptyValue();
    }),
    FIELD("value", VarDecl, init),
    FIELD("is_static", VarDecl, isStatic),
    FIELD("is_const", VarDecl, isConst),
};

const FieldSpec kAssignFields[] = {
    FIELD("target", AssignStmt, target),
    FIELD("value", AssignStmt, value),
    FIELD("op", AssignStmt, op),
};

const FieldSpec kReturnFields[] = {
    FIELD("value", ReturnStmt, value),
};

// `Yield` shares `Return`'s shape, but its value accessor lives on the base.
const FieldSpec kYieldFields[] = {
    sere::field("value", +[](const sere::Node& node) {
      return sere::makeValue(static_cast<const YieldStmt&>(node).value());
    }),
};

const FieldSpec kExprStmtFields[] = {
    FIELD("value", ExprStmt, expression),
};

const FieldSpec kPassFields[] = {};
const FieldSpec kBreakFields[] = {};
const FieldSpec kContinueFields[] = {};

const FieldSpec kIfFields[] = {
    FIELD("branches", IfStmt, branches),
};

const FieldSpec kWhileFields[] = {
    FIELD("test", WhileStmt, condition),
    FIELD("body", WhileStmt, body),
};

const FieldSpec kForFields[] = {
    FIELD("target", ForStmt, name),
    FIELD("iter", ForStmt, iterable),
    FIELD("body", ForStmt, body),
};

const FieldSpec kAssertFields[] = {
    FIELD("test", AssertStmt, condition),
    FIELD("msg", AssertStmt, message),
};

const FieldSpec kRaiseFields[] = {
    FIELD("exc", RaiseStmt, value),
};

const FieldSpec kTryFields[] = {
    FIELD("body", TryStmt, body),
    FIELD("handlers", TryStmt, handlers),
    FIELD("orelse", TryStmt, elseBody),
    FIELD("finalbody", TryStmt, finallyBody),
};

const FieldSpec kMatchFields[] = {
    FIELD("subject", MatchStmt, subject),
    FIELD("cases", MatchStmt, arms),
};

const FieldSpec kDeleteFields[] = {
    FIELD("target", DelStmt, target),
};

const FieldSpec kDeferFields[] = {
    FIELD("body", DeferStmt, body),
};

const FieldSpec kWithFields[] = {
    FIELD("context", WithStmt, context),
    FIELD("target", WithStmt, name),
    FIELD("body", WithStmt, body),
};

const FieldSpec kFunctionFields[] = {
    FIELD("name", FunctionDef, name),
    FIELD("args", FunctionDef, params),
    FIELD("returns", FunctionDef, returnType),
    FIELD("body", FunctionDef, body),
    FIELD("decorator_list", FunctionDef, decoratorExprs),
    FIELD("decorator_names", FunctionDef, decorators),
    FIELD("type_params", FunctionDef, typeParams),
    FIELD("docstring", FunctionDef, docstring),
    FIELD("extern_name", FunctionDef, externName),
    FIELD("owner", FunctionDef, ownerClass),
    FIELD("is_extern", FunctionDef, isExtern),
    FIELD("is_method", FunctionDef, isMethod),
    FIELD("is_async", FunctionDef, isAsync),
    FIELD("is_generator", FunctionDef, isGenerator),
    FIELD("is_abstract", FunctionDef, isAbstract),
    FIELD("is_override", FunctionDef, isOverride),
    FIELD("property", FunctionDef, propertyKind),
    FIELD("property_name", FunctionDef, propertyName),
    FIELD("module", FunctionDef, modulePrefix),
};

const FieldSpec kClassFields[] = {
    FIELD("name", ClassDef, name),
    FIELD("fields", ClassDef, fields),
    FIELD("methods", ClassDef, methods),
    FIELD("bases", ClassDef, bases),
    FIELD("base_types", ClassDef, baseTypes),
    FIELD("type_params", ClassDef, typeParams),
    FIELD("decorator_list", ClassDef, decoratorExprs),
    FIELD("decorator_names", ClassDef, decorators),
    FIELD("docstring", ClassDef, docstring),
    FIELD("is_struct", ClassDef, isStruct),
    FIELD("is_frozen", ClassDef, isFrozen),
};

const FieldSpec kEnumFields[] = {
    FIELD("name", EnumDef, name),
    FIELD("variants", EnumDef, variants),
    FIELD("methods", EnumDef, methods),
    FIELD("type_params", EnumDef, typeParams),
    FIELD("decorator_list", EnumDef, decoratorExprs),
    FIELD("decorator_names", EnumDef, decorators),
    FIELD("docstring", EnumDef, docstring),
    FIELD("is_flags", EnumDef, isFlags),
};

const FieldSpec kTypeAliasFields[] = {
    FIELD("name", TypeAlias, name),
    FIELD("value", TypeAlias, type),
    FIELD("type_params", TypeAlias, typeParams),
};

const FieldSpec kImportFields[] = {
    FIELD("module", ImportStmt, modulePath),
    FIELD("alias", ImportStmt, alias),
    FIELD("names", ImportStmt, names),
    FIELD("name_aliases", ImportStmt, nameAliases),
    FIELD("is_from", ImportStmt, isFrom),
    FIELD("star", ImportStmt, star),
};

const FieldSpec kSpliceFields[] = {
    FIELD("name", SpliceExpr, name),
    FIELD("is_repeat", SpliceExpr, isRepeat),
    FIELD("comma_separated", SpliceExpr, commaSeparated),
};

const FieldSpec kMacroFields[] = {
    FIELD("name", MacroDef, name),
    FIELD("params", MacroDef, params),
    FIELD("syntax_mode", MacroDef, syntaxMode),
    FIELD("interpolate", MacroDef, interpolate),
    FIELD("wrapper", MacroDef, wrapper),
    FIELD("quote", MacroDef, quoteBody),
    FIELD("arms", MacroDef, matchArms),
    FIELD("variadic", MacroDef, variadic),
    FIELD("typed", MacroDef, typed),
};

const FieldSpec kMacroInvokeFields[] = {
    FIELD("name", MacroInvokeExpr, name),
    FIELD("delimiter", MacroInvokeExpr, delimiter),
    FIELD("raw", MacroInvokeExpr, rawText),
};

const KindSpec kModuleSpec{"Module", SERE_AST_CATEGORY_MODULE, kModuleFields,
                           COUNT_OF(kModuleFields)};
const KindSpec kTypeSpec{"Type", SERE_AST_CATEGORY_TYPE, kTypeFields, COUNT_OF(kTypeFields)};
const KindSpec kIntegerSpec{"Constant", SERE_AST_CATEGORY_EXPR, kIntegerFields,
                            COUNT_OF(kIntegerFields)};
const KindSpec kFloatSpec{"Constant", SERE_AST_CATEGORY_EXPR, kFloatFields,
                          COUNT_OF(kFloatFields)};
const KindSpec kStringSpec{"Constant", SERE_AST_CATEGORY_EXPR, kStringFields,
                           COUNT_OF(kStringFields)};
const KindSpec kBooleanSpec{"Constant", SERE_AST_CATEGORY_EXPR, kBooleanFields,
                            COUNT_OF(kBooleanFields)};
const KindSpec kNoneSpec{"Constant", SERE_AST_CATEGORY_EXPR, kNoneFields,
                         COUNT_OF(kNoneFields)};
const KindSpec kJoinedStrSpec{"JoinedStr", SERE_AST_CATEGORY_EXPR, kJoinedStrFields,
                              COUNT_OF(kJoinedStrFields)};
const KindSpec kNameSpec{"Name", SERE_AST_CATEGORY_EXPR, kNameFields, COUNT_OF(kNameFields)};
const KindSpec kCallSpec{"Call", SERE_AST_CATEGORY_EXPR, kCallFields, COUNT_OF(kCallFields)};
const KindSpec kAttributeSpec{"Attribute", SERE_AST_CATEGORY_EXPR, kAttributeFields,
                              COUNT_OF(kAttributeFields)};
const KindSpec kBinOpSpec{"BinOp", SERE_AST_CATEGORY_EXPR, kBinOpFields, COUNT_OF(kBinOpFields)};
const KindSpec kUnaryOpSpec{"UnaryOp", SERE_AST_CATEGORY_EXPR, kUnaryOpFields,
                            COUNT_OF(kUnaryOpFields)};
const KindSpec kAwaitSpec{"Await", SERE_AST_CATEGORY_EXPR, kAwaitFields, COUNT_OF(kAwaitFields)};
const KindSpec kCastSpec{"Cast", SERE_AST_CATEGORY_EXPR, kCastFields, COUNT_OF(kCastFields)};
const KindSpec kSubscriptSpec{"Subscript", SERE_AST_CATEGORY_EXPR, kSubscriptFields,
                              COUNT_OF(kSubscriptFields)};
const KindSpec kListSpec{"List", SERE_AST_CATEGORY_EXPR, kListFields, COUNT_OF(kListFields)};
const KindSpec kDictSpec{"Dict", SERE_AST_CATEGORY_EXPR, kDictFields, COUNT_OF(kDictFields)};
const KindSpec kComprehensionSpec{"Comprehension", SERE_AST_CATEGORY_EXPR, kComprehensionFields,
                                  COUNT_OF(kComprehensionFields)};
const KindSpec kIfExpSpec{"IfExp", SERE_AST_CATEGORY_EXPR, kIfExpFields, COUNT_OF(kIfExpFields)};
const KindSpec kTupleSpec{"Tuple", SERE_AST_CATEGORY_EXPR, kTupleFields, COUNT_OF(kTupleFields)};
const KindSpec kNamedExprSpec{"NamedExpr", SERE_AST_CATEGORY_EXPR, kNamedExprFields,
                              COUNT_OF(kNamedExprFields)};
const KindSpec kDoSpec{"Do", SERE_AST_CATEGORY_EXPR, kDoFields, COUNT_OF(kDoFields)};
const KindSpec kLambdaSpec{"Lambda", SERE_AST_CATEGORY_EXPR, kLambdaFields,
                           COUNT_OF(kLambdaFields)};
const KindSpec kAnnAssignSpec{"AnnAssign", SERE_AST_CATEGORY_STMT, kAnnAssignFields,
                              COUNT_OF(kAnnAssignFields)};
const KindSpec kAssignSpec{"Assign", SERE_AST_CATEGORY_STMT, kAssignFields,
                           COUNT_OF(kAssignFields)};
const KindSpec kReturnSpec{"Return", SERE_AST_CATEGORY_STMT, kReturnFields,
                           COUNT_OF(kReturnFields)};
const KindSpec kYieldSpec{"Yield", SERE_AST_CATEGORY_STMT, kYieldFields, COUNT_OF(kYieldFields)};
const KindSpec kExprStmtSpec{"Expr", SERE_AST_CATEGORY_STMT, kExprStmtFields,
                             COUNT_OF(kExprStmtFields)};
const KindSpec kPassSpec{"Pass", SERE_AST_CATEGORY_STMT, kPassFields, COUNT_OF(kPassFields)};
const KindSpec kBreakSpec{"Break", SERE_AST_CATEGORY_STMT, kBreakFields, COUNT_OF(kBreakFields)};
const KindSpec kContinueSpec{"Continue", SERE_AST_CATEGORY_STMT, kContinueFields,
                             COUNT_OF(kContinueFields)};
const KindSpec kIfSpec{"If", SERE_AST_CATEGORY_STMT, kIfFields, COUNT_OF(kIfFields)};
const KindSpec kWhileSpec{"While", SERE_AST_CATEGORY_STMT, kWhileFields, COUNT_OF(kWhileFields)};
const KindSpec kForSpec{"For", SERE_AST_CATEGORY_STMT, kForFields, COUNT_OF(kForFields)};
const KindSpec kAssertSpec{"Assert", SERE_AST_CATEGORY_STMT, kAssertFields,
                           COUNT_OF(kAssertFields)};
const KindSpec kRaiseSpec{"Raise", SERE_AST_CATEGORY_STMT, kRaiseFields, COUNT_OF(kRaiseFields)};
const KindSpec kTrySpec{"Try", SERE_AST_CATEGORY_STMT, kTryFields, COUNT_OF(kTryFields)};
const KindSpec kMatchSpec{"Match", SERE_AST_CATEGORY_STMT, kMatchFields, COUNT_OF(kMatchFields)};
const KindSpec kDeleteSpec{"Delete", SERE_AST_CATEGORY_STMT, kDeleteFields,
                           COUNT_OF(kDeleteFields)};
const KindSpec kDeferSpec{"Defer", SERE_AST_CATEGORY_STMT, kDeferFields, COUNT_OF(kDeferFields)};
const KindSpec kWithSpec{"With", SERE_AST_CATEGORY_STMT, kWithFields, COUNT_OF(kWithFields)};
const KindSpec kFunctionSpec{"FunctionDef", SERE_AST_CATEGORY_STMT, kFunctionFields,
                             COUNT_OF(kFunctionFields)};
const KindSpec kClassSpec{"ClassDef", SERE_AST_CATEGORY_STMT, kClassFields,
                          COUNT_OF(kClassFields)};
const KindSpec kEnumSpec{"EnumDef", SERE_AST_CATEGORY_STMT, kEnumFields, COUNT_OF(kEnumFields)};
const KindSpec kTypeAliasSpec{"TypeAlias", SERE_AST_CATEGORY_STMT, kTypeAliasFields,
                              COUNT_OF(kTypeAliasFields)};
const KindSpec kImportSpec{"Import", SERE_AST_CATEGORY_STMT, kImportFields,
                           COUNT_OF(kImportFields)};
const KindSpec kSpliceSpec{"Splice", SERE_AST_CATEGORY_EXPR, kSpliceFields,
                           COUNT_OF(kSpliceFields)};
const KindSpec kMacroSpec{"MacroDef", SERE_AST_CATEGORY_STMT, kMacroFields,
                          COUNT_OF(kMacroFields)};
const KindSpec kMacroInvokeSpec{"MacroInvoke", SERE_AST_CATEGORY_EXPR, kMacroInvokeFields,
                                COUNT_OF(kMacroInvokeFields)};

const KindSpec kUnknownSpec{"Unknown", SERE_AST_CATEGORY_HELPER, nullptr, 0};

/// One `case` per `NodeKind`, so the compiler reports a kind the bridge forgot
/// instead of silently mapping fields of the wrong shape.
const KindSpec& specForIndex(std::size_t index) {
  switch (static_cast<NodeKind>(index)) {
  case NodeKind::TypeExpr: return kTypeSpec;
  case NodeKind::IntegerLiteral: return kIntegerSpec;
  case NodeKind::FloatLiteral: return kFloatSpec;
  case NodeKind::StringLiteral: return kStringSpec;
  case NodeKind::InterpolatedStringExpr: return kJoinedStrSpec;
  case NodeKind::BooleanLiteral: return kBooleanSpec;
  case NodeKind::NoneLiteral: return kNoneSpec;
  case NodeKind::NameExpr: return kNameSpec;
  case NodeKind::CallExpr: return kCallSpec;
  case NodeKind::MemberExpr: return kAttributeSpec;
  case NodeKind::BinaryExpr: return kBinOpSpec;
  case NodeKind::UnaryExpr: return kUnaryOpSpec;
  case NodeKind::AwaitExpr: return kAwaitSpec;
  case NodeKind::CastExpr: return kCastSpec;
  case NodeKind::IndexExpr: return kSubscriptSpec;
  case NodeKind::ListLiteral: return kListSpec;
  case NodeKind::DictLiteral: return kDictSpec;
  case NodeKind::ComprehensionExpr: return kComprehensionSpec;
  case NodeKind::TernaryExpr: return kIfExpSpec;
  case NodeKind::TupleExpr: return kTupleSpec;
  case NodeKind::WalrusExpr: return kNamedExprSpec;
  case NodeKind::DoExpr: return kDoSpec;
  case NodeKind::LambdaExpr: return kLambdaSpec;
  case NodeKind::VarDecl: return kAnnAssignSpec;
  case NodeKind::AssignStmt: return kAssignSpec;
  case NodeKind::ReturnStmt: return kReturnSpec;
  case NodeKind::YieldStmt: return kYieldSpec;
  case NodeKind::ExprStmt: return kExprStmtSpec;
  case NodeKind::PassStmt: return kPassSpec;
  case NodeKind::BreakStmt: return kBreakSpec;
  case NodeKind::ContinueStmt: return kContinueSpec;
  case NodeKind::IfStmt: return kIfSpec;
  case NodeKind::WhileStmt: return kWhileSpec;
  case NodeKind::ForStmt: return kForSpec;
  case NodeKind::AssertStmt: return kAssertSpec;
  case NodeKind::RaiseStmt: return kRaiseSpec;
  case NodeKind::TryStmt: return kTrySpec;
  case NodeKind::MatchStmt: return kMatchSpec;
  case NodeKind::DelStmt: return kDeleteSpec;
  case NodeKind::DeferStmt: return kDeferSpec;
  case NodeKind::WithStmt: return kWithSpec;
  case NodeKind::FunctionDef: return kFunctionSpec;
  case NodeKind::ClassDef: return kClassSpec;
  case NodeKind::EnumDef: return kEnumSpec;
  case NodeKind::TypeAlias: return kTypeAliasSpec;
  case NodeKind::ImportStmt: return kImportSpec;
  case NodeKind::SpliceExpr: return kSpliceSpec;
  case NodeKind::MacroDef: return kMacroSpec;
  case NodeKind::MacroInvokeExpr: return kMacroInvokeSpec;
  case NodeKind::MacroInvokeStmt: return kMacroInvokeSpec;
  case NodeKind::Module: return kModuleSpec;
  }
  return kUnknownSpec;
}

constexpr std::size_t kKindCount = static_cast<std::size_t>(NodeKind::Module) + 1;

// ---------------------------------------------------------------------------
// Helper field tables
// ---------------------------------------------------------------------------

using HelperGetter = FieldValue (*)(const void*);

struct HelperFieldSpec {
  const char* name;
  HelperGetter get;
};

struct HelperSpec {
  const char* name;
  const HelperFieldSpec* fields;
  std::size_t fieldCount;
  /// Source range of the helper when it carries one, else nullptr.
  void (*range)(const void*, SourceRange&);
};

constexpr HelperFieldSpec helperField(const char* name, HelperGetter get) {
  return HelperFieldSpec{name, get};
}

#define HFIELD(NAME, TYPE, MEMBER)                                                             \
  sere::helperField(NAME, +[](const void* pointer) {                                           \
    return sere::makeValue(static_cast<const TYPE*>(pointer)->MEMBER);                          \
  })

template <typename T> void helperRange(const void* pointer, SourceRange& out) {
  out = static_cast<const T*>(pointer)->range;
}

const HelperFieldSpec kPartFields[] = {
    HFIELD("literal", StringPart, literal),
    HFIELD("value", StringPart, value),
    HFIELD("spec", StringPart, spec),
};

const HelperFieldSpec kKeywordFields[] = {
    HFIELD("arg", NamedArgument, name),
    HFIELD("value", NamedArgument, value),
    HFIELD("is_splat", NamedArgument, splat),
};

const HelperFieldSpec kParamFields[] = {
    HFIELD("name", ParamDecl, name),
    HFIELD("annotation", ParamDecl, type),
    HFIELD("default", ParamDecl, defaultValue),
    HFIELD("kind", ParamDecl, kind),
};

const HelperFieldSpec kIfBranchFields[] = {
    HFIELD("test", IfBranch, condition),
    HFIELD("body", IfBranch, body),
};

const HelperFieldSpec kHandlerFields[] = {
    HFIELD("type", ExceptHandler, type),
    HFIELD("name", ExceptHandler, name),
    HFIELD("body", ExceptHandler, body),
};

const HelperFieldSpec kCaseFields[] = {
    HFIELD("pattern", MatchArm, pattern),
    HFIELD("guard", MatchArm, guard),
    HFIELD("body", MatchArm, body),
};

const HelperFieldSpec kClassFieldFields[] = {
    HFIELD("name", FieldDecl, name),
    HFIELD("annotation", FieldDecl, type),
    HFIELD("value", FieldDecl, init),
    HFIELD("is_public", FieldDecl, isPublic),
    HFIELD("is_static", FieldDecl, isStatic),
};

const HelperFieldSpec kVariantFields[] = {
    HFIELD("name", EnumVariant, name),
    HFIELD("value", EnumVariant, value),
    HFIELD("payload", EnumVariant, payload),
};

const HelperFieldSpec kPayloadFields[] = {
    HFIELD("name", EnumPayloadField, name),
    HFIELD("annotation", EnumPayloadField, type),
};

const HelperFieldSpec kMacroArmFields[] = {
    HFIELD("body", MacroMatchArm, body),
};

const HelperSpec kNoHelperSpec{"", nullptr, 0, nullptr};
const HelperSpec kPartSpec{"StringPart", kPartFields, COUNT_OF(kPartFields), nullptr};
const HelperSpec kKeywordSpec{"Keyword", kKeywordFields, COUNT_OF(kKeywordFields), nullptr};
const HelperSpec kParamSpec{"Param", kParamFields, COUNT_OF(kParamFields),
                            &helperRange<ParamDecl>};
const HelperSpec kIfBranchSpec{"IfBranch", kIfBranchFields, COUNT_OF(kIfBranchFields),
                               &helperRange<IfBranch>};
const HelperSpec kHandlerSpec{"Handler", kHandlerFields, COUNT_OF(kHandlerFields),
                              &helperRange<ExceptHandler>};
const HelperSpec kCaseSpec{"MatchCase", kCaseFields, COUNT_OF(kCaseFields),
                           &helperRange<MatchArm>};
const HelperSpec kClassFieldSpec{"Field", kClassFieldFields, COUNT_OF(kClassFieldFields),
                                 &helperRange<FieldDecl>};
const HelperSpec kVariantSpec{"Variant", kVariantFields, COUNT_OF(kVariantFields),
                              &helperRange<EnumVariant>};
const HelperSpec kPayloadSpec{"PayloadField", kPayloadFields, COUNT_OF(kPayloadFields), nullptr};
const HelperSpec kMacroArmSpec{"MacroArm", kMacroArmFields, COUNT_OF(kMacroArmFields),
                               &helperRange<MacroMatchArm>};

const HelperSpec& helperSpecFor(int32_t kind) {
  switch (kind) {
  case kHelperPart: return kPartSpec;
  case kHelperKeyword: return kKeywordSpec;
  case kHelperParam: return kParamSpec;
  case kHelperIfBranch: return kIfBranchSpec;
  case kHelperHandler: return kHandlerSpec;
  case kHelperCase: return kCaseSpec;
  case kHelperField: return kClassFieldSpec;
  case kHelperVariant: return kVariantSpec;
  case kHelperPayload: return kPayloadSpec;
  case kHelperMacroArm: return kMacroArmSpec;
  default: return kNoHelperSpec;
  }
}

/// The table of whatever a handle points at, or nullptr for an empty slot.
const KindSpec* kindSpecOf(const SereAstNode* handle) {
  if (handle == nullptr || handle->node == nullptr) {
    return nullptr;
  }
  return &specForIndex(static_cast<std::size_t>(handle->node->kind()));
}

const HelperSpec* helperSpecOf(const SereAstNode* handle) {
  if (handle == nullptr || handle->node != nullptr) {
    return nullptr;
  }
  const HelperSpec& spec = helperSpecFor(handle->helper.kind);
  return spec.fields == nullptr ? nullptr : &spec;
}

std::size_t fieldCountOf(const SereAstNode* handle) {
  if (const KindSpec* spec = kindSpecOf(handle); spec != nullptr) {
    return spec->fieldCount;
  }
  if (const HelperSpec* spec = helperSpecOf(handle); spec != nullptr) {
    return spec->fieldCount;
  }
  return 0;
}

/// Reads one field of a handle. `get` is set for nodes, `helperGet` for the
/// helper structs; the caller knows which one it has when the name is non-null.
const char* fieldSlotOf(const SereAstNode* handle, std::size_t index, FieldGetter& get,
                        HelperGetter& helperGet) {
  if (const KindSpec* spec = kindSpecOf(handle); spec != nullptr) {
    if (index >= spec->fieldCount) {
      return nullptr;
    }
    get = spec->fields[index].get;
    return spec->fields[index].name;
  }
  if (const HelperSpec* spec = helperSpecOf(handle); spec != nullptr) {
    if (index >= spec->fieldCount) {
      return nullptr;
    }
    helperGet = spec->fields[index].get;
    return spec->fields[index].name;
  }
  return nullptr;
}

FieldValue readField(const SereAstNode* handle, std::size_t index) {
  FieldGetter get = nullptr;
  HelperGetter helperGet = nullptr;
  if (fieldSlotOf(handle, index, get, helperGet) == nullptr) {
    return emptyValue();
  }
  if (get != nullptr && handle->node != nullptr) {
    return get(*handle->node);
  }
  if (helperGet != nullptr) {
    return helperGet(handle->helper.pointer);
  }
  return emptyValue();
}

/// Source range of whatever a handle points at. A helper struct carries its own
/// range, so a caller can ask about any handle it holds.
SourceRange rangeOf(const SereAstNode* node) {
  SourceRange range{};
  if (node == nullptr) {
    return range;
  }
  if (node->node != nullptr) {
    return node->node->range();
  }
  if (const HelperSpec* helper = helperSpecOf(node); helper != nullptr && helper->range != nullptr) {
    helper->range(node->helper.pointer, range);
  }
  return range;
}

/// The field index whose name matches, or -1.
int64_t fieldIndexOf(const SereAstNode* handle, std::string_view name) {
  const std::size_t count = fieldCountOf(handle);
  for (std::size_t index = 0; index < count; ++index) {
    FieldGetter get = nullptr;
    HelperGetter helperGet = nullptr;
    const char* fieldName = fieldSlotOf(handle, index, get, helperGet);
    if (fieldName != nullptr && name == fieldName) {
      return static_cast<int64_t>(index);
    }
  }
  return -1;
}

} // namespace
} // namespace sere

// ---------------------------------------------------------------------------
// Tree member definitions
// ---------------------------------------------------------------------------

namespace sere {
namespace {

/// The field at `index`, materialised into the handle's cache on first read.
///
/// The returned reference stays valid until the tree is freed: the cache is
/// sized once, before the first read, so an entry's address never moves. This is
/// what makes `sere_ast_field_text` able to hand out characters that outlive the
/// call, and it removes the repeated work of rebuilding a field value for every
/// element of a string list.
const FieldValue& cachedField(const SereAstNode* handle, std::size_t index) {
  static const FieldValue kEmpty{};
  if (handle == nullptr) {
    return kEmpty;
  }
  // Handles are read-only to a client, but the cache is bookkeeping the bridge
  // owns, like the tree's own handle and child-list pools.
  SereAstNode* mutableHandle = const_cast<SereAstNode*>(handle);
  if (!mutableHandle->fieldsReady) {
    mutableHandle->fields.resize(fieldCountOf(handle));
    mutableHandle->fieldsReady = true;
  }
  if (index >= mutableHandle->fields.size()) {
    return kEmpty;
  }
  FieldValue& slot = mutableHandle->fields[index];
  if (!slot.read) {
    slot = readField(handle, index);
    slot.read = true;
  }
  return slot;
}

/// Cached field read that also tolerates an out-of-range index, so a C caller
/// that passes -1 gets the empty value instead of a bad slot.
const FieldValue& fieldAt(const SereAstNode* handle, int64_t index) {
  if (index < 0) {
    return cachedField(nullptr, 0);
  }
  return cachedField(handle, static_cast<std::size_t>(index));
}

} // namespace
} // namespace sere

SereAstNode* SereAstTree::makeHandle(const sere::Node* node) {
  auto handle = std::make_unique<SereAstNode>();
  handle->node = node;
  handle->tree = this;
  SereAstNode* raw = handle.get();
  handles.push_back(std::move(handle));
  return raw;
}

SereAstNode* SereAstTree::makeHelper(const sere::HelperRef& ref) {
  auto handle = std::make_unique<SereAstNode>();
  handle->helper = ref;
  handle->tree = this;
  SereAstNode* raw = handle.get();
  handles.push_back(std::move(handle));
  return raw;
}

std::vector<SereAstNode*>& SereAstTree::listFor(const SereAstNode* handle, std::size_t index) {
  const auto found = listCache.find({handle, index});
  if (found != listCache.end()) {
    return *found->second;
  }
  auto owned = std::make_unique<std::vector<SereAstNode*>>();
  const sere::FieldValue& value = sere::cachedField(handle, index);
  owned->reserve(value.nodes.size() + value.helpers.size());
  for (const sere::Node* child : value.nodes) {
    owned->push_back(makeHandle(child));
  }
  for (const sere::HelperRef& helper : value.helpers) {
    owned->push_back(makeHelper(helper));
  }
  std::vector<SereAstNode*>* raw = owned.get();
  lists.push_back(std::move(owned));
  listCache[{handle, index}] = raw;
  return *raw;
}

// ---------------------------------------------------------------------------
// C ABI
// ---------------------------------------------------------------------------

namespace {

/// Diagnostics of the most recent failed parse. A thread-local keeps nested or
/// concurrent parses from overwriting each other's message.
std::string& lastErrorText() {
  static thread_local std::string text;
  return text;
}

/// The pool every string the bridge hands out lives in.
///
/// Sere's `str` is a pointer and a length, and the code generator builds the
/// value without copying, so the characters must stay valid for as long as the
/// program holds the string — which can be long after the tree is freed. The
/// pool therefore interns them for the life of the process, and each distinct
/// word is stored once: a field name, an operator word, or an identifier that
/// appears a thousand times in a tree shares one copy. `std::deque` keeps a
/// string's address stable while the pool grows, so an interning map may key on
/// a view into it.
const char* interned(std::string_view text) {
  static std::deque<std::string> pool;
  static std::unordered_map<std::string_view, const char*> index;
  static std::mutex lock;
  if (text.empty()) {
    return "";
  }
  const std::lock_guard<std::mutex> guard(lock);
  const auto found = index.find(text);
  if (found != index.end()) {
    return found->second;
  }
  pool.emplace_back(text);
  const char* stable = pool.back().c_str();
  index.emplace(std::string_view(stable, text.size()), stable);
  return stable;
}

} // namespace

extern "C" {

SereAstTree* sere_ast_parse(const char* source, int64_t length, const char* filename,
                            int32_t mode) {
  lastErrorText().clear();
  if (source == nullptr || length < 0) {
    lastErrorText() = "error: no source text to parse\n";
    return nullptr;
  }
  auto tree = std::make_unique<SereAstTree>();
  tree->source.assign(source, static_cast<std::size_t>(length));
  tree->filename = filename == nullptr || filename[0] == '\0' ? std::string("<unknown>")
                                                             : std::string(filename);
  tree->sourceManager = std::make_unique<sere::SourceManager>(tree->filename, tree->source);
  tree->diagnostics.setSource(tree->sourceManager.get());
  tree->diagnostics.setColorMode(sere::ColorMode::Never);

  sere::Lexer lexer(*tree->sourceManager, tree->diagnostics);
  std::vector<sere::Token> tokens = lexer.tokenizeAll();
  sere::Parser parser(tree->diagnostics, std::move(tokens), tree->sourceManager.get());
  if (mode == SERE_AST_MODE_EXPRESSION) {
    tree->expression = parser.parseTopExpr();
  } else {
    tree->module = parser.parseModule();
  }
  const bool empty = tree->module == nullptr && tree->expression == nullptr;
  if (empty || tree->diagnostics.hasErrors()) {
    std::string text = tree->diagnostics.format(*tree->sourceManager, false);
    if (text.empty()) {
      text = "error: the source could not be parsed\n";
    }
    lastErrorText() = std::move(text);
    return nullptr;
  }
  return tree.release();
}

void sere_ast_free(SereAstTree* tree) { delete tree; }

void sere_ast_last_error(const char** out_data, int64_t* out_len) {
  const char* text = interned(lastErrorText());
  if (out_data != nullptr) {
    *out_data = text;
  }
  if (out_len != nullptr) {
    *out_len = static_cast<int64_t>(std::strlen(text));
  }
}

const SereAstNode* sere_ast_root(const SereAstTree* tree) {
  if (tree == nullptr) {
    return nullptr;
  }
  SereAstTree* mutableTree = const_cast<SereAstTree*>(tree);
  if (tree->expression != nullptr) {
    return mutableTree->makeHandle(tree->expression.get());
  }
  if (tree->module != nullptr) {
    return mutableTree->makeHandle(tree->module.get());
  }
  return nullptr;
}

void sere_ast_source(const SereAstTree* tree, const char** out_data, int64_t* out_length) {
  const char* text = interned(tree == nullptr ? std::string_view{} : std::string_view(tree->source));
  if (out_data != nullptr) {
    *out_data = text;
  }
  if (out_length != nullptr) {
    *out_length = static_cast<int64_t>(std::strlen(text));
  }
}

void sere_ast_filename(const SereAstTree* tree, const char** out_data, int64_t* out_length) {
  const char* text =
      interned(tree == nullptr ? std::string_view{} : std::string_view(tree->filename));
  if (out_data != nullptr) {
    *out_data = text;
  }
  if (out_length != nullptr) {
    *out_length = static_cast<int64_t>(std::strlen(text));
  }
}

void sere_ast_kind(const SereAstNode* node, const char** out_data, int64_t* out_length) {
  const char* name = "Unknown";
  if (const sere::KindSpec* spec = sere::kindSpecOf(node); spec != nullptr) {
    name = spec->name;
  } else if (const sere::HelperSpec* helper = sere::helperSpecOf(node); helper != nullptr) {
    name = helper->name;
  }
  if (out_data != nullptr) {
    *out_data = name;
  }
  if (out_length != nullptr) {
    *out_length = static_cast<int64_t>(std::strlen(name));
  }
}

int32_t sere_ast_kind_id(const SereAstNode* node) {
  if (node == nullptr || node->node == nullptr) {
    return -1;
  }
  return static_cast<int32_t>(node->node->kind());
}

int32_t sere_ast_category(const SereAstNode* node) {
  if (const sere::KindSpec* spec = sere::kindSpecOf(node); spec != nullptr) {
    return spec->category;
  }
  return SERE_AST_CATEGORY_HELPER;
}

int64_t sere_ast_field_count(const SereAstNode* node) {
  return static_cast<int64_t>(sere::fieldCountOf(node));
}

void sere_ast_field_name(const SereAstNode* node, int64_t index, const char** out_data,
                         int64_t* out_length) {
  sere::FieldGetter get = nullptr;
  sere::HelperGetter helperGet = nullptr;
  const char* name =
      index < 0 ? nullptr
                : sere::fieldSlotOf(node, static_cast<std::size_t>(index), get, helperGet);
  if (name == nullptr) {
    name = "";
  }
  if (out_data != nullptr) {
    *out_data = name;
  }
  if (out_length != nullptr) {
    *out_length = static_cast<int64_t>(std::strlen(name));
  }
}

int64_t sere_ast_field_index(const SereAstNode* node, const char* name, int64_t length) {
  if (node == nullptr || name == nullptr || length <= 0) {
    return -1;
  }
  return sere::fieldIndexOf(node, std::string_view(name, static_cast<std::size_t>(length)));
}

int32_t sere_ast_field_kind(const SereAstNode* node, int64_t index) {
  if (node == nullptr) {
    return SERE_AST_VALUE_NONE;
  }
  return sere::fieldAt(node, index).kind;
}

const SereAstNode* sere_ast_field_node(const SereAstNode* node, int64_t index) {
  if (node == nullptr) {
    return nullptr;
  }
  const sere::FieldValue& value = sere::fieldAt(node, index);
  if (value.kind != SERE_AST_VALUE_NODE || value.node == nullptr) {
    return nullptr;
  }
  return node->tree->makeHandle(value.node);
}

int64_t sere_ast_field_node_count(const SereAstNode* node, int64_t index) {
  if (node == nullptr) {
    return 0;
  }
  const sere::FieldValue& value = sere::fieldAt(node, index);
  return static_cast<int64_t>(value.nodes.size() + value.helpers.size());
}

const SereAstNode* sere_ast_field_node_at(const SereAstNode* node, int64_t index, int64_t item) {
  if (node == nullptr || index < 0 || item < 0) {
    return nullptr;
  }
  const std::vector<SereAstNode*>& items =
      node->tree->listFor(node, static_cast<std::size_t>(index));
  if (static_cast<std::size_t>(item) >= items.size()) {
    return nullptr;
  }
  return items[static_cast<std::size_t>(item)];
}

void sere_ast_field_text(const SereAstNode* node, int64_t index, const char** out_data,
                         int64_t* out_length) {
  const sere::FieldValue& value = sere::fieldAt(node, index);
  const bool textual = value.kind == SERE_AST_VALUE_TEXT || value.kind == SERE_AST_VALUE_TOKEN;
  const char* data = textual ? interned(value.text) : "";
  if (out_data != nullptr) {
    *out_data = data;
  }
  if (out_length != nullptr) {
    *out_length = static_cast<int64_t>(std::strlen(data));
  }
}

int64_t sere_ast_field_text_count(const SereAstNode* node, int64_t index) {
  if (node == nullptr) {
    return 0;
  }
  const sere::FieldValue& value = sere::fieldAt(node, index);
  return value.kind == SERE_AST_VALUE_TEXTS ? static_cast<int64_t>(value.texts.size()) : 0;
}

void sere_ast_field_text_at(const SereAstNode* node, int64_t index, int64_t item,
                            const char** out_data, int64_t* out_length) {
  const char* data = "";
  if (node != nullptr && item >= 0) {
    const sere::FieldValue& value = sere::fieldAt(node, index);
    if (value.kind == SERE_AST_VALUE_TEXTS && static_cast<std::size_t>(item) < value.texts.size()) {
      data = interned(value.texts[static_cast<std::size_t>(item)]);
    }
  }
  if (out_data != nullptr) {
    *out_data = data;
  }
  if (out_length != nullptr) {
    *out_length = static_cast<int64_t>(std::strlen(data));
  }
}

int64_t sere_ast_field_int(const SereAstNode* node, int64_t index) {
  const sere::FieldValue& value = sere::fieldAt(node, index);
  return value.kind == SERE_AST_VALUE_INT && value.present ? value.integer : 0;
}

int32_t sere_ast_field_bool(const SereAstNode* node, int64_t index) {
  const sere::FieldValue& value = sere::fieldAt(node, index);
  return value.kind == SERE_AST_VALUE_BOOL && value.present && value.boolean ? 1 : 0;
}

double sere_ast_field_float(const SereAstNode* node, int64_t index) {
  const sere::FieldValue& value = sere::fieldAt(node, index);
  return value.kind == SERE_AST_VALUE_FLOAT && value.present ? value.real : 0.0;
}

int64_t sere_ast_range_start_line(const SereAstNode* node) {
  return static_cast<int64_t>(sere::rangeOf(node).start.line);
}

int64_t sere_ast_range_start_column(const SereAstNode* node) {
  return static_cast<int64_t>(sere::rangeOf(node).start.column);
}

int64_t sere_ast_range_end_line(const SereAstNode* node) {
  return static_cast<int64_t>(sere::rangeOf(node).end.line);
}

int64_t sere_ast_range_end_column(const SereAstNode* node) {
  return static_cast<int64_t>(sere::rangeOf(node).end.column);
}

int64_t sere_ast_kind_count(void) { return static_cast<int64_t>(sere::kKindCount); }

void sere_ast_kind_name_at(int64_t index, const char** out_data, int64_t* out_length) {
  const char* name = "Unknown";
  if (index >= 0 && static_cast<std::size_t>(index) < sere::kKindCount) {
    name = sere::specForIndex(static_cast<std::size_t>(index)).name;
  }
  if (out_data != nullptr) {
    *out_data = name;
  }
  if (out_length != nullptr) {
    *out_length = static_cast<int64_t>(std::strlen(name));
  }
}

int64_t sere_ast_field_count_of_kind(const char* kind_name, int64_t length) {
  if (kind_name == nullptr || length <= 0) {
    return -1;
  }
  const std::string_view wanted(kind_name, static_cast<std::size_t>(length));
  for (std::size_t index = 0; index < sere::kKindCount; ++index) {
    const sere::KindSpec& spec = sere::specForIndex(index);
    if (wanted == spec.name) {
      return static_cast<int64_t>(spec.fieldCount);
    }
  }
  return -1;
}

void sere_ast_field_name_of_kind(const char* kind_name, int64_t length, int64_t index,
                                 const char** out_data, int64_t* out_length) {  const char* name = "";
  if (kind_name != nullptr && length > 0 && index >= 0) {
    const std::string_view wanted(kind_name, static_cast<std::size_t>(length));
    for (std::size_t kind = 0; kind < sere::kKindCount; ++kind) {
      const sere::KindSpec& spec = sere::specForIndex(kind);
      if (wanted != spec.name) {
        continue;
      }
      if (static_cast<std::size_t>(index) < spec.fieldCount) {
        name = spec.fields[index].name;
      }
      break;
    }
  }
  if (out_data != nullptr) {
    *out_data = name;
  }
  if (out_length != nullptr) {
    *out_length = static_cast<int64_t>(std::strlen(name));
  }
}

int32_t sere_ast_kind_category(const char* kind_name, int64_t length) {
  if (kind_name == nullptr || length <= 0) {
    return SERE_AST_CATEGORY_HELPER;
  }
  const std::string_view wanted(kind_name, static_cast<std::size_t>(length));
  for (std::size_t index = 0; index < sere::kKindCount; ++index) {
    const sere::KindSpec& spec = sere::specForIndex(index);
    if (wanted == spec.name) {
      return spec.category;
    }
  }
  return SERE_AST_CATEGORY_HELPER;
}

} // extern "C"
