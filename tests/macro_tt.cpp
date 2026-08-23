/// @file macro_tt.cpp
/// Token trees group delimiter pairs including indent blocks.

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/macro/TokenTree.h"
#include "sere/source/SourceManager.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << "macro_tt: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  const std::string text = "vec!(1, 2)\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("macro_tt.sere", text);
  sere::Lexer lexer(source, diagnostics);
  const std::vector<sere::Token> tokens = lexer.tokenizeAll();
  if (diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return fail("lex failed");
  }
  const std::vector<sere::TokenTree> trees = sere::buildTokenTrees(tokens);
  bool foundGroup = false;
  for (const sere::TokenTree& tree : trees) {
    if (tree.kind == sere::TokenTreeKind::Delimited && tree.opener == sere::TokenKind::LParen) {
      foundGroup = true;
    }
  }
  if (!foundGroup) {
    return fail("expected parenthesized token tree");
  }
  return 0;
}
