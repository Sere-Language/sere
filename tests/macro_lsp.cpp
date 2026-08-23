/// @file macro_lsp.cpp
/// Language-server checks: macro symbols/uses, plus pointer hover and tokens.

#include "sere/ast/Query.h"
#include "sere/ast/Syntax.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/driver/Frontend.h"
#include "sere/lex/Lexer.h"
#include "sere/lsp/SemanticTokens.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifndef SERE_STDLIB_DIR
#define SERE_STDLIB_DIR ""
#endif

namespace {

[[nodiscard]] std::filesystem::path stdlibDir() {
  if (const char* fromEnv = std::getenv("SERE_STDLIB"); fromEnv != nullptr && fromEnv[0] != '\0') {
    return fromEnv;
  }
  return SERE_STDLIB_DIR;
}

int fail(const char* message) {
  std::cerr << "macro_lsp: " << message << '\n';
  return 1;
}

[[nodiscard]] const sere::MacroDef* findMacro(const sere::Module& module, std::string_view name) {
  for (const std::unique_ptr<sere::Stmt>& statement : module.statements()) {
    if (statement->kind() != sere::NodeKind::MacroDef) {
      continue;
    }
    const auto& def = static_cast<const sere::MacroDef&>(*statement);
    if (def.name() == name) {
      return &def;
    }
  }
  return nullptr;
}

[[nodiscard]] const sere::SemanticSymbol* findSymbol(const sere::TypeChecker& checker,
                                                     std::string_view name,
                                                     std::string_view kind) {
  for (const sere::SemanticSymbol& symbol : checker.symbols()) {
    if (symbol.name == name && symbol.kind == kind) {
      return &symbol;
    }
  }
  return nullptr;
}

int testFormatAndNameRange() {
  const std::string text =
      "macro twice(x):\n"
      "    quote:\n"
      "        ($x) + ($x)\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("macro_lsp.sere", text);
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll(), &source);
  std::unique_ptr<sere::Module> module = parser.parseModule();
  if (module == nullptr || diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return fail("failed to parse twice macro");
  }
  const sere::MacroDef* def = findMacro(*module, "twice");
  if (def == nullptr) {
    return fail("missing twice definition");
  }
  const std::string formatted = sere::formatMacro(*def);
  if (formatted.find("macro twice(x)") == std::string::npos) {
    std::cerr << formatted << '\n';
    return fail("formatMacro should describe twice(x)");
  }
  if (sere::macroSnippet(*def) != "twice!(${1:x})") {
    return fail("macroSnippet should expand twice!(x)");
  }
  const sere::SourceRange nameRange = sere::macroNameRange(*def);
  const std::string nameText =
      text.substr(nameRange.start.offset, nameRange.end.offset - nameRange.start.offset);
  if (nameText != "twice") {
    std::cerr << "name range text: '" << nameText << "'\n";
    return fail("macro name range should cover 'twice', not 'macro'");
  }

  const std::string rawText =
      "macro html:\n"
      "    syntax: raw\n"
      "    interpolate: brace\n"
      "    wrapper: Html\n";
  sere::DiagnosticEngine rawDiagnostics;
  sere::SourceManager rawSource("macro_html.sere", rawText);
  sere::Lexer rawLexer(rawSource, rawDiagnostics);
  sere::Parser rawParser(rawDiagnostics, rawLexer.tokenizeAll(), &rawSource);
  std::unique_ptr<sere::Module> rawModule = rawParser.parseModule();
  if (rawModule == nullptr || rawDiagnostics.hasErrors()) {
    rawDiagnostics.printAll(rawSource);
    return fail("failed to parse html macro");
  }
  const sere::MacroDef* html = findMacro(*rawModule, "html");
  if (html == nullptr) {
    return fail("missing html definition");
  }
  const std::string htmlText = sere::formatMacro(*html);
  if (htmlText.find("syntax: raw") == std::string::npos ||
      htmlText.find("interpolate: brace") == std::string::npos ||
      htmlText.find("wrapper: Html") == std::string::npos) {
    std::cerr << htmlText << '\n';
    return fail("formatMacro should describe raw html properties");
  }
  if (sere::macroSnippet(*html) != "html:\n    $0") {
    return fail("html snippet should be an indent body");
  }
  return 0;
}

