/// @file parse_fn.cpp
/// Checks that the parser accepts a typed function.

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const char* message) {
  std::cerr << "parse_fn: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  const std::string text = "def main(argv: list[str]) -> i32:\n    ptr: Unique[i32]\n    return 0\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("parse_fn.sere", text);
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll());
  const std::unique_ptr<sere::Module> module = parser.parseModule();
  if (module == nullptr || diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return fail("parse failed");
  }
  if (module->statements().size() != 1 ||
      module->statements().front()->kind() != sere::NodeKind::FunctionDef) {
    return fail("expected one function");
  }
  return 0;
}
