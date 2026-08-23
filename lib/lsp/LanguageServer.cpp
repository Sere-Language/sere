/// @file LanguageServer.cpp
/// JSON-RPC language server for diagnostics, hover, and completion.

#include "sere/lsp/LanguageServer.h"

#include "sere/Version.h"
#include "sere/ast/Query.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/driver/Frontend.h"
#include "sere/driver/ImportPath.h"
#include "sere/driver/Prelude.h"
#include "sere/driver/Project.h"
#include "sere/driver/SourceOverlay.h"
#include "sere/driver/Toolchain.h"
#include "sere/lsp/ImportCompletion.h"
#include "sere/lsp/SemanticTokens.h"
#include "sere/types/Type.h"
#include "sere/sema/TypeChecker.h"
#include "sere/source/SourceManager.h"

#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace sere {
namespace {

constexpr int kLspSyncIncremental = 2;
constexpr int kCompletionMethod = 2;
constexpr int kCompletionKeyword = 14;
constexpr int kCompletionFunction = 3;
constexpr int kCompletionVariable = 6;
constexpr int kCompletionClass = 7;
constexpr int kCompletionModule = 9;
constexpr int kCompletionEnum = 13;
constexpr int kCompletionType = 25;
constexpr int kCompletionField = 5;
constexpr int kCompletionStruct = 22;
constexpr int kCompletionMacro = 3;

constexpr int kSymbolMethod = 6;
constexpr int kSymbolClass = 5;
constexpr int kSymbolField = 8;
constexpr int kSymbolEnum = 10;
constexpr int kSymbolFunction = 12;
constexpr int kSymbolEnumMember = 22;
constexpr int kSymbolStruct = 23;
constexpr int kSymbolType = 26;
constexpr int kInlayParameter = 2;

void setStdioBinary() {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
}

[[nodiscard]] char percentDecodeNibble(char digit) {
  if (digit >= '0' && digit <= '9') {
    return static_cast<char>(digit - '0');
  }
  if (digit >= 'A' && digit <= 'F') {
    return static_cast<char>(digit - 'A' + 10);
  }
  if (digit >= 'a' && digit <= 'f') {
    return static_cast<char>(digit - 'a' + 10);
  }
  return 0;
}

[[nodiscard]] std::string percentDecode(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '%' && index + 2 < text.size()) {
      const char high = percentDecodeNibble(text[index + 1]);
      const char low = percentDecodeNibble(text[index + 2]);
      out.push_back(static_cast<char>((high << 4) | low));
      index += 2;
      continue;
    }
    out.push_back(text[index]);
  }
  return out;
}

[[nodiscard]] std::string uriToPath(std::string_view uri) {
  std::string_view rest = uri;
  if (rest.starts_with("file://")) {
    rest.remove_prefix(7);
    while (rest.starts_with('/')) {
      if (rest.size() >= 3 && rest[1] == ':') {
        break;
      }
      if (rest.size() >= 2 && rest[0] == '/' && rest[1] != '/') {
        rest.remove_prefix(1);
        break;
      }
      rest.remove_prefix(1);
    }
  }
  std::string path = percentDecode(rest);
#ifdef _WIN32
  for (char& ch : path) {
    if (ch == '/') {
      ch = '\\';
    }
  }
#endif
  return path;
}

[[nodiscard]] llvm::json::Object lspPosition(const SourceLocation& location) {
  const std::uint32_t line = location.line == 0 ? 0 : location.line - 1;
  const std::uint32_t column = location.column == 0 ? 0 : location.column - 1;
  return llvm::json::Object{{"line", static_cast<int64_t>(line)},
                            {"character", static_cast<int64_t>(column)}};
}

[[nodiscard]] llvm::json::Object lspRange(SourceRange range) {
  return llvm::json::Object{{"start", lspPosition(range.start)}, {"end", lspPosition(range.end)}};
}

[[nodiscard]] std::size_t offsetFromPosition(const std::string& text, int line, int character) {
  std::size_t offset = 0;
  int currentLine = 0;
  while (offset < text.size() && currentLine < line) {
    if (text[offset] == '\n') {
      ++currentLine;
    }
    ++offset;
  }
  return std::min(text.size(), offset + static_cast<std::size_t>(std::max(character, 0)));
}

void applyContentChange(std::string& text, const llvm::json::Object& change) {
  const std::optional<llvm::StringRef> next = change.getString("text");
  if (!next.has_value()) {
    return;
  }
  const llvm::json::Object* range = change.getObject("range");
  if (range == nullptr) {
    text = next->str();
    return;
  }
  const llvm::json::Object* start = range->getObject("start");
  const llvm::json::Object* end = range->getObject("end");
  if (start == nullptr || end == nullptr) {
    text = next->str();
    return;
  }
  const std::optional<int64_t> startLine = start->getInteger("line");
  const std::optional<int64_t> startChar = start->getInteger("character");
  const std::optional<int64_t> endLine = end->getInteger("line");
  const std::optional<int64_t> endChar = end->getInteger("character");
  if (!startLine.has_value() || !startChar.has_value() || !endLine.has_value() ||
      !endChar.has_value()) {
    text = next->str();
    return;
  }
  const std::size_t begin =
      offsetFromPosition(text, static_cast<int>(*startLine), static_cast<int>(*startChar));
  const std::size_t stop =
      offsetFromPosition(text, static_cast<int>(*endLine), static_cast<int>(*endChar));
  text.replace(begin, stop - begin, next->str());
}

[[nodiscard]] int lspSeverity(DiagnosticSeverity severity) {
  if (severity == DiagnosticSeverity::Warning) {
    return 2;
  }
  if (severity == DiagnosticSeverity::Note) {
    return 3;
  }
  return 1;
}

[[nodiscard]] std::size_t parseContentLength(std::string_view line) {
  std::size_t length = 0;
  bool seenDigit = false;
  for (const char ch : line) {
    if (ch == ' ' || ch == '\t') {
      if (seenDigit) {
        break;
      }
      continue;
    }
    if (ch < '0' || ch > '9') {
      break;
    }
    seenDigit = true;
    length = length * 10 + static_cast<std::size_t>(ch - '0');
  }
  return length;
}

[[nodiscard]] bool readMessage(std::string& body) {
  std::string line;
  std::size_t length = 0;
  bool sawLength = false;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    constexpr std::string_view prefix = "Content-Length:";
    if (line.starts_with(prefix)) {
      length = parseContentLength(std::string_view(line).substr(prefix.size()));
      sawLength = true;
    }
  }
  if (!sawLength || !std::cin) {
    return false;
  }
  body.resize(length);
  std::cin.read(body.data(), static_cast<std::streamsize>(length));
  return static_cast<std::size_t>(std::cin.gcount()) == length;
}

void writeMessage(llvm::json::Value payload) {
  std::string body;
  llvm::raw_string_ostream stream(body);
  stream << payload;
  stream.flush();
  llvm::outs() << "Content-Length: " << body.size() << "\r\n\r\n" << body;
  llvm::outs().flush();
}

void writeResult(const llvm::json::Value* id, llvm::json::Value result) {
  llvm::json::Object message{{"jsonrpc", "2.0"}, {"result", std::move(result)}};
  if (id != nullptr) {
    message["id"] = *id;
  }
  writeMessage(llvm::json::Value(std::move(message)));
}

void writeNullResult(const llvm::json::Value* id) {
  writeResult(id, nullptr);
}

[[nodiscard]] std::string formatMethod(const RecordMethod& method) {
  std::string text = method.name + "(";
  const Type* fn = method.type;
  const std::size_t start =
      !method.paramNames.empty() && method.paramNames[0] == "self" ? 1 : 0;
  if (fn != nullptr) {
    for (std::size_t index = start; index < fn->paramTypes().size(); ++index) {
      if (index > start) {
        text += ", ";
      }
      if (index < method.paramNames.size()) {
        text += method.paramNames[index] + ": ";
      }
      text += fn->paramTypes()[index] == nullptr ? "?" : fn->paramTypes()[index]->display();
    }
    text += ") -> ";
    text += fn->returnType() == nullptr ? "void" : fn->returnType()->display();
  } else {
    text += ")";
  }
  return text;
}

[[nodiscard]] std::string formatClass(const Type& type) {
  std::string kind = "class";
  if (type.isEnum()) {
    kind = "enum";
  } else if (type.isStruct()) {
    kind = "struct";
  }
  std::string text = kind + " " + type.name() + ":\n";
  for (const RecordField& field : type.fields()) {
    text += "    " + field.name + ": " + (field.type == nullptr ? "?" : field.type->display()) +
            "\n";
  }
  for (const RecordMethod& method : type.methods()) {
    text += "    def " + formatMethod(method) + "\n";
  }
  return text;
}