int testFrontendSymbols() {
  const std::string text =
      "macro twice(x):\n"
      "    quote:\n"
      "        ($x) + ($x)\n"
      "\n"
      "macro vec:\n"
      "    match:\n"
      "        ($($x:expr),*) => quote:\n"
      "            [$($x),*]\n"
      "\n"
      "def main() -> i32:\n"
      "    n: i32 = 3\n"
      "    total: i32 = twice!(n)\n"
      "    xs: list[i32] = vec!(1, 2, 3)\n"
      "    dbg!(total)\n"
      "    return total + xs[0]\n";
  sere::Frontend frontend;
  if (!frontend.analyze("macro_lsp.sere", text, stdlibDir())) {
    frontend.diagnostics().printAll();
    return fail("frontend analyze failed");
  }
  if (frontend.checker() == nullptr || frontend.module() == nullptr) {
    return fail("missing checker or module");
  }
  const sere::SemanticSymbol* twice = findSymbol(*frontend.checker(), "twice", "macro");
  if (twice == nullptr || twice->typeDisplay.find("macro twice(x)") == std::string::npos) {
    return fail("twice should be a macro symbol");
  }
  if (twice->snippet != "twice!(${1:x})" || twice->paramNames.size() != 1 ||
      twice->paramNames[0] != "x") {
    return fail("twice snippet and params should be populated");
  }
  if (!twice->navigable) {
    return fail("user macros should be navigable");
  }
  const sere::SemanticSymbol* vec = findSymbol(*frontend.checker(), "vec", "macro");
  if (vec == nullptr || vec->typeDisplay.find("match:") == std::string::npos) {
    return fail("vec should be a match macro");
  }
  const sere::SemanticSymbol* dbg = findSymbol(*frontend.checker(), "dbg", "macro");
  const sere::SemanticSymbol* todo = findSymbol(*frontend.checker(), "todo", "macro");
  const sere::SemanticSymbol* unreachable = findSymbol(*frontend.checker(), "unreachable", "macro");
  if (dbg == nullptr || todo == nullptr || unreachable == nullptr) {
    return fail("prelude macros dbg, todo, and unreachable should be registered");
  }
  if (dbg->navigable || todo->navigable || unreachable->navigable) {
    return fail("prelude macros should not be navigable");
  }
  if (sere::macroSnippet(*findMacro(*frontend.module(), "dbg")) != "dbg!(${1:x})") {
    return fail("dbg snippet should be dbg!(x)");
  }
  bool foundTwice = false;
  bool foundVec = false;
  bool foundDbg = false;
  for (const sere::MacroUse& use : frontend.macroUses()) {
    foundTwice = foundTwice || use.name == "twice";
    foundVec = foundVec || use.name == "vec";
    foundDbg = foundDbg || use.name == "dbg";
  }
  if (!foundTwice || !foundVec || !foundDbg) {
    return fail("collectMacroUses should record twice, vec, and dbg");
  }
  const std::size_t twiceAt = text.find("twice!");
  if (twiceAt == std::string::npos) {
    return fail("missing twice! in source");
  }
  const sere::MacroUse* named =
      sere::findMacroNameAt(frontend.macroUses(), static_cast<std::uint32_t>(twiceAt));
  if (named == nullptr || named->name != "twice") {
    return fail("findMacroNameAt should resolve twice!");
  }
  std::vector<sere::SourceRange> refs;
  sere::collectNameRefs(*frontend.module(), "twice", refs);
  if (refs.empty()) {
    return fail("collectNameRefs should include the twice definition");
  }
  const std::string defName =
      text.substr(refs.front().start.offset, refs.front().end.offset - refs.front().start.offset);
  if (defName != "twice") {
    std::cerr << "def name text: '" << defName << "'\n";
    return fail("definition reference should be the identifier twice");
  }
  return 0;
}

