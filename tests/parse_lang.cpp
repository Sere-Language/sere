/// @file parse_lang.cpp
/// Parser coverage for empty initializers, arithmetic, and control flow.

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const char* message) {
  std::cerr << "parse_lang: " << message << '\n';
  return 1;
}

[[nodiscard]] std::unique_ptr<sere::Module> parseText(const std::string& text,
                                                      sere::DiagnosticEngine& diagnostics) {
  sere::SourceManager source("parse_lang.sere", text);
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll());
  return parser.parseModule();
}

}  // namespace

int main() {
  sere::DiagnosticEngine emptyInitDiagnostics;
  const std::unique_ptr<sere::Module> emptyInit = parseText(
      "def main() -> i32:\n    ptr: Unique[i32] =\n    return 0\n", emptyInitDiagnostics);
  if (!emptyInitDiagnostics.hasErrors()) {
    return fail("bare '=' must be a parse error");
  }

  sere::DiagnosticEngine okDiagnostics;
  const std::unique_ptr<sere::Module> ok = parseText(
      "type Count = i32\n"
      "def main() -> i32:\n"
      "    n: Count = 1 + 2 * 3\n"
      "    if n > 0 and not False:\n"
      "        n = n - 1\n"
      "    elif n == 0:\n"
      "        pass\n"
      "    else:\n"
      "        n = -n\n"
      "    while n < 4:\n"
      "        if n == 2:\n"
      "            n = n + 1\n"
      "            continue\n"
      "        if n == 3:\n"
      "            break\n"
      "        n = n + 1\n"
      "    print(f\"n={n}\")\n"
      "    wide = n as i64\n"
      "    return n\n",
      okDiagnostics);
  if (ok == nullptr || okDiagnostics.hasErrors()) {
    return fail("control-flow program should parse");
  }

  sere::DiagnosticEngine collectionDiagnostics;
  const std::unique_ptr<sere::Module> collections = parseText(
      "class Counter:\n"
      "    @public\n"
      "    value: i32\n"
      "    @private\n"
      "    secret: i32\n"
      "    static total: i32 = 0\n"
      "    def __init__(self, value: i32) -> void:\n"
      "        self.value = value\n"
      "static module_count: i32 = 0\n"
      "def main() -> i32:\n"
      "    xs: list[i32] = [1, 2, 3]\n"
      "    xs[0] = 10\n"
      "    ages: dict[str, i32] = {\"ada\": 36}\n"
      "    ages[\"ada\"] = 37\n"
      "    empty: dict[str, i32] = dict[str, i32]()\n"
      "    arr: array[i32] = array[i32](1, 2)\n"
      "    return xs[1]\n",
      collectionDiagnostics);
  if (collections == nullptr || collectionDiagnostics.hasErrors()) {
    return fail("collections and visibility program should parse");
  }

  sere::DiagnosticEngine oopDiagnostics;
  const std::unique_ptr<sere::Module> oop = parseText(
      "from util import double\n"
      "from util import *\n"
      "import util as u\n"
      "import util.math as math\n"
      "def identity[T](value: T) -> T:\n"
      "    return value\n"
      "class Animal:\n"
      "    @abstract\n"
      "    def speak(self) -> i32:\n"
      "        pass\n"
      "class Dog(Animal):\n"
      "    @override\n"
      "    def speak(self) -> i32:\n"
      "        return 1\n"
      "class Box[T]:\n"
      "    value: T\n"
      "def main() -> i32:\n"
      "    n: i32 = 1\n"
      "    n += 2\n"
      "    ++n\n"
      "    return n\n",
      oopDiagnostics);
  if (oop == nullptr || oopDiagnostics.hasErrors()) {
    return fail("oop/import/compound program should parse");
  }

  sere::DiagnosticEngine langDiagnostics;
  const std::unique_ptr<sere::Module> lang = parseText(
      "enum Color:\n"
      "    Red\n"
      "    Green = 2\n"
      "def scale(value: i32, factor: i32 = 2) -> i32:\n"
      "    return value * factor\n"
      "def main() -> i32:\n"
      "    total: i32 = 0\n"
      "    for n in range(0, 3):\n"
      "        total += n\n"
      "        assert n is n\n"
      "    return scale(3)\n",
      langDiagnostics);
  if (lang == nullptr || langDiagnostics.hasErrors()) {
    return fail("enum/for/default/assert program should parse");
  }

  sere::DiagnosticEngine inlineEnumDiagnostics;
  const std::unique_ptr<sere::Module> inlineEnum = parseText(
      "enum Color: RED / GREEN\n"
      "enum Shade { Dark, Light }\n"
      "def main() -> i32:\n"
      "    tone: Color = Color.GREEN\n"
      "    return 0\n",
      inlineEnumDiagnostics);
  if (inlineEnum == nullptr || inlineEnumDiagnostics.hasErrors()) {
    return fail("inline enum forms should parse");
  }

  sere::DiagnosticEngine extraDiagnostics;
  const std::unique_ptr<sere::Module> extra = parseText(
      "def scale(value: i32, factor: i32 = 2) -> i32:\n"
      "    return value * factor\n"
      "def main(argv: list[str]) -> i32:\n"
      "    n: i32 | f32 = 1\n"
      "    print(arg for arg in argv)\n"
      "    xs: list[i32] = [x for x in range(0, ..., 3)]\n"
      "    return scale(3) + (0 ... 4)\n",
      extraDiagnostics);
  if (extra == nullptr || extraDiagnostics.hasErrors()) {
    return fail("defaults, unions, comprehensions, and range ellipsis should parse");
  }

  sere::DiagnosticEngine sliceDiagnostics;
  const std::unique_ptr<sere::Module> slices = parseText(
      "type Number = i32 | f32\n"
      "def main() -> i32:\n"
      "    xs: list[i32] = [1, 2, 3]\n"
      "    a = xs[1:]\n"
      "    b = xs[:2]\n"
      "    c = xs[1:2]\n"
      "    d = xs[:]\n"
      "    print(typeof(a), a.__name__, inspect(main))\n"
      "    return 0\n",
      sliceDiagnostics);
  if (slices == nullptr || sliceDiagnostics.hasErrors()) {
    return fail("slices, type aliases, and inspect calls should parse");
  }

  sere::DiagnosticEngine floatParseDiagnostics;
  const std::unique_ptr<sere::Module> floats = parseText(
      "def main() -> f64:\n"
      "    a: f64 = 1.0\n"
      "    b: f32 = 1.0f\n"
      "    c: f64 = .5 + 2. + 3e2 + 4.5e-1\n"
      "    d: f64 = 1 + 2.0\n"
      "    return a + c + d\n",
      floatParseDiagnostics);
  if (floats == nullptr || floatParseDiagnostics.hasErrors()) {
    return fail("float literals should parse");
  }

  sere::DiagnosticEngine externDiagnostics;
  const std::unique_ptr<sere::Module> native = parseText(
      "extern \"C\" \"native_add\"\n"
      "def add(left: i32, right: i32) -> i32\n"
      "extern \"native_sub\"\n"
      "def sub(left: i32, right: i32) -> i32\n",
      externDiagnostics);
  if (native == nullptr || externDiagnostics.hasErrors()) {
    return fail("extern C declarations should parse");
  }

  sere::DiagnosticEngine suiteColonDiagnostics;
  const std::unique_ptr<sere::Module> suiteColon = parseText(
      "def min(left: i32, right: i32) -> i32:\n"
      "    if left < right:\n"
      "        return left\n"
      "    return right\n"
      "def main() -> i32:\n"
      "    flag: bool = True\n"
      "    if flag:\n"
      "        pass\n"
      "    xs: list[i32] = [1]\n"
      "    for x in xs:\n"
      "        pass\n"
      "    return min(1, 2)\n",
      suiteColonDiagnostics);
  if (suiteColon == nullptr || suiteColonDiagnostics.hasErrors()) {
    return fail("if/for with an identifier before ':' should parse");
  }

  sere::DiagnosticEngine quoteParseDiagnostics;
  const std::unique_ptr<sere::Module> quotes = parseText(
      "def main() -> i32:\n"
      "    a: str = 'hi'\n"
      "    b: str = '''ab'''\n"
      "    c: str = \"\"\"cd\"\"\"\n"
      "    d: regex = `a+`\n"
      "    e: str = f'x={a}'\n"
      "    c: i8 = 'A'\n"
      "    return 0\n",
      quoteParseDiagnostics);
  if (quotes == nullptr || quoteParseDiagnostics.hasErrors()) {
    return fail("single, triple, regex, and f-string quotes should parse");
  }

  sere::DiagnosticEngine pointerDiagnostics;
  const std::unique_ptr<sere::Module> pointers = parseText(
      "def main() -> i32:\n"
      "    n: i32 = 10\n"
      "    p: Ptr[i32] = &n\n"
      "    *p = 20\n"
      "    *p += 1\n"
      "    owned: Unique[i32] = unique[i32](1)\n"
      "    *owned = 3\n"
      "    mask: i32 = n & 1\n"
      "    return *p + *owned + mask\n",
      pointerDiagnostics);
  if (pointers == nullptr || pointerDiagnostics.hasErrors()) {
    return fail("address-of and dereference should parse");
  }

  sere::DiagnosticEngine pythonishDiagnostics;
  const std::unique_ptr<sere::Module> pythonish = parseText(
      "def greet(name):\n"
      "    print(name)\n"
      "def main() -> i32:\n"
      "    xs = [1, 2, 3]\n"
      "    a, b = 1, 2\n"
      "    pair = (3, 4)\n"
      "    if (n := 1) > 0:\n"
      "        add1 = lambda (x: i32) -> i32: x + 1\n"
      "        const limit = 4\n"
      "        n //= 1\n"
      "        n **= 1\n"
      "    with ctx as value:\n"
      "        defer:\n"
      "            del xs[0]\n"
      "        pass\n"
      "    return 0\n",
      pythonishDiagnostics);
  if (pythonish == nullptr || pythonishDiagnostics.hasErrors()) {
    return fail("python-superset syntax should parse");
  }
  bool foundWithBinding = false;
  for (const std::unique_ptr<sere::Stmt>& statement : pythonish->statements()) {
    if (statement->kind() != sere::NodeKind::FunctionDef) {
      continue;
    }
    const auto& function = static_cast<const sere::FunctionDef&>(*statement);
    for (const std::unique_ptr<sere::Stmt>& inner : function.body()) {
      if (inner->kind() != sere::NodeKind::WithStmt) {
        continue;
      }
      const auto& with = static_cast<const sere::WithStmt&>(*inner);
      if (with.name() != "value") {
        return fail("with ctx as value must bind 'value', not parse as a cast");
      }
      foundWithBinding = true;
    }
  }
  if (!foundWithBinding) {
    return fail("python-superset sample should include a with-as binding");
  }

  sere::DiagnosticEngine printStmtDiagnostics;
  const std::unique_ptr<sere::Module> printStmt =
      parseText("def main() -> i32:\n    print x\n    return 0\n", printStmtDiagnostics);
  if (!printStmtDiagnostics.hasErrors()) {
    return fail("print as a statement must be a SyntaxError");
  }

  sere::DiagnosticEngine dottedDiagnostics;
  const std::unique_ptr<sere::Module> dotted = parseText(
      "import gl\n"
      "def create_window() -> gl.Window:\n"
      "    return gl.Window(1, 1, \"x\")\n",
      dottedDiagnostics);
  if (dotted == nullptr || dottedDiagnostics.hasErrors()) {
    dottedDiagnostics.printAll();
    return fail("qualified return types like gl.Window must parse");
  }

  sere::DiagnosticEngine kwParseDiagnostics;
  const std::unique_ptr<sere::Module> kwCall = parseText(
      "def add(a: i32, b: i32 = 0) -> i32:\n"
      "    return a + b\n"
      "def main() -> i32:\n"
      "    return add(b=2, a=3)\n",
      kwParseDiagnostics);
  if (kwCall == nullptr || kwParseDiagnostics.hasErrors()) {
    kwParseDiagnostics.printAll();
    return fail("keyword arguments at call sites must parse");
  }
  return 0;
}