[[nodiscard]] std::string hoverText(const Node& node) {
  if (node.kind() == NodeKind::MacroDef) {
    return formatMacro(static_cast<const MacroDef&>(node));
  }
  if (node.kind() == NodeKind::SpliceExpr) {
    return "$" + static_cast<const SpliceExpr&>(node).name() + "  (macro splice)";
  }
  if (node.kind() == NodeKind::UnaryExpr) {
    const auto& unary = static_cast<const UnaryExpr&>(node);
    if (node.resolvedType() != nullptr &&
        (unary.op() == UnaryOp::Deref || unary.op() == UnaryOp::AddrOf)) {
      std::string label = unary.op() == UnaryOp::Deref ? "*" : "&";
      if (unary.operand().kind() == NodeKind::NameExpr) {
        label += static_cast<const NameExpr&>(unary.operand()).name();
      }
      return label + ": " + node.resolvedType()->display();
    }
  }
  if (node.kind() == NodeKind::MatchStmt) {
    const auto& match = static_cast<const MatchStmt&>(node);
    std::string text = "match";
    if (match.subject().resolvedType() != nullptr) {
      text += " " + match.subject().resolvedType()->display();
    }
    text += "\n" + std::to_string(match.arms().size()) +
            (match.arms().size() == 1 ? " case" : " cases");
    return text;
  }
  if (node.kind() == NodeKind::LambdaExpr) {
    if (node.resolvedType() != nullptr) {
      return "lambda: " + node.resolvedType()->display();
    }
    return "lambda";
  }
  if (node.kind() == NodeKind::WalrusExpr) {
    const auto& walrus = static_cast<const WalrusExpr&>(node);
    std::string text = walrus.name() + " :=";
    if (node.resolvedType() != nullptr) {
      text += " " + node.resolvedType()->display();
    }
    return text;
  }
  if (node.kind() == NodeKind::WithStmt) {
    return "with";
  }
  if (node.kind() == NodeKind::NameExpr) {
    const auto& name = static_cast<const NameExpr&>(node);
    if (node.resolvedType() != nullptr && node.resolvedType()->kind() == TypeKind::Alias) {
      return name.name() + ": " + node.resolvedType()->display() + " = " +
             node.resolvedType()->canonical()->display();
    }
    if (node.resolvedType() != nullptr && node.resolvedType()->isRecord()) {
      return formatClass(*node.resolvedType()->canonical());
    }
    if (name.name() == "_") {
      if (node.resolvedType() != nullptr) {
        return "_  (wildcard " + node.resolvedType()->display() + ")";
      }
      return "_  (wildcard)";
    }
    if (node.resolvedType() != nullptr) {
      if (!name.compileTimeText().empty()) {
        return name.name() + ": " + node.resolvedType()->display() + " = \"" +
               name.compileTimeText() + "\"";
      }
      return name.name() + ": " + node.resolvedType()->display();
    }
    return name.name();
  }
  if (node.kind() == NodeKind::ClassDef) {
    if (node.resolvedType() != nullptr) {
      return formatClass(*node.resolvedType());
    }
  }
  if (node.kind() == NodeKind::FunctionDef) {
    const auto& function = static_cast<const FunctionDef&>(node);
    std::string text = "def " + function.name() + "(";
    for (std::size_t index = 0; index < function.params().size(); ++index) {
      if (index != 0) {
        text += ", ";
      }
      text += function.params()[index].name;
      if (function.params()[index].type != nullptr) {
        text += ": " + function.params()[index].type->name();
      }
    }
    text += ") -> " + function.returnType().name();
    if (function.hasInferredReturn()) {
      text += "  (inferred)";
    }
    return text;
  }
  if (node.kind() == NodeKind::TypeAlias) {
    const auto& alias = static_cast<const TypeAlias&>(node);
    return "type " + alias.name() + " = " + alias.type().name();
  }
  if (node.resolvedType() != nullptr && node.resolvedType()->kind() == TypeKind::Record) {
    return formatClass(*node.resolvedType());
  }
  if (node.resolvedType() != nullptr) {
    return node.resolvedType()->display();
  }
  if (node.kind() == NodeKind::TypeExpr) {
    return static_cast<const TypeExpr&>(node).name();
  }
  return {};
}

[[nodiscard]] const SemanticSymbol* findNamedSymbol(TypeChecker* checker, const std::string& name,
                                                    std::string_view kind = {}) {
  if (checker == nullptr || name.empty()) {
    return nullptr;
  }
  const SemanticSymbol* match = nullptr;
  for (const SemanticSymbol& symbol : checker->symbols()) {
    if (symbol.name != name) {
      continue;
    }
    if (!kind.empty() && symbol.kind != kind) {
      continue;
    }
    match = &symbol;
    if (symbol.kind == "macro") {
      return &symbol;
    }
  }
  return match;
}

void appendMacroUseRefs(const std::vector<MacroUse>& uses, const std::string& name,
                        std::vector<SourceRange>& refs) {
  for (const MacroUse& use : uses) {
    if (use.name == name) {
      refs.push_back(use.nameRange);
    }
  }
}

[[nodiscard]] const Type* recordTypeOf(const Node& node) {
  if (node.kind() == NodeKind::UnaryExpr) {
    const auto& unary = static_cast<const UnaryExpr&>(node);
    if (unary.op() == UnaryOp::Deref && node.resolvedType() != nullptr) {
      return node.resolvedType()->canonical();
    }
  }
  const Type* type = node.resolvedType();
  if (type != nullptr && type->isRecord()) {
    return type->canonical();
  }
  if (node.kind() == NodeKind::MemberExpr) {
    const Type* object = static_cast<const MemberExpr&>(node).object().resolvedType();
    return object == nullptr ? nullptr : object->canonical();
  }
  return type == nullptr ? nullptr : type->canonical();
}

[[nodiscard]] std::string identifierPrefix(std::string_view text, std::uint32_t offset) {
  std::size_t end = offset > text.size() ? text.size() : static_cast<std::size_t>(offset);
  std::size_t start = end;
  while (start > 0) {
    const unsigned char ch = static_cast<unsigned char>(text[start - 1]);
    if (!(std::isalnum(ch) != 0 || ch == '_')) {
      break;
    }
    --start;
  }
  return std::string(text.substr(start, end - start));
}

struct CallSite {
  std::string callee;
  std::size_t active = 0;
  bool method = false;
};

[[nodiscard]] CallSite scanCallSite(std::string_view text, std::size_t cursor) {
  CallSite site;
  if (cursor > text.size()) {
    cursor = text.size();
  }
  int depth = 0;
  bool inString = false;
  char quote = '\0';
  std::size_t open = cursor;
  std::size_t commas = 0;
  while (open > 0) {
    --open;
    const char ch = text[open];
    if (inString) {
      if (ch == quote && (open == 0 || text[open - 1] != '\\')) {
        inString = false;
      }
      continue;
    }
    if (ch == '"' || ch == '\'') {
      inString = true;
      quote = ch;
      continue;
    }
    if (ch == ')') {
      ++depth;
    } else if (ch == '(') {
      if (depth == 0) {
        break;
      }
      --depth;
    } else if (ch == ',' && depth == 0) {
      ++commas;
    }
  }
  std::size_t nameAt = open;
  if (nameAt > 0 && text[nameAt - 1] == '!') {
    --nameAt;
  }
  site.callee = identifierPrefix(text, static_cast<std::uint32_t>(nameAt));
  if (nameAt >= site.callee.size() && nameAt - site.callee.size() > 0) {
    site.method = text[nameAt - site.callee.size() - 1] == '.';
  }
  site.active = commas;
  return site;
}

[[nodiscard]] llvm::json::Object makeSignature(const std::string& name,
                                               const std::vector<std::string>& params,
                                               const std::vector<std::string>& types,
                                               const std::string& returnType, bool macro) {
  std::string label = name;
  label += macro ? "!(" : "(";
  llvm::json::Array parameters;
  for (std::size_t index = 0; index < params.size(); ++index) {
    if (index != 0) {
      label += ", ";
    }
    const std::size_t start = label.size();
    label += params[index];
    if (index < types.size() && !types[index].empty()) {
      label += ": " + types[index];
    }
    const std::size_t end = label.size();
    parameters.push_back(llvm::json::Object{
        {"label", llvm::json::Array{static_cast<int64_t>(start), static_cast<int64_t>(end)}}});
  }
  label += ")";
  if (!returnType.empty()) {
    label += " -> " + returnType;
  }
  return llvm::json::Object{{"label", std::move(label)}, {"parameters", std::move(parameters)}};
}

[[nodiscard]] const SemanticSymbol* findCallable(const TypeChecker& checker,
                                                 const std::string& name, bool method,
                                                 const std::string& container) {
  const SemanticSymbol* fallback = nullptr;
  for (const SemanticSymbol& symbol : checker.symbols()) {
    if (symbol.name != name) {
      continue;
    }
    const bool callable = symbol.kind == "function" || symbol.kind == "method" ||
                          symbol.kind == "class" || symbol.kind == "struct" ||
                          symbol.kind == "enum" || symbol.kind == "macro";
    if (!callable) {
      continue;
    }
    if (!container.empty() && symbol.container == container) {
      return &symbol;
    }
    if (method && symbol.kind == "method") {
      return &symbol;
    }
    if (!method && symbol.kind != "method") {
      return &symbol;
    }
    if (fallback == nullptr) {
      fallback = &symbol;
    }
  }
  return fallback;
}

void collectCallableParams(const SemanticSymbol& match, std::vector<std::string>& params,
                           std::vector<std::string>& types, std::string& returnType) {
  params = match.paramNames;
  types = match.paramTypes;
  returnType = match.returnType;
  if (!params.empty() && params[0] == "self") {
    params.erase(params.begin());
    if (!types.empty()) {
      types.erase(types.begin());
    }
  }
}

[[nodiscard]] bool hasDotBefore(std::string_view text, std::uint32_t offset, const std::string& prefix) {
  if (offset < prefix.size()) {
    return false;
  }
  const std::size_t prefixStart = static_cast<std::size_t>(offset) - prefix.size();
  return prefixStart > 0 && text[prefixStart - 1] == '.';
}

void addCompletion(llvm::json::Array& items,
                   const std::string& label,
                   int kind,
                   const std::string& detail,
                   const std::string& prefix,
                   const std::string& insertText = {},
                   const std::string& sortText = {},
                   bool snippet = true) {
  if (!prefix.empty() && !label.starts_with(prefix) &&
      (insertText.empty() || !insertText.starts_with(prefix))) {
    return;
  }
  llvm::json::Object item{{"label", label}, {"kind", kind}};
  if (!detail.empty()) {
    item["detail"] = detail;
  }
  if (!insertText.empty()) {
    item["insertText"] = insertText;
    if (snippet) {
      item["insertTextFormat"] = 2;
    }
  }
  if (!sortText.empty()) {
    item["sortText"] = sortText;
  }
  items.push_back(std::move(item));
}

