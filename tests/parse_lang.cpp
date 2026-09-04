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

} // namespace

int main() {
  {
    sere::DiagnosticEngine diagnostics;
    const auto module = parseText(R"sere(f"before\n\t{42}\r\n\\n\"{{ok}}"
f'only\ntext'
f"""triple\ntext"""
)sere", diagnostics);
    if (module == nullptr || diagnostics.hasErrors() || module->statements().size() != 3) {
      return fail("f-strings with escapes should parse");
    }
    const auto& statement = static_cast<const sere::ExprStmt&>(*module->statements()[0]);
    const auto& string = static_cast<const sere::InterpolatedStringExpr&>(statement.expression());
    if (string.parts().size() != 3 || string.parts()[0].literal != "before\n\t" ||
        string.parts()[1].value == nullptr ||
        string.parts()[2].literal != "\r\n\\n\"{ok}") {
      return fail("f-string escapes should decode around interpolation, preserving escaped backslashes");
    }
    const std::string expected[] = {"only\ntext", "triple\ntext"};
    for (std::size_t index = 1; index < 3; ++index) {
      const auto& stmt = static_cast<const sere::ExprStmt&>(*module->statements()[index]);
      const auto& value = static_cast<const sere::InterpolatedStringExpr&>(stmt.expression());
      if (value.parts().size() != 1 || value.parts()[0].literal != expected[index - 1]) {
        return fail("single and triple quoted f-strings should decode escapes without interpolation");
      }
    }
  }
  {
    const std::string genericEnum = "enum Result[T]:\n"
                                    "    Ok(T)\n"
                                    "    Err(str)\n";
    sere::DiagnosticEngine diagnostics;
    sere::SourceManager source("parse_generic_enum.sere", genericEnum);
    sere::Lexer lexer(source, diagnostics);
    sere::Parser parser(diagnostics, lexer.tokenizeAll());
    std::unique_ptr<sere::Module> module = parser.parseModule();
    if (module == nullptr || diagnostics.hasErrors() || module->statements().size() != 1 ||
        module->statements()[0]->kind() != sere::NodeKind::EnumDef) {
      diagnostics.printAll(source);
      return fail("generic enum should parse");
    }
    const auto& result = static_cast<const sere::EnumDef&>(*module->statements()[0]);
    if (result.typeParams() != std::vector<std::string>{"T"} || result.variants().size() != 2) {
      return fail("generic enum should retain type parameters and variants");
    }
  }
  sere::DiagnosticEngine emptyInitDiagnostics;
  const std::unique_ptr<sere::Module> emptyInit =
      parseText("def main() -> i32:\n    ptr: Unique[i32] =\n    return 0\n", emptyInitDiagnostics);
  if (!emptyInitDiagnostics.hasErrors()) {
    return fail("bare '=' must be a parse error");
  }

  sere::DiagnosticEngine okDiagnostics;
  const std::unique_ptr<sere::Module> ok = parseText("type Count = i32\n"
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
  const std::unique_ptr<sere::Module> collections =
      parseText("class Counter:\n"
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
  const std::unique_ptr<sere::Module> oop = parseText("from util import double\n"
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
  const std::unique_ptr<sere::Module> lang =
      parseText("enum Color:\n"
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
  const std::unique_ptr<sere::Module> inlineEnum = parseText("enum Color: RED / GREEN\n"
                                                             "enum Shade { Dark, Light }\n"
                                                             "def main() -> i32:\n"
                                                             "    tone: Color = Color.GREEN\n"
                                                             "    return 0\n",
                                                             inlineEnumDiagnostics);
  if (inlineEnum == nullptr || inlineEnumDiagnostics.hasErrors()) {
    return fail("inline enum forms should parse");
  }

  sere::DiagnosticEngine extraDiagnostics;
  const std::unique_ptr<sere::Module> extra =
      parseText("def scale(value: i32, factor: i32 = 2) -> i32:\n"
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
  const std::unique_ptr<sere::Module> slices =
      parseText("type Number = i32 | f32\n"
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

  sere::DiagnosticEngine typeObjectDiagnostics;
  const std::unique_ptr<sere::Module> typeObjects =
      parseText("class BaseApplication:\n"
                "    pass\n"
                "class BaseWindow:\n"
                "    pass\n"
                "class s2d:\n"
                "    @public version: str = \"0.1.0\"\n"
                "    @public static Application: type[BaseApplication] = BaseApplication\n"
                "    @public static Window: type[BaseWindow] = BaseWindow\n"
                "def main() -> i32:\n"
                "    return 0\n",
                typeObjectDiagnostics);
  if (typeObjects == nullptr || typeObjectDiagnostics.hasErrors()) {
    return fail("type[T] class fields should parse");
  }

  sere::DiagnosticEngine floatParseDiagnostics;
  const std::unique_ptr<sere::Module> floats = parseText("def main() -> f64:\n"
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
  const std::unique_ptr<sere::Module> native = parseText("extern \"C\" \"native_add\"\n"
                                                         "def add(left: i32, right: i32) -> i32\n"
                                                         "extern \"native_sub\"\n"
                                                         "def sub(left: i32, right: i32) -> i32\n",
                                                         externDiagnostics);
  if (native == nullptr || externDiagnostics.hasErrors()) {
    return fail("extern C declarations should parse");
  }

  sere::DiagnosticEngine suiteColonDiagnostics;
  const std::unique_ptr<sere::Module> suiteColon =
      parseText("def min(left: i32, right: i32) -> i32:\n"
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
  const std::unique_ptr<sere::Module> quotes = parseText("def main() -> i32:\n"
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
  const std::unique_ptr<sere::Module> pointers =
      parseText("def main() -> i32:\n"
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
  const std::unique_ptr<sere::Module> pythonish =
      parseText("def greet(name):\n"
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
  const std::unique_ptr<sere::Module> dotted = parseText("import gl\n"
                                                         "def create_window() -> gl.Window:\n"
                                                         "    return gl.Window(1, 1, \"x\")\n",
                                                         dottedDiagnostics);
  if (dotted == nullptr || dottedDiagnostics.hasErrors()) {
    dottedDiagnostics.printAll();
    return fail("qualified return types like gl.Window must parse");
  }

  sere::DiagnosticEngine propertyDiagnostics;
  const std::unique_ptr<sere::Module> properties =
      parseText("class Vec2:\n"
                "    @private x: i32\n"
                "    @private y: i32\n"
                "    @public x.get:\n"
                "        return self.x\n"
                "    @public y.get:\n"
                "        return self.y\n"
                "    @private x.set(value: i32) -> void:\n"
                "        self.x = value\n",
                propertyDiagnostics);
  if (properties == nullptr || propertyDiagnostics.hasErrors()) {
    propertyDiagnostics.printAll();
    return fail("property get/set accessors must parse");
  }
  bool sawGetX = false;
  bool sawGetY = false;
  bool sawSetX = false;
  for (const std::unique_ptr<sere::Stmt>& statement : properties->statements()) {
    if (statement->kind() != sere::NodeKind::ClassDef) {
      continue;
    }
    const auto& classDef = static_cast<const sere::ClassDef&>(*statement);
    for (const std::unique_ptr<sere::FunctionDef>& method : classDef.methods()) {
      if (method->propertyKind() == sere::PropertyKind::Get && method->propertyName() == "x" &&
          method->name() == "__get_x") {
        sawGetX = true;
      }
      if (method->propertyKind() == sere::PropertyKind::Get && method->propertyName() == "y" &&
          method->name() == "__get_y") {
        sawGetY = true;
      }
      if (method->propertyKind() == sere::PropertyKind::Set && method->propertyName() == "x" &&
          method->name() == "__set_x") {
        sawSetX = true;
      }
    }
  }
  if (!sawGetX || !sawGetY || !sawSetX) {
    return fail("x.get / y.get / x.set must become __get_x / __get_y / __set_x");
  }

  sere::DiagnosticEngine callableDiagnostics;
  const std::unique_ptr<sere::Module> callables =
      parseText("def apply(cb: Callable[[str, str, i32], i32], name: str) -> i32:\n"
                "    return cb(name, name, 1)\n"
                "def ret_only(cb: Callable[i32]) -> i32:\n"
                "    return cb(1)\n"
                "def prefix(cb: Callable[[i32, ...], i32]) -> i32:\n"
                "    return cb(1, 2)\n"
                "def any_cb(cb: Callable) -> void:\n"
                "    cb()\n"
                "def fn_only(cb: Function[[i32], i32]) -> i32:\n"
                "    return cb(1)\n"
                "def make(cls: Class[i32]) -> void:\n"
                "    pass\n",
                callableDiagnostics);
  if (callables == nullptr || callableDiagnostics.hasErrors()) {
    callableDiagnostics.printAll();
    return fail("Callable / Function / Class type syntax must parse");
  }

  sere::DiagnosticEngine listDiagnostics;
  const std::unique_ptr<sere::Module> lists = parseText("def main() -> i32:\n"
                                                        "    a: i32 = 1\n"
                                                        "    b: i32 = 2\n"
                                                        "    xs = [\n"
                                                        "        a,\n"
                                                        "        b,\n"
                                                        "    ]\n"
                                                        "    return xs[0]\n",
                                                        listDiagnostics);
  if (lists == nullptr || listDiagnostics.hasErrors()) {
    listDiagnostics.printAll();
    return fail("multiline list literals with trailing commas must parse");
  }

  sere::DiagnosticEngine decoDiagnostics;
  const std::unique_ptr<sere::Module> decos =
      parseText("def identity(fn: Callable) -> Callable:\n"
                "    return fn\n"
                "class Hook:\n"
                "    def wrap(self, fn: Callable) -> Callable:\n"
                "        return fn\n"
                "@identity\n"
                "@identity(with_params=True)\n"
                "@Hook.wrap\n"
                "def f() -> i32:\n"
                "    return 1\n"
                "@identity\n"
                "class Box:\n"
                "    x: i32\n"
                "    @identity\n"
                "    def get(self) -> i32:\n"
                "        return self.x\n"
                "@identity\n"
                "struct Point:\n"
                "    x: i32\n",
                decoDiagnostics);
  if (decos == nullptr || decoDiagnostics.hasErrors()) {
    decoDiagnostics.printAll();
    return fail("custom decorators including Class.method must parse");
  }

  sere::DiagnosticEngine dottedBaseDiagnostics;
  const std::unique_ptr<sere::Module> dottedBases =
      parseText("class App(seres2d.Application):\n"
                "    window: seres2d.Window\n"
                "    def draw(self, batch: seres2d.Batch) -> void:\n"
                "        pass\n"
                "class Child(ui.widget.View, mixin.Drawable):\n"
                "    pass\n",
                dottedBaseDiagnostics);
  if (dottedBases == nullptr || dottedBaseDiagnostics.hasErrors()) {
    dottedBaseDiagnostics.printAll();
    return fail("dotted class bases and field types must parse");
  }
  return 0;
}
