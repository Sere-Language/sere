/// @file sema_class.cpp
/// Type-checks class constructors and methods.

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
  std::cerr << "sema_class: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  const std::string text =
      "class Point:\n"
      "    x: i32\n"
      "    y: i32\n"
      "    def __init__(self, x: i32, y: i32) -> void:\n"
      "        self.x = x\n"
      "        self.y = y\n"
      "    def add(self, dx: i32, dy: i32) -> Point:\n"
      "        return Point(self.x + dx, self.y + dy)\n"
      "def main() -> i32:\n"
      "    p: Point = Point(1, 2)\n"
      "    q: Point = p.add(3, 4)\n"
      "    return q.x\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("sema_class.sere", text);
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

  const std::string inherited =
      "class Pet:\n"
      "    name: str\n"
      "    def __init__(self, name: str) -> void:\n"
      "        self.name = name\n"
      "    def id(self) -> i32:\n"
      "        return 1\n"
      "class Cat(Pet):\n"
      "    def __init__(self, name: str) -> void:\n"
      "        super().__init__(name)\n"
      "    def id(self) -> i32:\n"
      "        return super().id() + 1\n"
      "def main() -> i32:\n"
      "    c: Cat = Cat(\"z\")\n"
      "    return c.id()\n";
  sere::DiagnosticEngine inheritedDiagnostics;
  sere::SourceManager inheritedSource("sema_super.sere", inherited);
  sere::Lexer inheritedLexer(inheritedSource, inheritedDiagnostics);
  sere::Parser inheritedParser(inheritedDiagnostics, inheritedLexer.tokenizeAll());
  std::unique_ptr<sere::Module> inheritedModule = inheritedParser.parseModule();
  if (inheritedModule == nullptr || inheritedDiagnostics.hasErrors()) {
    inheritedDiagnostics.printAll(inheritedSource);
    return fail("super() sample failed to parse");
  }
  sere::TypeContext inheritedTypes;
  sere::TypeChecker inheritedChecker(inheritedTypes, inheritedDiagnostics);
  if (!inheritedChecker.check(*inheritedModule)) {
    inheritedDiagnostics.printAll(inheritedSource);
    return fail("super() should type check");
  }

  const std::string badSuper =
      "def main() -> i32:\n"
      "    super().__init__()\n"
      "    return 0\n";
  sere::DiagnosticEngine badDiagnostics;
  sere::SourceManager badSource("sema_super_bad.sere", badSuper);
  sere::Lexer badLexer(badSource, badDiagnostics);
  sere::Parser badParser(badDiagnostics, badLexer.tokenizeAll());
  std::unique_ptr<sere::Module> badModule = badParser.parseModule();
  sere::TypeContext badTypes;
  sere::TypeChecker badChecker(badTypes, badDiagnostics);
  if (badModule != nullptr && badChecker.check(*badModule)) {
    return fail("super() outside a method must fail");
  }
  return 0;
}