[[nodiscard]] int completionKind(ImportItemKind kind) {
  switch (kind) {
  case ImportItemKind::Module:
  case ImportItemKind::Package:
    return kCompletionModule;
  case ImportItemKind::Function:
  case ImportItemKind::Macro:
    return kCompletionFunction;
  case ImportItemKind::Class:
    return kCompletionClass;
  case ImportItemKind::Struct:
    return kCompletionStruct;
  case ImportItemKind::Enum:
    return kCompletionEnum;
  case ImportItemKind::Type:
    return kCompletionType;
  case ImportItemKind::Keyword:
    return kCompletionKeyword;
  case ImportItemKind::Variable:
    return kCompletionVariable;
  }
  return kCompletionVariable;
}

void addImportCompletionItems(llvm::json::Array& items,
                              const std::vector<ImportCompletionItem>& candidates,
                              const std::string& prefix) {
  for (const ImportCompletionItem& candidate : candidates) {
    addCompletion(items, candidate.label, completionKind(candidate.kind), candidate.detail, prefix,
                  candidate.insertText, candidate.sortText, false);
  }
}

void addKeywordCompletions(llvm::json::Array& items, const std::string& prefix) {
  const char* keywords[] = {"def",    "class",  "struct", "type",   "return", "if",     "elif",
                            "else",   "while",  "pass",   "and",    "or",     "not",
                            "True",   "False",  "None",   "extern", "break",  "continue",
                            "import", "from",   "as",     "static", "abstract", "override",
                            "enum",   "for",    "in",     "is",     "assert", "try", "except",
                            "finally", "raise", "match", "case", "lambda", "with", "defer",
                            "del", "const", "macro", "quote", "__name__", "__file__", "__package__",
                            "__doc__",
                            "__debug__", "__sere_version__", "__windows__", "__linux__",
                            "__macos__", "__unix__", "__x86_64__", "__arm64__",
                            "__platform__", "__arch__"};
  for (const char* keyword : keywords) {
    addCompletion(items, keyword, kCompletionKeyword, "keyword", prefix, {}, "1" + std::string(keyword));
  }
}

void addTypeCompletions(llvm::json::Array& items, const std::string& prefix) {
  const char* types[] = {"void", "None", "Any", "bool", "i8",     "i16",   "i32",  "i64", "u8",     "u16",
                         "u32",  "u64",  "f32",    "f64",   "str",  "regex", "byte", "Unique", "Shared",
                         "Ptr",  "list", "array", "dict", "enum"};
  for (const char* typeName : types) {
    std::string insert;
    if (std::string_view(typeName) == "Unique" || std::string_view(typeName) == "Shared" ||
        std::string_view(typeName) == "Ptr" || std::string_view(typeName) == "list" ||
        std::string_view(typeName) == "array") {
      insert = std::string(typeName) + "[${1:T}]";
    } else if (std::string_view(typeName) == "dict") {
      insert = "dict[${1:K}, ${2:V}]";
    }
    addCompletion(items, typeName, kCompletionType, "type", prefix, insert);
  }
}

class LanguageSession {
public:
  LanguageSession() { applyLanguageContext(std::filesystem::current_path()); }