[[nodiscard]] bool hasTokenAt(const std::vector<sere::SemanticToken>& tokens, const std::string& text,
                              std::size_t offset, std::uint32_t length, sere::SemanticType type) {
  std::uint32_t line = 0;
  std::uint32_t column = 0;
  for (std::size_t index = 0; index < offset && index < text.size(); ++index) {
    if (text[index] == '\n') {
      ++line;
      column = 0;
    } else {
      ++column;
    }
  }
  for (const sere::SemanticToken& token : tokens) {
    if (token.line == line && token.column == column && token.length == length &&
        token.type == static_cast<std::uint32_t>(type)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] const sere::UnaryExpr* unaryAt(const sere::Module& module, std::size_t offset) {
  const sere::Node* node = sere::findNodeAt(module, static_cast<std::uint32_t>(offset));
  if (node == nullptr || node->kind() != sere::NodeKind::UnaryExpr) {
    return nullptr;
  }
  return static_cast<const sere::UnaryExpr*>(node);
}

[[nodiscard]] bool unaryHasType(const sere::UnaryExpr* expr, sere::UnaryOp op,
                                std::string_view typeName) {
  return expr != nullptr && expr->op() == op && expr->resolvedType() != nullptr &&
         expr->resolvedType()->display() == typeName;
}

int testPointerLsp() {
  const std::string text =
      "def main() -> i32:\n"
      "    n: i32 = 10\n"
      "    p: Ptr[i32] = &n\n"
      "    *p = 20\n"
      "    owned: Unique[i32] = unique[i32](1)\n"
      "    return *p + *owned\n";
  sere::Frontend frontend;
  if (!frontend.analyze("pointer_lsp.sere", text, stdlibDir())) {
    frontend.diagnostics().printAll();
    return fail("pointer frontend analyze failed");
  }
  if (frontend.module() == nullptr || frontend.source() == nullptr) {
    return fail("missing module or source for pointer lsp");
  }
  const std::size_t addrAt = text.find("&n");
  const std::size_t derefP = text.find("*p =");
  const std::size_t derefOwned = text.find("*owned");
  if (addrAt == std::string::npos || derefP == std::string::npos || derefOwned == std::string::npos) {
    return fail("missing pointer operators in source");
  }
  if (!unaryHasType(unaryAt(*frontend.module(), addrAt), sere::UnaryOp::AddrOf, "Ptr[i32]")) {
    return fail("hover on '&' should resolve to Ptr[i32]");
  }
  if (!unaryHasType(unaryAt(*frontend.module(), derefP), sere::UnaryOp::Deref, "i32")) {
    return fail("hover on '*' should resolve to the pointee type");
  }
  if (!unaryHasType(unaryAt(*frontend.module(), derefOwned), sere::UnaryOp::Deref, "i32")) {
    return fail("hover on '*owned' should resolve Unique[T] to T");
  }
  const sere::Node* name = sere::findNodeAt(*frontend.module(), static_cast<std::uint32_t>(addrAt + 1));
  if (name == nullptr || name->kind() != sere::NodeKind::NameExpr || name->resolvedType() == nullptr ||
      name->resolvedType()->display() != "i32") {
    return fail("hover on the address-of operand should still show i32");
  }
  std::vector<sere::SemanticToken> tokens;
  sere::collectSemanticTokens(frontend, tokens);
  if (!hasTokenAt(tokens, text, addrAt, 1, sere::SemanticType::Operator) ||
      !hasTokenAt(tokens, text, derefP, 1, sere::SemanticType::Operator) ||
      !hasTokenAt(tokens, text, derefOwned, 1, sere::SemanticType::Operator)) {
    return fail("semantic tokens should classify '&' and '*' as operators");
  }
  const std::size_t ptrAt = text.find("Ptr[");
  const std::size_t uniqueAt = text.find("Unique[");
  if (ptrAt == std::string::npos || uniqueAt == std::string::npos) {
    return fail("missing Ptr/Unique type names");
  }
  if (!hasTokenAt(tokens, text, ptrAt, 3, sere::SemanticType::Type) ||
      !hasTokenAt(tokens, text, uniqueAt, 6, sere::SemanticType::Type)) {
    return fail("Ptr and Unique should be highlighted as builtin types");
  }
  return 0;
}

}  // namespace

int main() {
  if (int status = testFormatAndNameRange(); status != 0) {
    return status;
  }
  if (int status = testFrontendSymbols(); status != 0) {
    return status;
  }
  if (int status = testPointerLsp(); status != 0) {
    return status;
  }
  return 0;
}
