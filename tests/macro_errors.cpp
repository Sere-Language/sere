/// @file macro_errors.cpp
/// Unknown macros and leftover invokes are diagnostics, not crashes.

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/macro/Expander.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const char* message) {
  std::cerr << "macro_errors: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  const std::string text =
      "def main() -> i32:\n"
      "    return missing!(1)\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("macro_errors.sere", text);
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll(), &source);
  std::unique_ptr<sere::Module> module = parser.parseModule();
  if (module == nullptr || diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return fail("parse failed");
  }
  sere::MacroEnv env;
  sere::MacroExpander expander(diagnostics, env);
  const bool ok = expander.expandModule(*module);
  if (ok || !diagnostics.hasErrors()) {
    return fail("expected unknown macro diagnostic");
  }
  return 0;
}