  [[nodiscard]] bool handleMessage(const llvm::json::Object& message);

private:
  void publishDiagnostics(const std::string& uri, const DiagnosticEngine& diagnostics);
  void rebuildOverlay();
  void analyzeDocument(const std::string& uri);
  void analyzeOpenDocuments();
  void analyzeDependents(const std::string& changedUri);
  [[nodiscard]] bool documentImports(const std::string& uri,
                                     const std::filesystem::path& path) const;
  [[nodiscard]] bool isContextAffecting(const std::filesystem::path& path) const;
  void handleInitialize(const llvm::json::Value* id, const llvm::json::Object* params);
  void captureWorkspaceRoot(const llvm::json::Object& params);
  void applyInitializeOptions(const llvm::json::Object& params);
  void applyClientConfiguration(const llvm::json::Object* settings);
  void applyLanguageContext(const std::filesystem::path& start);
  void fillImportCompletions(llvm::json::Array& items,
                             const ImportCompletionQuery& query,
                             const std::string& uri);
  void handleDidOpen(const llvm::json::Object& params);
  void handleDidChange(const llvm::json::Object& params);
  void handleDidClose(const llvm::json::Object& params);
  void handleDidChangeWatchedFiles(const llvm::json::Object& params);
  void handleDidChangeConfiguration(const llvm::json::Object& params);
  void handleDidChangeWorkspaceFolders(const llvm::json::Object& params);
  void handleHover(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleCompletion(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleDefinition(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleDocumentSymbol(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleSignatureHelp(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleInlayHint(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleReferences(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleCodeLens(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleRename(const llvm::json::Value* id, const llvm::json::Object& params);
  void handlePrepareRename(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleDocumentHighlight(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleWorkspaceSymbol(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleSemanticTokens(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleFoldingRange(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleFormatting(const llvm::json::Value* id, const llvm::json::Object& params);
  void handleCodeAction(const llvm::json::Value* id, const llvm::json::Object& params);
  [[nodiscard]] Frontend* analyzeCached(const std::string& uri);
  [[nodiscard]] std::optional<std::pair<std::string, std::uint32_t>>
  documentOffset(const llvm::json::Object& params) const;

  std::filesystem::path stdlibDir_;
  std::filesystem::path configuredStdlib_{};
  std::filesystem::path workspaceRoot_{};
  std::string contextStamp_{};
  SourceOverlay overlay_{};
  std::unordered_map<std::string, std::string> documents_{};
  std::unordered_map<std::string, std::unique_ptr<Frontend>> frontends_{};
  bool contextDirty_ = false;
  bool shutdown_ = false;
};

bool LanguageSession::handleMessage(const llvm::json::Object& message) {
  const std::optional<llvm::StringRef> method = message.getString("method");
  const llvm::json::Value* id = message.get("id");
  const llvm::json::Object* params = message.getObject("params");
  if (!method.has_value()) {
    return true;
  }
  if (*method == "initialize") {
    handleInitialize(id, params);
    return true;
  }
  if (*method == "initialized" || *method == "textDocument/didSave") {
    return true;
  }
  if (*method == "shutdown") {
    shutdown_ = true;
    writeNullResult(id);
    return true;
  }
  if (*method == "exit") {
    return false;
  }
  if (*method == "textDocument/didOpen" && params != nullptr) {
    handleDidOpen(*params);
    return true;
  }
  if (*method == "textDocument/didChange" && params != nullptr) {
    handleDidChange(*params);
    return true;
  }
  if (*method == "textDocument/didClose" && params != nullptr) {
    handleDidClose(*params);
    return true;
  }
  if (*method == "workspace/didChangeWatchedFiles" && params != nullptr) {
    handleDidChangeWatchedFiles(*params);
    return true;
  }
  if (*method == "workspace/didChangeConfiguration" && params != nullptr) {
    handleDidChangeConfiguration(*params);
    return true;
  }
  if (*method == "workspace/didChangeWorkspaceFolders" && params != nullptr) {
    handleDidChangeWorkspaceFolders(*params);
    return true;
  }
  if (*method == "textDocument/hover" && params != nullptr) {
    handleHover(id, *params);
    return true;
  }
  if (*method == "textDocument/completion" && params != nullptr) {
    handleCompletion(id, *params);
    return true;
  }
  if (*method == "textDocument/definition" && params != nullptr) {
    handleDefinition(id, *params);
    return true;
  }
  if (*method == "textDocument/documentSymbol" && params != nullptr) {
    handleDocumentSymbol(id, *params);
    return true;
  }
  if (*method == "textDocument/signatureHelp" && params != nullptr) {
    handleSignatureHelp(id, *params);
    return true;
  }
  if (*method == "textDocument/inlayHint" && params != nullptr) {
    handleInlayHint(id, *params);
    return true;
  }
  if (*method == "textDocument/references" && params != nullptr) {
    handleReferences(id, *params);
    return true;
  }
  if (*method == "textDocument/codeLens" && params != nullptr) {
    handleCodeLens(id, *params);
    return true;
  }
  if (*method == "textDocument/rename" && params != nullptr) {
    handleRename(id, *params);
    return true;
  }
  if (*method == "textDocument/prepareRename" && params != nullptr) {
    handlePrepareRename(id, *params);
    return true;
  }
  if (*method == "textDocument/documentHighlight" && params != nullptr) {
    handleDocumentHighlight(id, *params);
    return true;
  }
  if (*method == "textDocument/typeDefinition" && params != nullptr) {
    handleDefinition(id, *params);
    return true;
  }
  if (*method == "textDocument/implementation" && params != nullptr) {
    handleDefinition(id, *params);
    return true;
  }
  if (*method == "workspace/symbol" && params != nullptr) {
    handleWorkspaceSymbol(id, *params);
    return true;
  }
  if (*method == "textDocument/semanticTokens/full" && params != nullptr) {
    handleSemanticTokens(id, *params);
    return true;
  }
  if (*method == "textDocument/semanticTokens/range" && params != nullptr) {
    handleSemanticTokens(id, *params);
    return true;
  }
  if (*method == "textDocument/foldingRange" && params != nullptr) {
    handleFoldingRange(id, *params);
    return true;
  }
  if (*method == "textDocument/formatting" && params != nullptr) {
    handleFormatting(id, *params);
    return true;
  }
  if (*method == "textDocument/rangeFormatting" && params != nullptr) {
    handleFormatting(id, *params);
    return true;
  }
  if (*method == "textDocument/codeAction" && params != nullptr) {
    handleCodeAction(id, *params);
    return true;
  }
  if (id != nullptr) {
    writeNullResult(id);
  }
  return !shutdown_;
}

void LanguageSession::publishDiagnostics(const std::string& uri,
                                         const DiagnosticEngine& diagnostics) {
  llvm::json::Array items;
  for (const Diagnostic& diagnostic : diagnostics.diagnostics()) {
    items.push_back(llvm::json::Object{
        {"range", lspRange(diagnostic.range)},
        {"severity", lspSeverity(diagnostic.severity)},
        {"source", "sere"},
        {"code", std::string(diagnosticCodeName(diagnostic.code))},
        {"message", diagnostic.help.empty()
                        ? diagnostic.message
                        : diagnostic.message + "\n= help: " + diagnostic.help},
    });
  }
  writeMessage(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/publishDiagnostics"},
      {"params", llvm::json::Object{{"uri", uri}, {"diagnostics", std::move(items)}}},
  });
}

void LanguageSession::rebuildOverlay() {
  overlay_.clear();
  for (const auto& [uri, text] : documents_) {
    overlay_.set(uriToPath(uri), text);
  }
}

void LanguageSession::analyzeDocument(const std::string& uri) {
  const auto found = documents_.find(uri);
  if (found == documents_.end()) {
    return;
  }
  auto frontend = std::make_unique<Frontend>();
  const std::filesystem::path file = uriToPath(uri);
  applyLanguageContext(file);
  rebuildOverlay();
  (void)frontend->analyze(file.string(), found->second, stdlibDir_, &overlay_);
  publishDiagnostics(uri, frontend->diagnostics());
  frontends_[uri] = std::move(frontend);
}

void LanguageSession::analyzeOpenDocuments() {
  rebuildOverlay();
  std::vector<std::string> uris;
  uris.reserve(documents_.size());
  for (const auto& [uri, _] : documents_) {
    uris.push_back(uri);
  }
  for (const std::string& uri : uris) {
    analyzeDocument(uri);
  }
}

bool LanguageSession::documentImports(const std::string& uri,
                                      const std::filesystem::path& path) const {
  const auto found = frontends_.find(uri);
  if (found == frontends_.end() || found->second == nullptr) {
    return false;
  }
  for (const std::filesystem::path& imported : found->second->importedModulePaths()) {
    if (SourceOverlay::normalize(imported) == SourceOverlay::normalize(path)) {
      return true;
    }
  }
  return false;
}

bool LanguageSession::isContextAffecting(const std::filesystem::path& path) const {
  if (path.empty()) {
    return false;
  }
  const LanguageContext context = resolveLanguageContext(
      !path.empty() ? path : (!workspaceRoot_.empty() ? workspaceRoot_ : std::filesystem::current_path()));
  if (isLanguageContextPath(path, context)) {
    return true;
  }
  if (!stdlibDir_.empty() && pathIsUnderDirectory(path, stdlibDir_)) {
    return true;
  }
  if (!configuredStdlib_.empty() && pathIsUnderDirectory(path, configuredStdlib_)) {
    return true;
  }
  return path.filename() == "sere.toml";
}

void LanguageSession::analyzeDependents(const std::string& changedUri) {
  const std::filesystem::path changed = uriToPath(changedUri);
  const bool refreshAll = contextDirty_ || isContextAffecting(changed);
  contextDirty_ = false;
  std::vector<std::string> uris;
  for (const auto& [uri, _] : documents_) {
    if (uri == changedUri) {
      continue;
    }
    if (refreshAll || documentImports(uri, changed)) {
      uris.push_back(uri);
    }
  }
  for (const std::string& uri : uris) {
    analyzeDocument(uri);
  }
}

Frontend* LanguageSession::analyzeCached(const std::string& uri) {
  const auto found = frontends_.find(uri);
  if (found != frontends_.end()) {
    return found->second.get();
  }
  analyzeDocument(uri);
  const auto created = frontends_.find(uri);
  if (created == frontends_.end()) {
    return nullptr;
  }
  return created->second.get();
}

void LanguageSession::applyInitializeOptions(const llvm::json::Object& params) {
  const llvm::json::Object* options = params.getObject("initializationOptions");
  if (options == nullptr) {
    return;
  }
  if (const std::optional<llvm::StringRef> path = options->getString("stdlibPath")) {
    if (!path->empty()) {
      configuredStdlib_ = path->str();
    }
  }
}

void LanguageSession::applyClientConfiguration(const llvm::json::Object* settings) {
  if (settings == nullptr) {
    return;
  }
  const llvm::json::Object* sere = settings->getObject("sere");
  const llvm::json::Object* root = sere != nullptr ? sere : settings;
  if (const std::optional<llvm::StringRef> path = root->getString("stdlibPath")) {
    configuredStdlib_ = path->empty() ? std::filesystem::path{} : std::filesystem::path(path->str());
  }
}

void LanguageSession::applyLanguageContext(const std::filesystem::path& start) {
  const std::filesystem::path probe =
      !start.empty() ? start : (!workspaceRoot_.empty() ? workspaceRoot_ : std::filesystem::current_path());
  const LanguageContext context = resolveLanguageContext(probe);
  if (!configuredStdlib_.empty()) {
    std::error_code error;
    if (std::filesystem::exists(configuredStdlib_ / "prelude.sere", error)) {
      stdlibDir_ = configuredStdlib_;
    } else {
      stdlibDir_ = context.stdlib;
    }
  } else {
    stdlibDir_ = context.stdlib;
  }
  if (context.project.has_value() && workspaceRoot_.empty()) {
    workspaceRoot_ = context.project->root;
  }
  std::string stamp = languageContextStamp(context);
  if (!stdlibDir_.empty()) {
    stamp += '|';
    stamp += SourceOverlay::normalize(stdlibDir_);
  }
  if (stamp != contextStamp_) {
    contextDirty_ = !contextStamp_.empty();
    contextStamp_ = std::move(stamp);
  }
}

void LanguageSession::captureWorkspaceRoot(const llvm::json::Object& params) {
  if (const std::optional<llvm::StringRef> uri = params.getString("rootUri")) {
    if (!uri->empty() && *uri != "null") {
      workspaceRoot_ = uriToPath(uri->str());
    }
  }
  if (workspaceRoot_.empty()) {
    if (const std::optional<llvm::StringRef> path = params.getString("rootPath")) {
      workspaceRoot_ = std::filesystem::path(path->str());
    }
  }
  if (!workspaceRoot_.empty()) {
    return;
  }
  const llvm::json::Array* folders = params.getArray("workspaceFolders");
  if (folders == nullptr || folders->empty()) {
    return;
  }
  const llvm::json::Object* folder = (*folders)[0].getAsObject();
  if (folder == nullptr) {
    return;
  }
  if (const std::optional<llvm::StringRef> uri = folder->getString("uri")) {
    workspaceRoot_ = uriToPath(uri->str());
  }
}

void LanguageSession::fillImportCompletions(llvm::json::Array& items,
                                            const ImportCompletionQuery& query,
                                            const std::string& uri) {
  const std::filesystem::path file = uriToPath(uri);
  const LanguageContext context = resolveLanguageContext(file);
  const std::filesystem::path stdlib = !stdlibDir_.empty() ? stdlibDir_ : context.stdlib;
  std::vector<std::filesystem::path> dirs = importSearchDirs(file.parent_path(), stdlib);
  appendWorkspaceImportDirs(dirs, workspaceRoot_);
  appendLanguageContextDirs(dirs, context);
  if (query.kind == ImportCompletionKind::FromImportKeyword) {
    addCompletion(items, "import", kCompletionKeyword, "keyword", query.prefix, {}, "0import");
    return;
  }
  if (query.kind == ImportCompletionKind::ImportAsKeyword) {
    addCompletion(items, "as", kCompletionKeyword, "keyword", query.prefix, {}, "0as");
    return;
  }
  if (query.kind == ImportCompletionKind::ModulePath) {
    addImportCompletionItems(
        items, importModuleCompletions(dirs, stdlib, query.typedPath, file), query.prefix);
    return;
  }
  const std::filesystem::path moduleFile =
      resolveImportFile(dirs, splitImportPath(query.modulePath), file, nullptr);
  addImportCompletionItems(items, importExportCompletions(moduleFile, query.prefix), query.prefix);
}

void LanguageSession::handleInitialize(const llvm::json::Value* id, const llvm::json::Object* params) {
  if (params != nullptr) {
    captureWorkspaceRoot(*params);
    applyInitializeOptions(*params);
  }
  applyLanguageContext(workspaceRoot_);
  llvm::json::Array tokenTypes;
  for (const char* name : kSemanticTokenTypeNames) {
    tokenTypes.push_back(name);
  }
  llvm::json::Array tokenModifiers;
  for (const char* name : kSemanticTokenModifierNames) {
    tokenModifiers.push_back(name);
  }
  llvm::json::Object capabilities{
      {"textDocumentSync",
       llvm::json::Object{{"openClose", true}, {"change", kLspSyncIncremental}}},
      {"hoverProvider", true},
      {"definitionProvider", true},
      {"typeDefinitionProvider", true},
      {"implementationProvider", true},
      {"documentHighlightProvider", true},
      {"documentSymbolProvider", true},
      {"workspaceSymbolProvider", true},
      {"renameProvider", llvm::json::Object{{"prepareProvider", true}}},
      {"documentFormattingProvider", true},
      {"documentRangeFormattingProvider", true},
      {"codeActionProvider", true},
      {"foldingRangeProvider", true},
      {"selectionRangeProvider", true},
      {"semanticTokensProvider",
       llvm::json::Object{
           {"legend",
            llvm::json::Object{{"tokenTypes", std::move(tokenTypes)},
                               {"tokenModifiers", std::move(tokenModifiers)}}},
           {"full", true},
           {"range", true}}},
      {"callHierarchyProvider", true},
      {"signatureHelpProvider",
       llvm::json::Object{{"triggerCharacters", llvm::json::Array{"(", ",", "!"}},
                          {"retriggerCharacters", llvm::json::Array{",", " "}}}},
      {"inlayHintProvider", true},
      {"referencesProvider", true},
      {"codeLensProvider", llvm::json::Object{{"resolveProvider", false}}},
      {"completionProvider",
       llvm::json::Object{{"triggerCharacters", llvm::json::Array{".", "\"", "@", "!"}},
                          {"resolveProvider", true}}},
      {"workspace",
       llvm::json::Object{
           {"workspaceFolders",
            llvm::json::Object{{"supported", true}, {"changeNotifications", true}}},
           {"didChangeWatchedFiles", llvm::json::Object{{"dynamicRegistration", false}}},
           {"didChangeConfiguration", llvm::json::Object{{"dynamicRegistration", false}}},
       }},
  };
  writeResult(id, llvm::json::Object{
                      {"capabilities", std::move(capabilities)},
                      {"serverInfo", llvm::json::Object{{"name", "sere"},
                                                        {"version", SERE_VERSION_STRING}}},
                  });
}

void LanguageSession::handleDidOpen(const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  if (document == nullptr) {
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  const std::optional<llvm::StringRef> text = document->getString("text");
  if (!uri.has_value() || !text.has_value()) {
    return;
  }
  documents_[uri->str()] = text->str();
  analyzeDocument(uri->str());
  analyzeDependents(uri->str());
}

void LanguageSession::handleDidChange(const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  const llvm::json::Array* changes = params.getArray("contentChanges");
  if (document == nullptr || changes == nullptr || changes->empty()) {
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    return;
  }
  std::string& text = documents_[uri->str()];
  for (const llvm::json::Value& changeValue : *changes) {
    const llvm::json::Object* change = changeValue.getAsObject();
    if (change != nullptr) {
      applyContentChange(text, *change);
    }
  }
  analyzeDocument(uri->str());
  analyzeDependents(uri->str());
}

void LanguageSession::handleDidClose(const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  if (document == nullptr) {
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    return;
  }
  const std::filesystem::path closed = uriToPath(uri->str());
  const bool refreshDependents = isContextAffecting(closed);
  documents_.erase(uri->str());
  frontends_.erase(uri->str());
  overlay_.remove(closed);
  writeMessage(llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", "textDocument/publishDiagnostics"},
      {"params", llvm::json::Object{{"uri", uri->str()}, {"diagnostics", llvm::json::Array{}}}},
  });
  if (refreshDependents) {
    applyLanguageContext(workspaceRoot_);
    analyzeOpenDocuments();
  }
}

void LanguageSession::handleDidChangeWatchedFiles(const llvm::json::Object& params) {
  const llvm::json::Array* changes = params.getArray("changes");
  if (changes == nullptr || changes->empty()) {
    return;
  }
  bool refresh = false;
  for (const llvm::json::Value& changeValue : *changes) {
    const llvm::json::Object* change = changeValue.getAsObject();
    if (change == nullptr) {
      continue;
    }
    const std::optional<llvm::StringRef> uri = change->getString("uri");
    if (!uri.has_value()) {
      continue;
    }
    const std::filesystem::path path = uriToPath(uri->str());
    if (isContextAffecting(path) || documents_.contains(uri->str())) {
      refresh = true;
      continue;
    }
    for (const auto& [openUri, _] : documents_) {
      if (documentImports(openUri, path)) {
        refresh = true;
        break;
      }
    }
  }
  if (!refresh) {
    return;
  }
  applyLanguageContext(!workspaceRoot_.empty() ? workspaceRoot_ : std::filesystem::current_path());
  analyzeOpenDocuments();
}

void LanguageSession::handleDidChangeConfiguration(const llvm::json::Object& params) {
  applyClientConfiguration(params.getObject("settings"));
  applyLanguageContext(!workspaceRoot_.empty() ? workspaceRoot_ : std::filesystem::current_path());
  analyzeOpenDocuments();
}

void LanguageSession::handleDidChangeWorkspaceFolders(const llvm::json::Object& params) {
  const llvm::json::Object* event = params.getObject("event");
  const llvm::json::Array* added = event != nullptr ? event->getArray("added") : nullptr;
  if (added != nullptr && !added->empty()) {
    const llvm::json::Object* folder = (*added)[0].getAsObject();
    if (folder != nullptr) {
      if (const std::optional<llvm::StringRef> uri = folder->getString("uri")) {
        workspaceRoot_ = uriToPath(uri->str());
      }
    }
  }
  applyLanguageContext(workspaceRoot_);
  analyzeOpenDocuments();
}

std::optional<std::pair<std::string, std::uint32_t>>
LanguageSession::documentOffset(const llvm::json::Object& params) const {
  const llvm::json::Object* document = params.getObject("textDocument");
  const llvm::json::Object* position = params.getObject("position");
  if (document == nullptr || position == nullptr) {
    return std::nullopt;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  const std::optional<int64_t> line = position->getInteger("line");
  const std::optional<int64_t> character = position->getInteger("character");
  if (!uri.has_value() || !line.has_value() || !character.has_value()) {
    return std::nullopt;
  }
  const auto found = documents_.find(uri->str());
  if (found == documents_.end()) {
    return std::nullopt;
  }
  SourceManager source(uriToPath(uri->str()), found->second);
  const std::uint32_t offset =
      source.offsetAt(static_cast<std::uint32_t>(*line + 1),
                      static_cast<std::uint32_t>(*character + 1));
  return std::make_pair(uri->str(), offset);
}

void LanguageSession::handleHover(const llvm::json::Value* id, const llvm::json::Object& params) {
  const auto located = documentOffset(params);
  if (!located.has_value()) {
    writeNullResult(id);
    return;
  }
  Frontend* frontend = analyzeCached(located->first);
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeNullResult(id);
    return;
  }
  const Node* node = findNodeAt(*frontend->module(), located->second);
  std::string contents;
  SourceRange hoverRange{{}, {}};
  const MacroUse* namedUse = findMacroNameAt(frontend->macroUses(), located->second);
  if (namedUse != nullptr) {
    const SemanticSymbol* symbol = findNamedSymbol(frontend->checker(), namedUse->name, "macro");
    contents = symbol != nullptr && !symbol->typeDisplay.empty() ? symbol->typeDisplay
                                                                  : "macro " + namedUse->name;
    hoverRange = namedUse->nameRange;
  } else if (node != nullptr) {
    contents = hoverText(*node);
    hoverRange = node->range();
    if (node->kind() == NodeKind::MacroDef) {
      const auto& macro = static_cast<const MacroDef&>(*node);
      contents = formatMacro(macro);
      const SourceRange nameRange = macroNameRange(macro);
      if (rangeContains(nameRange, located->second)) {
        hoverRange = nameRange;
      }
    }
  }
  if (contents.empty()) {
    const MacroUse* use = findMacroUseAt(frontend->macroUses(), located->second);
    if (use != nullptr) {
      const SemanticSymbol* symbol = findNamedSymbol(frontend->checker(), use->name, "macro");
      contents = symbol != nullptr && !symbol->typeDisplay.empty() ? symbol->typeDisplay
                                                                    : "macro " + use->name;
      hoverRange = use->nameRange;
    }
  }
  if (contents.empty()) {
    writeNullResult(id);
    return;
  }
  llvm::json::Object result{
      {"contents", llvm::json::Object{{"kind", "markdown"},
                                      {"value", "```sere\n" + contents + "\n```"}}},
  };
  if (hoverRange.start.line != 0) {
    result["range"] = lspRange(hoverRange);
  }
  writeResult(id, std::move(result));
}

void LanguageSession::handleCompletion(const llvm::json::Value* id,
                                       const llvm::json::Object& params) {
  const auto located = documentOffset(params);
  llvm::json::Array items;
  if (!located.has_value()) {
    writeResult(id, std::move(items));
    return;
  }
  const std::string& text = documents_[located->first];
  const ImportCompletionQuery importQuery = detectImportCompletion(text, located->second);
  if (importQuery.kind != ImportCompletionKind::None) {
    fillImportCompletions(items, importQuery, located->first);
    writeResult(id, std::move(items));
    return;
  }
  const std::string prefix = identifierPrefix(text, located->second);
  Frontend* frontend = analyzeCached(located->first);
  if (hasDotBefore(text, located->second, prefix) && frontend != nullptr &&
      frontend->module() != nullptr) {
    const std::uint32_t objectOffset =
        located->second > prefix.size() + 1
            ? located->second - static_cast<std::uint32_t>(prefix.size()) - 1
            : located->second;
    const Node* node = findNodeAt(*frontend->module(), objectOffset);
    const Type* record = node == nullptr ? nullptr : recordTypeOf(*node);
    if (record != nullptr && (record->isRecord() || record->isModule())) {
      for (const RecordField& field : record->fields()) {
        if (!field.isPublic) {
          continue;
        }
        const bool isFn = field.type != nullptr && field.type->kind() == TypeKind::Function;
        addCompletion(items, field.name, isFn ? kCompletionFunction : kCompletionField,
                      field.type == nullptr ? "" : field.type->display(), prefix);
      }
      for (const RecordMethod& method : record->methods()) {
        if (!method.isPublic) {
          continue;
        }
        addCompletion(items, method.name, kCompletionMethod, formatMethod(method), prefix);
      }
      writeResult(id, std::move(items));
      return;
    }
    if (record != nullptr && record->isList()) {
      addCompletion(items, "append", kCompletionMethod, "append(value)", prefix);
      writeResult(id, std::move(items));
      return;
    }
    if (frontend->checker() != nullptr) {
      const std::string objectName = identifierPrefix(text, objectOffset);
      for (const SemanticSymbol& symbol : frontend->checker()->symbols()) {
        if (symbol.name != objectName || symbol.typeDisplay.empty()) {
          continue;
        }
        const Type* inferred =
            frontend->types() == nullptr ? nullptr : frontend->types()->record(symbol.typeDisplay);
        if (inferred != nullptr && inferred->isRecord()) {
          for (const RecordField& field : inferred->fields()) {
            if (field.isPublic) {
              addCompletion(items, field.name, kCompletionField,
                            field.type == nullptr ? "" : field.type->display(), prefix);
            }
          }
          for (const RecordMethod& method : inferred->methods()) {
            if (!method.isPublic) {
              continue;
            }
            addCompletion(items, method.name, kCompletionMethod, formatMethod(method), prefix);
          }
          writeResult(id, std::move(items));
          return;
        }
      }
    }
    writeResult(id, std::move(items));
    return;
  }
  if (frontend != nullptr && frontend->module() != nullptr) {
    const CallExpr* call = findCallAt(*frontend->module(), located->second);
    if (call != nullptr) {
      for (const std::string& name : call->paramNames()) {
        addCompletion(items, name, kCompletionVariable, "parameter", prefix);
      }
    }
  }
  addKeywordCompletions(items, prefix);
  addTypeCompletions(items, prefix);
  if (frontend != nullptr && frontend->checker() != nullptr) {
    for (const SemanticSymbol& symbol : frontend->checker()->symbols()) {
      if (symbol.kind == "field" || symbol.kind == "method" || symbol.kind == "enumMember") {
        continue;
      }
      const int kind = symbol.kind == "function" ? kCompletionFunction
                       : symbol.kind == "class"  ? kCompletionClass
                       : symbol.kind == "struct" ? kCompletionStruct
                       : symbol.kind == "enum"   ? kCompletionEnum
                       : symbol.kind == "module" ? kCompletionClass
                       : symbol.kind == "type"   ? kCompletionType
                       : symbol.kind == "macro"  ? kCompletionMacro
                                                 : kCompletionVariable;
      const std::string detail =
          symbol.kind == "macro"
              ? (symbol.typeDisplay.empty() ? "macro" : symbol.typeDisplay)
              : symbol.typeDisplay;
      if (symbol.kind == "macro" && !symbol.snippet.empty()) {
        addCompletion(items, symbol.name, kind, detail, prefix, symbol.snippet);
      } else {
        addCompletion(items, symbol.name, kind, detail, prefix);
      }
    }
  }
  writeResult(id, std::move(items));
}

void LanguageSession::handleDefinition(const llvm::json::Value* id,
                                       const llvm::json::Object& params) {
  const auto located = documentOffset(params);
  if (!located.has_value()) {
    writeNullResult(id);
    return;
  }
  Frontend* frontend = analyzeCached(located->first);
  if (frontend == nullptr || frontend->module() == nullptr || frontend->checker() == nullptr) {
    writeNullResult(id);
    return;
  }
  const Node* node = findNodeAt(*frontend->module(), located->second);
  std::string name;
  std::string container;
  bool preferMacro = false;
  if (node != nullptr && node->kind() == NodeKind::MacroDef) {
    name = static_cast<const MacroDef*>(node)->name();
    preferMacro = true;
  }
  if (name.empty()) {
    const MacroUse* use = findMacroNameAt(frontend->macroUses(), located->second);
    if (use != nullptr) {
      name = use->name;
      preferMacro = true;
    }
  }
  if (name.empty() && node != nullptr && node->kind() == NodeKind::NameExpr) {
    name = static_cast<const NameExpr*>(node)->name();
  } else if (name.empty() && node != nullptr && node->kind() == NodeKind::MemberExpr) {
    const auto& member = static_cast<const MemberExpr&>(*node);
    name = member.field();
    if (member.object().resolvedType() != nullptr) {
      container = member.object().resolvedType()->name();
    }
  } else if (name.empty() && node != nullptr && node->kind() == NodeKind::TypeExpr) {
    name = static_cast<const TypeExpr*>(node)->name();
  }
  if (name.empty()) {
    const MacroUse* use = findMacroUseAt(frontend->macroUses(), located->second);
    if (use != nullptr) {
      name = use->name;
      preferMacro = true;
    }
  }
  if (name.empty()) {
    writeNullResult(id);
    return;
  }
  const SemanticSymbol* match = nullptr;
  for (const SemanticSymbol& symbol : frontend->checker()->symbols()) {
    if (!symbol.navigable || symbol.name != name) {
      continue;
    }
    if (preferMacro && symbol.kind != "macro") {
      continue;
    }
    if (!container.empty() && symbol.container != container) {
      continue;
    }
    match = &symbol;
  }
  if (match == nullptr) {
    writeNullResult(id);
    return;
  }
  writeResult(id, llvm::json::Array{llvm::json::Object{
                     {"uri", located->first},
                     {"range", lspRange(match->range)},
                 }});
}

void LanguageSession::handleDocumentSymbol(const llvm::json::Value* id,
                                           const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  llvm::json::Array symbols;
  if (document == nullptr) {
    writeResult(id, std::move(symbols));
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    writeResult(id, std::move(symbols));
    return;
  }
  Frontend* frontend = analyzeCached(uri->str());
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeResult(id, std::move(symbols));
    return;
  }
  for (const std::unique_ptr<Stmt>& statement : frontend->module()->statements()) {
    if (statement->fromPrelude()) {
      continue;
    }
    if (statement->kind() == NodeKind::FunctionDef) {
      const auto& function = static_cast<const FunctionDef&>(*statement);
      symbols.push_back(llvm::json::Object{
          {"name", function.name()},
          {"kind", kSymbolFunction},
          {"range", lspRange(function.range())},
          {"selectionRange", lspRange(function.range())},
      });
    } else if (statement->kind() == NodeKind::ClassDef) {
      const auto& classDef = static_cast<const ClassDef&>(*statement);
      llvm::json::Array children;
      for (const FieldDecl& field : classDef.fields()) {
        children.push_back(llvm::json::Object{
            {"name", field.name},
            {"kind", kSymbolField},
            {"range", lspRange(field.range)},
            {"selectionRange", lspRange(field.range)},
        });
      }
      for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
        children.push_back(llvm::json::Object{
            {"name", method->name()},
            {"kind", kSymbolMethod},
            {"range", lspRange(method->range())},
            {"selectionRange", lspRange(method->range())},
        });
      }
      symbols.push_back(llvm::json::Object{
          {"name", classDef.name()},
          {"kind", classDef.isStruct() ? kSymbolStruct : kSymbolClass},
          {"range", lspRange(classDef.range())},
          {"selectionRange", lspRange(classDef.range())},
          {"children", std::move(children)},
      });
    } else if (statement->kind() == NodeKind::EnumDef) {
      const auto& enumDef = static_cast<const EnumDef&>(*statement);
      llvm::json::Array children;
      for (const EnumVariant& variant : enumDef.variants()) {
        children.push_back(llvm::json::Object{
            {"name", variant.name},
            {"kind", kSymbolEnumMember},
            {"range", lspRange(variant.range)},
            {"selectionRange", lspRange(variant.range)},
        });
      }
      symbols.push_back(llvm::json::Object{
          {"name", enumDef.name()},
          {"kind", kSymbolEnum},
          {"range", lspRange(enumDef.range())},
          {"selectionRange", lspRange(enumDef.range())},
          {"children", std::move(children)},
      });
    } else if (statement->kind() == NodeKind::TypeAlias) {
      const auto& alias = static_cast<const TypeAlias&>(*statement);
      symbols.push_back(llvm::json::Object{
          {"name", alias.name()},
          {"kind", kSymbolType},
          {"range", lspRange(alias.range())},
          {"selectionRange", lspRange(alias.range())},
      });
    } else if (statement->kind() == NodeKind::MacroDef) {
      const auto& macro = static_cast<const MacroDef&>(*statement);
      symbols.push_back(llvm::json::Object{
          {"name", macro.name() + "!"},
          {"detail", "macro"},
          {"kind", kSymbolFunction},
          {"range", lspRange(macro.range())},
          {"selectionRange", lspRange(macroNameRange(macro))},
      });
    }
  }
  writeResult(id, std::move(symbols));
}

void LanguageSession::handleSignatureHelp(const llvm::json::Value* id,
                                          const llvm::json::Object& params) {
  const auto located = documentOffset(params);
  if (!located.has_value()) {
    writeNullResult(id);
    return;
  }
  Frontend* frontend = analyzeCached(located->first);
  if (frontend == nullptr) {
    writeNullResult(id);
    return;
  }
  const std::string& text = documents_[located->first];
  const CallSite site = scanCallSite(text, located->second);
  if (site.callee.empty()) {
    writeNullResult(id);
    return;
  }
  std::vector<std::string> names;
  std::vector<std::string> types;
  std::string returnType;
  bool macro = false;
  const CallExpr* call =
      frontend->module() == nullptr ? nullptr : findCallAt(*frontend->module(), located->second);
  if (call != nullptr && !call->paramNames().empty()) {
    names = call->paramNames();
    const Type* fn = call->callee().resolvedType();
    std::size_t typeSkip = call->isMethod() ? 1 : 0;
    if (call->isConstructor() && call->resolvedType() != nullptr) {
      const int initIndex = call->resolvedType()->methodIndex("__init__");
      if (initIndex >= 0) {
        fn = call->resolvedType()->methods()[static_cast<std::size_t>(initIndex)].type;
        typeSkip = 1;
      }
    }
    if (fn != nullptr) {
      for (std::size_t index = typeSkip; index < fn->paramTypes().size(); ++index) {
        types.push_back(fn->paramTypes()[index] == nullptr ? "?"
                                                           : fn->paramTypes()[index]->display());
      }
      if (fn->returnType() != nullptr && !call->isConstructor()) {
        returnType = fn->returnType()->display();
      }
    }
    if (call->resolvedType() != nullptr && !call->isConstructor() && returnType.empty()) {
      returnType = call->resolvedType()->display();
    }
  } else if (frontend->checker() != nullptr) {
    const SemanticSymbol* match =
        findCallable(*frontend->checker(), site.callee, site.method, {});
    if (match != nullptr &&
        (match->kind == "class" || match->kind == "struct" || match->kind == "enum")) {
      const SemanticSymbol* init =
          findCallable(*frontend->checker(), "__init__", true, match->name);
      match = init != nullptr ? init : match;
    }
    if (match != nullptr) {
      macro = match->kind == "macro";
      collectCallableParams(*match, names, types, returnType);
    }
  }
  if (names.empty() && types.empty() && returnType.empty()) {
    writeNullResult(id);
    return;
  }
  std::size_t active = site.active;
  if (call != nullptr) {
    active = call->arguments().size();
    for (std::size_t index = 0; index < call->arguments().size(); ++index) {
      if (located->second <= call->arguments()[index]->range().end.offset) {
        active = index;
        break;
      }
    }
  }
  if (!names.empty() && active >= names.size()) {
    active = names.size() - 1;
  }
  writeResult(id, llvm::json::Object{
                      {"signatures", llvm::json::Array{makeSignature(site.callee, names, types,
                                                                     returnType, macro)}},
                      {"activeSignature", 0},
                      {"activeParameter", static_cast<int64_t>(active)},
                  });
}

void LanguageSession::handleInlayHint(const llvm::json::Value* id,
                                      const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  llvm::json::Array hints;
  if (document == nullptr) {
    writeResult(id, std::move(hints));
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    writeResult(id, std::move(hints));
    return;
  }
  Frontend* frontend = analyzeCached(uri->str());
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeResult(id, std::move(hints));
    return;
  }
  std::vector<const CallExpr*> calls;
  collectCalls(*frontend->module(), calls);
  for (const CallExpr* call : calls) {
    const std::vector<std::string>& names = call->paramNames();
    for (std::size_t index = 0; index < call->arguments().size() && index < names.size();
         ++index) {
      if (names[index].empty()) {
        continue;
      }
      const SourceRange range = call->arguments()[index]->range();
      hints.push_back(llvm::json::Object{
          {"position", lspPosition(range.start)},
          {"label", names[index] + ": "},
          {"kind", kInlayParameter},
          {"paddingRight", false},
      });
    }
  }
  writeResult(id, std::move(hints));
}

void LanguageSession::handleReferences(const llvm::json::Value* id,
                                       const llvm::json::Object& params) {
  const auto located = documentOffset(params);
  if (!located.has_value()) {
    writeResult(id, llvm::json::Array{});
    return;
  }
  Frontend* frontend = analyzeCached(located->first);
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeResult(id, llvm::json::Array{});
    return;
  }
  const Node* node = findNodeAt(*frontend->module(), located->second);
  std::string name;
  if (node != nullptr && node->kind() == NodeKind::NameExpr) {
    name = static_cast<const NameExpr*>(node)->name();
  } else if (node != nullptr && node->kind() == NodeKind::MemberExpr) {
    name = static_cast<const MemberExpr*>(node)->field();
  } else if (node != nullptr && node->kind() == NodeKind::FunctionDef) {
    name = static_cast<const FunctionDef*>(node)->name();
  } else if (node != nullptr && node->kind() == NodeKind::ClassDef) {
    name = static_cast<const ClassDef*>(node)->name();
  } else if (node != nullptr && node->kind() == NodeKind::EnumDef) {
    name = static_cast<const EnumDef*>(node)->name();
  } else if (node != nullptr && node->kind() == NodeKind::MacroDef) {
    name = static_cast<const MacroDef*>(node)->name();
  }
  if (name.empty()) {
    const MacroUse* use = findMacroNameAt(frontend->macroUses(), located->second);
    if (use != nullptr) {
      name = use->name;
    }
  }
  if (name.empty()) {
    const std::string& text = documents_[located->first];
    name = identifierPrefix(text, located->second);
  }
  llvm::json::Array locations;
  if (!name.empty()) {
    std::vector<SourceRange> refs;
    collectNameRefs(*frontend->module(), name, refs);
    appendMacroUseRefs(frontend->macroUses(), name, refs);
    for (const SourceRange& range : refs) {
      locations.push_back(llvm::json::Object{
          {"uri", located->first},
          {"range", lspRange(range)},
      });
    }
  }
  writeResult(id, std::move(locations));
}

void LanguageSession::handleCodeLens(const llvm::json::Value* id,
                                     const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  llvm::json::Array lenses;
  if (document == nullptr) {
    writeResult(id, std::move(lenses));
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    writeResult(id, std::move(lenses));
    return;
  }
  Frontend* frontend = analyzeCached(uri->str());
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeResult(id, std::move(lenses));
    return;
  }
  auto addLens = [&](const std::string& name, SourceRange range) {
    std::vector<SourceRange> refs;
    collectNameRefs(*frontend->module(), name, refs);
    appendMacroUseRefs(frontend->macroUses(), name, refs);
    const std::size_t count = refs.size();
    std::string title = std::to_string(count) + (count == 1 ? " reference" : " references");
    llvm::json::Array locations;
    for (const SourceRange& ref : refs) {
      locations.push_back(llvm::json::Object{
          {"uri", uri->str()},
          {"range", lspRange(ref)},
      });
    }
    lenses.push_back(llvm::json::Object{
        {"range", lspRange(range)},
        {"command",
         llvm::json::Object{
             {"title", std::move(title)},
             {"command", "editor.action.showReferences"},
             {"arguments", llvm::json::Array{uri->str(), lspPosition(range.start),
                                             std::move(locations)}},
         }},
    });
  };
  for (const std::unique_ptr<Stmt>& statement : frontend->module()->statements()) {
    if (statement->fromPrelude()) {
      continue;
    }
    if (statement->kind() == NodeKind::FunctionDef) {
      const auto& function = static_cast<const FunctionDef&>(*statement);
      addLens(function.name(), function.range());
    } else if (statement->kind() == NodeKind::ClassDef) {
      const auto& classDef = static_cast<const ClassDef&>(*statement);
      addLens(classDef.name(), classDef.range());
      for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
        addLens(method->name(), method->range());
      }
    } else if (statement->kind() == NodeKind::EnumDef) {
      const auto& enumDef = static_cast<const EnumDef&>(*statement);
      addLens(enumDef.name(), enumDef.range());
    } else if (statement->kind() == NodeKind::MacroDef) {
      const auto& macro = static_cast<const MacroDef&>(*statement);
      addLens(macro.name(), macro.range());
    }
  }
  writeResult(id, std::move(lenses));
}

void LanguageSession::handlePrepareRename(const llvm::json::Value* id,
                                          const llvm::json::Object& params) {
  const auto located = documentOffset(params);
  if (!located.has_value()) {
    writeNullResult(id);
    return;
  }
  Frontend* frontend = analyzeCached(located->first);
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeNullResult(id);
    return;
  }
  const Node* node = findNodeAt(*frontend->module(), located->second);
  if (node != nullptr && node->kind() == NodeKind::MacroDef) {
    writeResult(id, lspRange(macroNameRange(static_cast<const MacroDef&>(*node))));
    return;
  }
  const MacroUse* namedUse = findMacroNameAt(frontend->macroUses(), located->second);
  if (namedUse != nullptr) {
    writeResult(id, lspRange(namedUse->nameRange));
    return;
  }
  if (node == nullptr) {
    writeNullResult(id);
    return;
  }
  if (node->kind() == NodeKind::NameExpr || node->kind() == NodeKind::MemberExpr ||
      node->kind() == NodeKind::TypeExpr) {
    writeResult(id, lspRange(node->range()));
    return;
  }
  writeNullResult(id);
}

void LanguageSession::handleRename(const llvm::json::Value* id, const llvm::json::Object& params) {
  const auto located = documentOffset(params);
  const std::optional<llvm::StringRef> newName = params.getString("newName");
  if (!located.has_value() || !newName.has_value()) {
    writeNullResult(id);
    return;
  }
  Frontend* frontend = analyzeCached(located->first);
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeNullResult(id);
    return;
  }
  const Node* node = findNodeAt(*frontend->module(), located->second);
  std::string name;
  if (node != nullptr && node->kind() == NodeKind::NameExpr) {
    name = static_cast<const NameExpr*>(node)->name();
  } else if (node != nullptr && node->kind() == NodeKind::MemberExpr) {
    name = static_cast<const MemberExpr*>(node)->field();
  } else if (node != nullptr && node->kind() == NodeKind::ClassDef) {
    name = static_cast<const ClassDef*>(node)->name();
  } else if (node != nullptr && node->kind() == NodeKind::FunctionDef) {
    name = static_cast<const FunctionDef*>(node)->name();
  } else if (node != nullptr && node->kind() == NodeKind::MacroDef) {
    name = static_cast<const MacroDef*>(node)->name();
  }
  if (name.empty()) {
    const MacroUse* use = findMacroNameAt(frontend->macroUses(), located->second);
    if (use != nullptr) {
      name = use->name;
    }
  }
  llvm::json::Array edits;
  if (!name.empty()) {
    std::vector<SourceRange> refs;
    collectNameRefs(*frontend->module(), name, refs);
    appendMacroUseRefs(frontend->macroUses(), name, refs);
    for (const SourceRange& range : refs) {
      edits.push_back(llvm::json::Object{
          {"range", lspRange(range)},
          {"newText", newName->str()},
      });
    }
  }
  writeResult(id, llvm::json::Object{
                      {"changes", llvm::json::Object{{located->first, std::move(edits)}}},
                  });
}

void LanguageSession::handleDocumentHighlight(const llvm::json::Value* id,
                                              const llvm::json::Object& params) {
  const auto located = documentOffset(params);
  llvm::json::Array highlights;
  if (!located.has_value()) {
    writeResult(id, std::move(highlights));
    return;
  }
  Frontend* frontend = analyzeCached(located->first);
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeResult(id, std::move(highlights));
    return;
  }
  const Node* node = findNodeAt(*frontend->module(), located->second);
  std::string name;
  if (node != nullptr && node->kind() == NodeKind::NameExpr) {
    name = static_cast<const NameExpr*>(node)->name();
  } else if (node != nullptr && node->kind() == NodeKind::MemberExpr) {
    name = static_cast<const MemberExpr*>(node)->field();
  } else if (node != nullptr && node->kind() == NodeKind::MacroDef) {
    name = static_cast<const MacroDef*>(node)->name();
  }
  if (name.empty()) {
    const MacroUse* use = findMacroNameAt(frontend->macroUses(), located->second);
    if (use != nullptr) {
      name = use->name;
    }
  }
  if (!name.empty()) {
    std::vector<SourceRange> refs;
    collectNameRefs(*frontend->module(), name, refs);
    appendMacroUseRefs(frontend->macroUses(), name, refs);
    for (const SourceRange& range : refs) {
      highlights.push_back(llvm::json::Object{{"range", lspRange(range)}, {"kind", 1}});
    }
  }
  writeResult(id, std::move(highlights));
}

void LanguageSession::handleWorkspaceSymbol(const llvm::json::Value* id,
                                            const llvm::json::Object& params) {
  const std::optional<llvm::StringRef> query = params.getString("query");
  llvm::json::Array symbols;
  const std::string needle = query.has_value() ? query->str() : "";
  for (const auto& entry : frontends_) {
    if (entry.second == nullptr || entry.second->checker() == nullptr) {
      continue;
    }
    for (const SemanticSymbol& symbol : entry.second->checker()->symbols()) {
      if (!needle.empty() && symbol.name.find(needle) == std::string::npos) {
        continue;
      }
      int kind = kSymbolFunction;
      if (symbol.kind == "class") {
        kind = kSymbolClass;
      } else if (symbol.kind == "struct") {
        kind = kSymbolStruct;
      } else if (symbol.kind == "enum") {
        kind = kSymbolEnum;
      } else if (symbol.kind == "enumMember") {
        kind = kSymbolEnumMember;
      } else if (symbol.kind == "field") {
        kind = kSymbolField;
      } else if (symbol.kind == "type") {
        kind = kSymbolType;
      } else if (symbol.kind == "macro") {
        kind = kSymbolFunction;
        symbols.push_back(llvm::json::Object{
            {"name", symbol.name + "!"},
            {"kind", kind},
            {"location",
             llvm::json::Object{{"uri", entry.first}, {"range", lspRange(symbol.range)}}},
            {"containerName", symbol.container},
        });
        continue;
      }
      symbols.push_back(llvm::json::Object{
          {"name", symbol.name},
          {"kind", kind},
          {"location", llvm::json::Object{{"uri", entry.first}, {"range", lspRange(symbol.range)}}},
          {"containerName", symbol.container},
      });
    }
  }
  writeResult(id, std::move(symbols));
}

void LanguageSession::handleSemanticTokens(const llvm::json::Value* id,
                                           const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  llvm::json::Array data;
  if (document == nullptr) {
    writeResult(id, llvm::json::Object{{"data", std::move(data)}});
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    writeResult(id, llvm::json::Object{{"data", std::move(data)}});
    return;
  }
  Frontend* frontend = analyzeCached(uri->str());
  if (frontend == nullptr || frontend->source() == nullptr) {
    writeResult(id, llvm::json::Object{{"data", std::move(data)}});
    return;
  }
  std::vector<SemanticToken> tokens;
  collectSemanticTokens(*frontend, tokens);
  std::vector<std::int64_t> encoded;
  encodeSemanticTokens(std::move(tokens), encoded);
  for (std::int64_t value : encoded) {
    data.push_back(value);
  }
  writeResult(id, llvm::json::Object{{"data", std::move(data)}});
}

void LanguageSession::handleFoldingRange(const llvm::json::Value* id,
                                         const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  llvm::json::Array ranges;
  if (document == nullptr) {
    writeResult(id, std::move(ranges));
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    writeResult(id, std::move(ranges));
    return;
  }
  Frontend* frontend = analyzeCached(uri->str());
  if (frontend == nullptr || frontend->module() == nullptr) {
    writeResult(id, std::move(ranges));
    return;
  }
  auto addFold = [&](SourceRange range) {
    if (range.end.line > range.start.line) {
      ranges.push_back(llvm::json::Object{
          {"startLine", static_cast<int64_t>(range.start.line == 0 ? 0 : range.start.line - 1)},
          {"endLine", static_cast<int64_t>(range.end.line == 0 ? 0 : range.end.line - 1)},
          {"kind", "region"},
      });
    }
  };
  for (const std::unique_ptr<Stmt>& statement : frontend->module()->statements()) {
    if (!statement->fromPrelude()) {
      addFold(statement->range());
    }
  }
  writeResult(id, std::move(ranges));
}

void LanguageSession::handleFormatting(const llvm::json::Value* id,
                                       const llvm::json::Object& params) {
  const llvm::json::Object* document = params.getObject("textDocument");
  llvm::json::Array edits;
  if (document == nullptr) {
    writeResult(id, std::move(edits));
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    writeResult(id, std::move(edits));
    return;
  }
  const auto found = documents_.find(uri->str());
  if (found == documents_.end()) {
    writeResult(id, std::move(edits));
    return;
  }
  std::string formatted = found->second;
  if (!formatted.empty() && formatted.back() != '\n') {
    formatted.push_back('\n');
    const SourceRange whole{{1, 1, 0}, {1, 1, static_cast<std::uint32_t>(found->second.size())}};
    edits.push_back(llvm::json::Object{
        {"range", llvm::json::Object{{"start", llvm::json::Object{{"line", 0}, {"character", 0}}},
                                     {"end", lspPosition(whole.end)}}},
        {"newText", formatted},
    });
  }
  writeResult(id, std::move(edits));
}

void LanguageSession::handleCodeAction(const llvm::json::Value* id,
                                       const llvm::json::Object& params) {
  llvm::json::Array actions;
  const llvm::json::Object* document = params.getObject("textDocument");
  const llvm::json::Object* range = params.getObject("range");
  if (document == nullptr) {
    writeResult(id, std::move(actions));
    return;
  }
  const std::optional<llvm::StringRef> uri = document->getString("uri");
  if (!uri.has_value()) {
    writeResult(id, std::move(actions));
    return;
  }
  Frontend* frontend = analyzeCached(uri->str());
  if (frontend == nullptr) {
    writeResult(id, std::move(actions));
    return;
  }
  const auto foundText = documents_.find(uri->str());
  const std::string& text = foundText == documents_.end() ? std::string{} : foundText->second;
  int startLine = 0;
  int endLine = 1 << 30;
  if (range != nullptr) {
    if (const llvm::json::Object* start = range->getObject("start")) {
      startLine = static_cast<int>(start->getInteger("line").value_or(0));
    }
    if (const llvm::json::Object* end = range->getObject("end")) {
      endLine = static_cast<int>(end->getInteger("line").value_or(startLine));
    }
  }
  for (const Diagnostic& diagnostic : frontend->diagnostics().diagnostics()) {
    const int diagLine =
        diagnostic.range.start.line == 0 ? 0 : static_cast<int>(diagnostic.range.start.line - 1);
    if (diagLine < startLine || diagLine > endLine) {
      continue;
    }
    int column = 0;
    int line = 0;
    std::size_t lineStart = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
      if (line == diagLine) {
        lineStart = index;
        break;
      }
      if (text[index] == '\n') {
        ++line;
      }
    }
    std::size_t lineEnd = lineStart;
    while (lineEnd < text.size() && text[lineEnd] != '\n' && text[lineEnd] != '\r') {
      ++lineEnd;
    }
    column = static_cast<int>(lineEnd - lineStart);
    const std::string lineText = text.substr(lineStart, static_cast<std::size_t>(column));
    if (lineText.find("# type") != std::string::npos) {
      continue;
    }
    const std::string code{diagnosticCodeName(diagnostic.code)};
    const std::string ignore = "  # type[" + code + "]: ignore";
    llvm::json::Object edit{
        {"changes",
         llvm::json::Object{
             {uri->str(),
              llvm::json::Array{llvm::json::Object{
                  {"range", llvm::json::Object{{"start", llvm::json::Object{{"line", diagLine},
                                                                          {"character", column}}},
                                              {"end", llvm::json::Object{{"line", diagLine},
                                                                        {"character", column}}}}},
                  {"newText", ignore},
              }}}}}};
    std::string title = "Ignore " + code + " on this line";
    actions.push_back(llvm::json::Object{
        {"title", title},
        {"kind", "quickfix"},
        {"edit", std::move(edit)},
    });
  }
  writeResult(id, std::move(actions));
}

}  // namespace

int runLanguageServer() {
  setStdioBinary();
  LanguageSession session;
  std::string body;
  while (readMessage(body)) {
    llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(body);
    if (!parsed) {
      llvm::consumeError(parsed.takeError());
      continue;
    }
    const llvm::json::Object* message = parsed->getAsObject();
    if (message == nullptr) {
      continue;
    }
    if (!session.handleMessage(*message)) {
      break;
    }
  }
  return 0;
}

}  // namespace sere
