/// @file ImportCompletion.cpp
/// Detects import-statement cursor context and collects module/export candidates.

#include "sere/lsp/ImportCompletion.h"

#include "sere/ast/Syntax.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/driver/Library.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace sere {
namespace {

[[nodiscard]] bool isSpace(char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

[[nodiscard]] bool isIdentStart(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalpha(value) != 0 || ch == '_';
}

[[nodiscard]] bool isIdentContinue(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalnum(value) != 0 || ch == '_';
}

[[nodiscard]] std::string_view lineToCursor(std::string_view text, std::uint32_t offset) {
  const std::size_t end = offset > text.size() ? text.size() : static_cast<std::size_t>(offset);
  std::size_t start = end;
  while (start > 0 && text[start - 1] != '\n' && text[start - 1] != '\r') {
    --start;
  }
  return text.substr(start, end - start);
}

[[nodiscard]] std::string_view ltrim(std::string_view text) {
  std::size_t index = 0;
  while (index < text.size() && isSpace(text[index])) {
    ++index;
  }
  return text.substr(index);
}

[[nodiscard]] bool keywordAt(std::string_view text, std::string_view keyword) {
  if (!text.starts_with(keyword)) {
    return false;
  }
  if (text.size() == keyword.size()) {
    return true;
  }
  return isSpace(text[keyword.size()]);
}

[[nodiscard]] std::string_view afterKeyword(std::string_view text, std::string_view keyword) {
  if (text.size() < keyword.size()) {
    return {};
  }
  return ltrim(text.substr(keyword.size()));
}

[[nodiscard]] std::string takeDottedPath(std::string_view text, std::size_t& consumed) {
  consumed = 0;
  std::string out;
  bool needIdent = true;
  while (consumed < text.size()) {
    const char ch = text[consumed];
    if (needIdent) {
      if (!isIdentStart(ch) && ch != '.') {
        break;
      }
      if (ch == '.') {
        out.push_back('.');
        ++consumed;
        continue;
      }
      needIdent = false;
      out.push_back(ch);
      ++consumed;
      continue;
    }
    if (isIdentContinue(ch)) {
      out.push_back(ch);
      ++consumed;
      continue;
    }
    if (ch == '.') {
      out.push_back('.');
      ++consumed;
      needIdent = true;
      continue;
    }
    break;
  }
  return out;
}

[[nodiscard]] std::string lastSegment(std::string_view path) {
  const std::size_t dot = path.rfind('.');
  if (dot == std::string_view::npos) {
    return std::string(path);
  }
  return std::string(path.substr(dot + 1));
}

[[nodiscard]] bool isImportPrefix(std::string_view text) {
  constexpr std::string_view keyword = "import";
  return !text.empty() && keyword.starts_with(text) && isIdentStart(text.front()) &&
         std::all_of(text.begin(), text.end(), isIdentContinue);
}

[[nodiscard]] bool isAsPrefix(std::string_view text) {
  constexpr std::string_view keyword = "as";
  return !text.empty() && keyword.starts_with(text) && isIdentStart(text.front()) &&
         std::all_of(text.begin(), text.end(), isIdentContinue);
}

[[nodiscard]] ImportCompletionQuery fromNamesQuery(std::string modulePath, std::string_view rest) {
  ImportCompletionQuery query;
  query.kind = ImportCompletionKind::FromNames;
  query.modulePath = std::move(modulePath);
  const std::string_view names = ltrim(rest);
  if (names.starts_with('*')) {
    query.kind = ImportCompletionKind::None;
    return query;
  }
  const std::size_t comma = names.rfind(',');
  const std::string_view tail = comma == std::string_view::npos ? names : ltrim(names.substr(comma + 1));
  std::size_t consumed = 0;
  query.prefix = takeDottedPath(tail, consumed);
  return query;
}

[[nodiscard]] ImportCompletionQuery parseFromLine(std::string_view rest) {
  ImportCompletionQuery query;
  std::size_t consumed = 0;
  const std::string typed = takeDottedPath(rest, consumed);
  const std::string_view leftover = ltrim(rest.substr(consumed));
  if (keywordAt(leftover, "import")) {
    return fromNamesQuery(typed, afterKeyword(leftover, "import"));
  }
  if (leftover.empty() && !typed.empty() && !typed.ends_with('.') &&
      consumed < rest.size() && isSpace(rest[consumed])) {
    query.kind = ImportCompletionKind::FromImportKeyword;
    query.typedPath = typed;
    query.modulePath = typed;
    return query;
  }
  if (isImportPrefix(leftover)) {
    query.kind = ImportCompletionKind::FromImportKeyword;
    query.typedPath = typed;
    query.modulePath = typed;
    query.prefix = std::string(leftover);
    return query;
  }
  query.kind = ImportCompletionKind::ModulePath;
  query.typedPath = typed;
  query.prefix = lastSegment(typed);
  return query;
}

[[nodiscard]] ImportCompletionQuery parseImportLine(std::string_view rest) {
  ImportCompletionQuery query;
  std::size_t consumed = 0;
  const std::string typed = takeDottedPath(rest, consumed);
  const std::string_view leftover = ltrim(rest.substr(consumed));
  if (keywordAt(leftover, "as")) {
    query.kind = ImportCompletionKind::None;
    return query;
  }
  if (leftover.empty() && !typed.empty() && !typed.ends_with('.') &&
      consumed < rest.size() && isSpace(rest[consumed])) {
    query.kind = ImportCompletionKind::ImportAsKeyword;
    query.typedPath = typed;
    query.modulePath = typed;
    return query;
  }
  if (isAsPrefix(leftover)) {
    query.kind = ImportCompletionKind::ImportAsKeyword;
    query.typedPath = typed;
    query.modulePath = typed;
    query.prefix = std::string(leftover);
    return query;
  }
  query.kind = ImportCompletionKind::ModulePath;
  query.typedPath = typed;
  query.prefix = lastSegment(typed);
  return query;
}

[[nodiscard]] std::optional<std::string> readText(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  std::ostringstream stream;
  stream << input.rdbuf();
  return stream.str();
}

[[nodiscard]] std::string formatTypeExpr(const TypeExpr& type) {
  std::string text = type.name();
  if (type.args().empty()) {
    return text;
  }
  text += '[';
  for (std::size_t index = 0; index < type.args().size(); ++index) {
    if (index != 0) {
      text += ", ";
    }
    text += formatTypeExpr(*type.args()[index]);
  }
  text += ']';
  return text;
}

[[nodiscard]] ImportCompletionItem exportItem(std::string name,
                                              ImportItemKind kind,
                                              std::string detail,
                                              std::string_view prefix) {
  ImportCompletionItem item;
  item.label = std::move(name);
  item.detail = std::move(detail);
  item.kind = kind;
  item.sortText = "1" + item.label;
  if (!prefix.empty() && !item.label.starts_with(prefix)) {
    item.label.clear();
  }
  return item;
}

[[nodiscard]] ImportCompletionItem fromFunction(const FunctionDef& function, std::string_view prefix) {
  std::string detail = "def " + function.name() + "(";
  for (std::size_t index = 0; index < function.params().size(); ++index) {
    if (index != 0) {
      detail += ", ";
    }
    detail += function.params()[index].name;
  }
  detail += ") -> " + formatTypeExpr(function.returnType());
  return exportItem(function.name(), ImportItemKind::Function, std::move(detail), prefix);
}

[[nodiscard]] ImportCompletionItem fromClass(const ClassDef& cls, std::string_view prefix) {
  const ImportItemKind kind = cls.isStruct() ? ImportItemKind::Struct : ImportItemKind::Class;
  const char* noun = cls.isStruct() ? "struct" : "class";
  return exportItem(cls.name(), kind, noun, prefix);
}

[[nodiscard]] ImportCompletionItem fromStmt(const Stmt& statement, std::string_view prefix) {
  switch (statement.kind()) {
  case NodeKind::FunctionDef:
    return fromFunction(static_cast<const FunctionDef&>(statement), prefix);
  case NodeKind::ClassDef:
    return fromClass(static_cast<const ClassDef&>(statement), prefix);
  case NodeKind::EnumDef:
    return exportItem(static_cast<const EnumDef&>(statement).name(), ImportItemKind::Enum, "enum",
                      prefix);
  case NodeKind::TypeAlias:
    return exportItem(static_cast<const TypeAlias&>(statement).name(), ImportItemKind::Type, "type",
                      prefix);
  case NodeKind::MacroDef:
    return exportItem(static_cast<const MacroDef&>(statement).name(), ImportItemKind::Macro, "macro",
                      prefix);
  case NodeKind::VarDecl:
    return exportItem(static_cast<const VarDecl&>(statement).name(), ImportItemKind::Variable,
                      "variable", prefix);
  default:
    return {};
  }
}

}  // namespace

ImportCompletionQuery detectImportCompletion(std::string_view text, std::uint32_t offset) {
  const std::string_view line = ltrim(lineToCursor(text, offset));
  if (keywordAt(line, "import")) {
    return parseImportLine(afterKeyword(line, "import"));
  }
  if (keywordAt(line, "from")) {
    return parseFromLine(afterKeyword(line, "from"));
  }
  return {};
}

std::vector<ImportCompletionItem> importModuleCompletions(
    const std::vector<std::filesystem::path>& searchDirs,
    const std::filesystem::path& stdlibDir,
    std::string_view typedPath,
    const std::filesystem::path& skipFile) {
  std::vector<ImportCompletionItem> items;
  const std::vector<ImportModuleEntry> modules =
      listImportModules(searchDirs, stdlibDir, typedPath, skipFile);
  items.reserve(modules.size());
  for (const ImportModuleEntry& module : modules) {
    ImportCompletionItem item;
    item.label = module.dottedName;
    item.insertText = module.lastSegment;
    item.detail = module.detail;
    item.kind = module.isPackage ? ImportItemKind::Package : ImportItemKind::Module;
    item.sortText = (module.detail.starts_with("stdlib") ? "0" : "1") + module.dottedName;
    items.push_back(std::move(item));
  }
  return items;
}

std::vector<ImportCompletionItem> importExportCompletions(
    const std::filesystem::path& moduleFile,
    std::string_view prefix,
    const std::optional<std::string>& overlayText) {
  std::vector<ImportCompletionItem> items;
  ImportCompletionItem star;
  star.label = "*";
  star.detail = "import all public names";
  star.kind = ImportItemKind::Keyword;
  star.sortText = "0*";
  if (prefix.empty() || star.label.starts_with(prefix)) {
    items.push_back(std::move(star));
  }
  std::filesystem::path sourceFile = moduleFile;
  if (isSereLibraryFile(sourceFile)) {
    std::string extractError;
    sourceFile = ensureLibraryExtracted(sourceFile, extractError);
    if (sourceFile.empty()) {
      return items;
    }
  }
  const std::optional<std::string> text =
      overlayText.has_value() ? overlayText : readText(sourceFile);
  if (!text.has_value()) {
    return items;
  }
  DiagnosticEngine diagnostics;
  SourceManager source(moduleFile.string(), *text);
  diagnostics.setSource(&source);
  Lexer lexer(source, diagnostics);
  Parser parser(diagnostics, lexer.tokenizeAll(), &source);
  const std::unique_ptr<Module> module = parser.parseModule();
  if (module == nullptr) {
    return items;
  }
  for (const std::unique_ptr<Stmt>& statement : module->statements()) {
    if (statement == nullptr || statement->fromPrelude() || statement->isPrivate()) {
      continue;
    }
    ImportCompletionItem item = fromStmt(*statement, prefix);
    if (!item.label.empty()) {
      items.push_back(std::move(item));
    }
  }
  return items;
}

}  // namespace sere
