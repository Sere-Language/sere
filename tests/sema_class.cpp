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

  const std::string abstractHook =
      "class Application:\n"
      "    def __init__(self) -> void:\n"
      "        pass\n"
      "    @abstract\n"
      "    def on_update(self, dt: f64) -> void:\n"
      "        panic(\"override me\")\n"
      "class Main(Application):\n"
      "    def __init__(self) -> void:\n"
      "        super().__init__()\n"
      "def main() -> i32:\n"
      "    app: Main = Main()\n"
      "    return 0\n";
  sere::DiagnosticEngine hookDiagnostics;
  sere::SourceManager hookSource("sema_abstract_hook.sere", abstractHook);
  sere::Lexer hookLexer(hookSource, hookDiagnostics);
  sere::Parser hookParser(hookDiagnostics, hookLexer.tokenizeAll());
  std::unique_ptr<sere::Module> hookModule = hookParser.parseModule();
  if (hookModule == nullptr || hookDiagnostics.hasErrors()) {
    hookDiagnostics.printAll(hookSource);
    return fail("abstract method with a body should parse");
  }
  sere::TypeContext hookTypes;
  sere::TypeChecker hookChecker(hookTypes, hookDiagnostics);
  if (!hookChecker.check(*hookModule)) {
    hookDiagnostics.printAll(hookSource);
    return fail("subclass may be constructed when abstract methods have a default body");
  }

  const std::string abstractPass =
      "class Animal:\n"
      "    @abstract\n"
      "    def speak(self) -> i32:\n"
      "        pass\n"
      "class Mute(Animal):\n"
      "    def __init__(self) -> void:\n"
      "        pass\n"
      "def main() -> i32:\n"
      "    m: Mute = Mute()\n"
      "    return 0\n";
  sere::DiagnosticEngine passDiagnostics;
  sere::SourceManager passSource("sema_abstract_pass.sere", abstractPass);
  sere::Lexer passLexer(passSource, passDiagnostics);
  sere::Parser passParser(passDiagnostics, passLexer.tokenizeAll());
  std::unique_ptr<sere::Module> passModule = passParser.parseModule();
  sere::TypeContext passTypes;
  sere::TypeChecker passChecker(passTypes, passDiagnostics);
  if (passModule != nullptr && passChecker.check(*passModule)) {
    return fail("subclass of pass-only @abstract must stay abstract");
  }

  const std::string typeObject =
      "class BaseApplication:\n"
      "    def __init__(self) -> void:\n"
      "        pass\n"
      "class BaseWindow:\n"
      "    def __init__(self) -> void:\n"
      "        pass\n"
      "class s2d:\n"
      "    @public version: str = \"0.1.0\"\n"
      "    @public static Application: type[BaseApplication] = BaseApplication\n"
      "    @public static Window: type[BaseWindow] = BaseWindow\n"
      "def main() -> i32:\n"
      "    app: BaseApplication = s2d.Application()\n"
      "    win: BaseWindow = s2d.Window()\n"
      "    return 0\n";
  sere::DiagnosticEngine typeObjectDiagnostics;
  sere::SourceManager typeObjectSource("sema_type_object.sere", typeObject);
  sere::Lexer typeObjectLexer(typeObjectSource, typeObjectDiagnostics);
  sere::Parser typeObjectParser(typeObjectDiagnostics, typeObjectLexer.tokenizeAll());
  std::unique_ptr<sere::Module> typeObjectModule = typeObjectParser.parseModule();
  if (typeObjectModule == nullptr || typeObjectDiagnostics.hasErrors()) {
    typeObjectDiagnostics.printAll(typeObjectSource);
    return fail("type[T] static class fields should parse");
  }
  sere::TypeContext typeObjectTypes;
  sere::TypeChecker typeObjectChecker(typeObjectTypes, typeObjectDiagnostics);
  if (!typeObjectChecker.check(*typeObjectModule)) {
    typeObjectDiagnostics.printAll(typeObjectSource);
    return fail("type[T] static class fields should typecheck");
  }

  const std::string decorated =
      "def identity(fn: Callable) -> Callable:\n"
      "    return fn\n"
      "def factory(with_params: bool) -> Callable:\n"
      "    return identity\n"
      "class Hook:\n"
      "    def wrap(self, fn: Callable) -> Callable:\n"
      "        return fn\n"
      "@identity\n"
      "def add(a: i32, b: i32) -> i32:\n"
      "    return a + b\n"
      "@factory(with_params=True)\n"
      "def mul(a: i32, b: i32) -> i32:\n"
      "    return a * b\n"
      "@Hook.wrap\n"
      "def sub(a: i32, b: i32) -> i32:\n"
      "    return a - b\n"
      "class Box:\n"
      "    value: i32\n"
      "    def __init__(self, value: i32) -> void:\n"
      "        self.value = value\n"
      "    @identity\n"
      "    def get(self) -> i32:\n"
      "        return self.value\n"
      "class Point:\n"
      "    x: i32\n"
      "    def __init__(self, x: i32) -> void:\n"
      "        self.x = x\n"
      "def main() -> i32:\n"
      "    p: Point = Point(1)\n"
      "    b: Box = Box(2)\n"
      "    return add(1, 2) + mul(2, 3) + sub(5, 1) + b.get() + p.x\n";
  sere::DiagnosticEngine decoDiagnostics;
  sere::SourceManager decoSource("sema_deco.sere", decorated);
  sere::Lexer decoLexer(decoSource, decoDiagnostics);
  sere::Parser decoParser(decoDiagnostics, decoLexer.tokenizeAll());
  std::unique_ptr<sere::Module> decoModule = decoParser.parseModule();
  if (decoModule == nullptr || decoDiagnostics.hasErrors()) {
    decoDiagnostics.printAll(decoSource);
    return fail("custom decorators should parse");
  }
  sere::TypeContext decoTypes;
  sere::TypeChecker decoChecker(decoTypes, decoDiagnostics);
  if (!decoChecker.check(*decoModule)) {
    decoDiagnostics.printAll(decoSource);
    return fail("custom decorators should typecheck");
  }
  return 0;
}
