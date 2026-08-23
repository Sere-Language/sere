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
  const std::string privateMethod =
      "class Box:\n"
      "    def __init__(self) -> void:\n"
      "        pass\n"
      "    @private def sneak(self) -> i32:\n"
      "        return 7\n"
      "    def ok(self) -> i32:\n"
      "        return self.sneak()\n"
      "def main() -> i32:\n"
      "    b: Box = Box()\n"
      "    return b.ok()\n";
  sere::DiagnosticEngine privateOk;
  sere::SourceManager privateOkSource("sema_private_ok.sere", privateMethod);
  sere::Lexer privateOkLexer(privateOkSource, privateOk);
  sere::Parser privateOkParser(privateOk, privateOkLexer.tokenizeAll());
  std::unique_ptr<sere::Module> privateOkModule = privateOkParser.parseModule();
  if (privateOkModule == nullptr || privateOk.hasErrors()) {
    privateOk.printAll(privateOkSource);
    return fail("private method used inside its class should parse");
  }
  sere::TypeContext privateOkTypes;
  sere::TypeChecker privateOkChecker(privateOkTypes, privateOk);
  if (!privateOkChecker.check(*privateOkModule)) {
    privateOk.printAll(privateOkSource);
    return fail("private method used inside its class should typecheck");
  }

  const std::string privateLeak =
      "class Box:\n"
      "    def __init__(self) -> void:\n"
      "        pass\n"
      "    @private def sneak(self) -> i32:\n"
      "        return 7\n"
      "def main() -> i32:\n"
      "    b: Box = Box()\n"
      "    return b.sneak()\n";
  sere::DiagnosticEngine privateBad;
  sere::SourceManager privateBadSource("sema_private_bad.sere", privateLeak);
  sere::Lexer privateBadLexer(privateBadSource, privateBad);
  sere::Parser privateBadParser(privateBad, privateBadLexer.tokenizeAll());
  std::unique_ptr<sere::Module> privateBadModule = privateBadParser.parseModule();
  if (privateBadModule == nullptr || privateBad.hasErrors()) {
    privateBad.printAll(privateBadSource);
    return fail("private leak sample failed to parse");
  }
  sere::TypeContext privateBadTypes;
  sere::TypeChecker privateBadChecker(privateBadTypes, privateBad);
  if (privateBadChecker.check(*privateBadModule) || !privateBad.hasErrors()) {
    return fail("calling a private method from outside the class should fail");
  }

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

  const std::string properties =
      "class Vec2:\n"
      "    @private x: i32\n"
      "    @private y: i32\n"
      "    def __init__(self, x: i32, y: i32) -> void:\n"
      "        self.x = x\n"
      "        self.y = y\n"
      "    @public x.get:\n"
      "        return self.x\n"
      "    @public y.get:\n"
      "        return self.y\n"
      "    @public x.set(value: i32) -> void:\n"
      "        self.x = value\n"
      "def main() -> i32:\n"
      "    v: Vec2 = Vec2(1, 2)\n"
      "    v.x = 10\n"
      "    return v.x + v.y\n";
  sere::DiagnosticEngine propertyOk;
  sere::SourceManager propertyOkSource("sema_property.sere", properties);
  sere::Lexer propertyOkLexer(propertyOkSource, propertyOk);
  sere::Parser propertyOkParser(propertyOk, propertyOkLexer.tokenizeAll());
  std::unique_ptr<sere::Module> propertyOkModule = propertyOkParser.parseModule();
  if (propertyOkModule == nullptr || propertyOk.hasErrors()) {
    propertyOk.printAll(propertyOkSource);
    return fail("public property get/set should parse");
  }
  sere::TypeContext propertyOkTypes;
  sere::TypeChecker propertyOkChecker(propertyOkTypes, propertyOk);
  if (!propertyOkChecker.check(*propertyOkModule)) {
    propertyOk.printAll(propertyOkSource);
    return fail("public property get/set should typecheck");
  }

  const std::string privateSetter =
      "class Vec2:\n"
      "    @private x: i32\n"
      "    def __init__(self, x: i32) -> void:\n"
      "        self.x = x\n"
      "    @public x.get:\n"
      "        return self.x\n"
      "    @private x.set(value: i32) -> void:\n"
      "        self.x = value\n"
      "def main() -> i32:\n"
      "    v: Vec2 = Vec2(1)\n"
      "    v.x = 10\n"
      "    return v.x\n";
  sere::DiagnosticEngine propertyBad;
  sere::SourceManager propertyBadSource("sema_property_private.sere", privateSetter);
  sere::Lexer propertyBadLexer(propertyBadSource, propertyBad);
  sere::Parser propertyBadParser(propertyBad, propertyBadLexer.tokenizeAll());
  std::unique_ptr<sere::Module> propertyBadModule = propertyBadParser.parseModule();
  if (propertyBadModule == nullptr || propertyBad.hasErrors()) {
    propertyBad.printAll(propertyBadSource);
    return fail("private setter sample failed to parse");
  }
  sere::TypeContext propertyBadTypes;
  sere::TypeChecker propertyBadChecker(propertyBadTypes, propertyBad);
  if (propertyBadChecker.check(*propertyBadModule) || !propertyBad.hasErrors()) {
    return fail("assigning through a private setter from outside the class should fail");
  }
  return 0;
}
