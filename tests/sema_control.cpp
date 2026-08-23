/// @file sema_control.cpp
/// Type-checks arithmetic, comparisons, and if/while.

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/sema/TypeChecker.h"
#include "sere/source/SourceManager.h"
#include "sere/types/TypeContext.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const char* message) {
  std::cerr << "sema_control: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  const std::string text =
      "def main() -> i32:\n"
      "    total: i32 = 0\n"
      "    n: i32 = 0\n"
      "    while n < 3:\n"
      "        if n == 1:\n"
      "            total = total + 4\n"
      "            n = n + 1\n"
      "            continue\n"
      "        if n == 2:\n"
      "            break\n"
      "        else:\n"
      "            total = total + 1\n"
      "        n = n + 1\n"
      "    return total\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("sema_control.sere", text);
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll());
  std::unique_ptr<sere::Module> module = parser.parseModule();
  if (module == nullptr || diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return fail("parse failed");
  }
  sere::TypeContext types;
  sere::TypeChecker checker(types, diagnostics);
  if (!checker.check(*module)) {
    diagnostics.printAll(source);
    return fail("type check failed");
  }
  return 0;
}
