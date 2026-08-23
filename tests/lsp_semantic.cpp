/// @file lsp_semantic.cpp
/// Semantic tokens for keywords, modifiers, decorators, enums, and members.

#include "sere/driver/Frontend.h"
#include "sere/lsp/SemanticTokens.h"
#include "sere/ast/Query.h"
#include "sere/sema/TypeChecker.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
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
  std::cerr << "lsp_semantic: " << message << '\n';
  return 1;
}

[[nodiscard]] bool hasType(const std::vector<sere::SemanticToken>& tokens,
                           sere::SemanticType type) {
  const auto wanted = static_cast<std::uint32_t>(type);
  for (const sere::SemanticToken& token : tokens) {
    if (token.type == wanted && token.length > 0) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool hasTokenAt(const std::vector<sere::SemanticToken>& tokens,
                              std::uint32_t line, std::uint32_t column,
                              std::uint32_t length, sere::SemanticType type) {
  const auto wanted = static_cast<std::uint32_t>(type);
  for (const sere::SemanticToken& token : tokens) {
    if (token.line == line && token.column == column && token.length == length &&
        token.type == wanted) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  const std::string text =
      "enum Color:\n"
      "    RED\n"
      "struct Point:\n"
      "    x: i32\n"
      "@public\n"
      "class Box:\n"
      "    static total: i32 = 0\n"
      "macro twice(x):\n"
      "    quote:\n"
      "        ($x) + ($x)\n"
      "def main() -> i32:\n"
      "    c: Color = Color.RED\n"
      "    return 0\n";
  sere::Frontend frontend;
  (void)frontend.analyze("lsp_semantic.sere", text, stdlibDir());
  if (frontend.checker() == nullptr) {
    frontend.diagnostics().printAll(*frontend.source());
    return fail("type checker missing");
  }
  bool sawMember = false;
  for (const sere::SemanticSymbol& symbol : frontend.checker()->symbols()) {
    if (symbol.kind == "enumMember") {
      sawMember = true;
    }
  }
  if (!sawMember) {
    return fail("checker has no enumMember symbols");
  }
  std::vector<sere::SemanticToken> tokens;
  collectSemanticTokens(frontend, tokens);
  if (!hasType(tokens, sere::SemanticType::Keyword)) {
    return fail("expected keyword tokens");
  }
  if (!hasType(tokens, sere::SemanticType::Modifier)) {
    return fail("expected modifier tokens for static/const");
  }
  if (!hasType(tokens, sere::SemanticType::Decorator)) {
    return fail("expected decorator tokens");
  }
  if (!hasType(tokens, sere::SemanticType::Enum)) {
    return fail("expected enum name tokens");
  }
  if (!hasType(tokens, sere::SemanticType::EnumMember)) {
    return fail("expected enum member tokens");
  }
  if (!hasTokenAt(tokens, 2, 0, 6, sere::SemanticType::Keyword)) {
    return fail("expected struct keyword token");
  }
  if (!hasTokenAt(tokens, 2, 7, 5, sere::SemanticType::Struct)) {
    return fail("expected struct name token");
  }
  if (!hasTokenAt(tokens, 5, 0, 5, sere::SemanticType::Keyword)) {
    return fail("expected class keyword token");
  }
  if (!hasTokenAt(tokens, 5, 6, 3, sere::SemanticType::Class)) {
    return fail("expected class name token");
  }
  if (!hasTokenAt(tokens, 7, 0, 5, sere::SemanticType::Keyword)) {
    return fail("expected macro keyword token");
  }
  if (!hasTokenAt(tokens, 7, 6, 5, sere::SemanticType::Macro)) {
    return fail("expected macro name token");
  }

  const std::string matchText =
      "enum Color:\n"
      "    RED\n"
      "macro vec:\n"
      "    match:\n"
      "        ($($x:expr),*) => quote:\n"
      "            [$($x),*]\n"
      "def main() -> i32:\n"
      "    c: Color = Color.RED\n"
      "    match c:\n"
      "        case Color.RED:\n"
      "            return 1\n"
      "        case _:\n"
      "            return 0\n";
  sere::Frontend matchFrontend;
  if (!matchFrontend.analyze("lsp_match.sere", matchText, stdlibDir())) {
    matchFrontend.diagnostics().printAll();
    return fail("match sample failed to analyze");
  }
  std::vector<sere::SemanticToken> matchTokens;
  collectSemanticTokens(matchFrontend, matchTokens);
  if (!hasTokenAt(matchTokens, 3, 4, 5, sere::SemanticType::Keyword)) {
    return fail("expected match keyword in macro body");
  }
  if (!hasTokenAt(matchTokens, 8, 4, 5, sere::SemanticType::Keyword)) {
    return fail("expected match keyword in statement");
  }
  if (!hasTokenAt(matchTokens, 9, 8, 4, sere::SemanticType::Keyword)) {
    return fail("expected case keyword");
  }
  if (!hasType(matchTokens, sere::SemanticType::Operator)) {
    return fail("expected => operator token");
  }
  if (!hasTokenAt(matchTokens, 11, 13, 1, sere::SemanticType::Variable)) {
    return fail("expected wildcard _ token");
  }
  if (!hasType(matchTokens, sere::SemanticType::TypeParameter)) {
    return fail("expected :expr fragment specifier token");
  }

  const std::string incomplete =
      "def scale(value: i32, factor: i32) -> i32:\n"
      "    return value\n"
      "def main() -> void:\n"
      "    scale(1,\n";
  sere::Frontend signatureFrontend;
  (void)signatureFrontend.analyze("lsp_signature.sere", incomplete, stdlibDir());
  if (signatureFrontend.checker() == nullptr) {
    signatureFrontend.diagnostics().printAll();
    return fail("incomplete call should still typecheck for signature help");
  }
  bool foundScale = false;
  for (const sere::SemanticSymbol& symbol : signatureFrontend.checker()->symbols()) {
    if (symbol.name != "scale" || symbol.kind != "function") {
      continue;
    }
    if (symbol.paramNames.size() != 2 || symbol.paramNames[0] != "value" ||
        symbol.paramNames[1] != "factor") {
      return fail("scale signature should expose parameter names in order");
    }
    foundScale = true;
    break;
  }
  if (!foundScale) {
    return fail("scale should be in the symbol table during an incomplete call");
  }
  if (signatureFrontend.module() == nullptr) {
    return fail("incomplete call should still produce a module");
  }
  std::vector<const sere::CallExpr*> calls;
  sere::collectCalls(*signatureFrontend.module(), calls);
  bool sawScaleCall = false;
  for (const sere::CallExpr* call : calls) {
    if (call != nullptr && call->paramNames().size() >= 2) {
      sawScaleCall = true;
      break;
    }
  }
  if (!sawScaleCall) {
    return fail("unclosed scale(1, should still parse as a call with parameter names");
  }

  const std::filesystem::path preludePath = stdlibDir() / "prelude.sere";
  const std::string overlayPrelude =
      "extern \"C\" \"sere_input\"\n"
      "def input(prompt: str) -> str\n"
      "def abs(value: i32) -> i32:\n"
      "    return value\n";
  sere::Frontend overlayFrontend;
  overlayFrontend.setFileOverlay(
      {{sere::Frontend::overlayKey(preludePath), overlayPrelude}});
  const std::string overlayUser =
      "def main() -> i32:\n"
      "    name: str = input(\"n\")\n"
      "    return 0\n";
  if (!overlayFrontend.analyze("lsp_overlay.sere", overlayUser, stdlibDir())) {
    overlayFrontend.diagnostics().printAll(*overlayFrontend.source());
    return fail("overlay prelude should typecheck input()");
  }
  bool sawInput = false;
  for (const sere::SemanticSymbol& symbol : overlayFrontend.checker()->symbols()) {
    if (symbol.name == "input" && symbol.kind == "function") {
      sawInput = true;
      break;
    }
  }
  if (!sawInput) {
    return fail("overlay prelude must publish input as a function symbol");
  }
  return 0;
}
