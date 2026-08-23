/// @file sema_types.cpp
/// Checks Unique/Shared/list typing for a main function.

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
  std::cerr << "sema_types: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  const std::string text =
      "type Count = i32\n"
      "def main(argv: list[str]) -> i32:\n"
      "    n: Count = 0\n"
      "    ptr: Unique[i32] = unique[i32](1)\n"
      "    shared_ptr: Shared[i32]\n"
      "    raw: Ptr[i32] = ptr as Ptr[i32]\n"
      "    wide: i64 = i64(n)\n"
      "    xs: list[i32] = [1, 2, 3]\n"
      "    xs.append(4)\n"
      "    xs[0] = 10\n"
      "    arr: array[i32] = array[i32](1, 2)\n"
      "    ages: dict[str, i32] = {\"ada\": 36}\n"
      "    empty: dict[str, i32] = dict[str, i32]()\n"
      "    print(ptr, raw, wide, len(xs), xs[1], len(arr), ages[\"ada\"], len(empty))\n"
      "    return n\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("sema_types.sere", text);
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

  const std::string visibility =
      "class Counter:\n"
      "    @public\n"
      "    value: i32\n"
      "    @private\n"
      "    secret: i32\n"
      "    static total: i32 = 0\n"
      "    def __init__(self, value: i32) -> void:\n"
      "        self.value = value\n"
      "        self.secret = 1\n"
      "        Counter.total = Counter.total + 1\n"
      "static module_count: i32 = 0\n"
      "def main() -> i32:\n"
      "    c: Counter = Counter(5)\n"
      "    module_count = module_count + 1\n"
      "    return c.value + Counter.total + module_count\n";
  sere::DiagnosticEngine visDiagnostics;
  sere::SourceManager visSource("sema_vis.sere", visibility);
  sere::Lexer visLexer(visSource, visDiagnostics);
  sere::Parser visParser(visDiagnostics, visLexer.tokenizeAll());
  std::unique_ptr<sere::Module> visModule = visParser.parseModule();
  sere::TypeContext visTypes;
  sere::TypeChecker visChecker(visTypes, visDiagnostics);
  if (visModule == nullptr || !visChecker.check(*visModule)) {
    visDiagnostics.printAll(visSource);
    return fail("public/static fields should type check");
  }

  const std::string privateLeak =
      "class Box:\n"
      "    @private\n"
      "    secret: i32\n"
      "    def __init__(self) -> void:\n"
      "        self.secret = 1\n"
      "def main() -> i32:\n"
      "    b: Box = Box()\n"
      "    return b.secret\n";
  sere::DiagnosticEngine leakDiagnostics;
  sere::SourceManager leakSource("sema_priv.sere", privateLeak);
  sere::Lexer leakLexer(leakSource, leakDiagnostics);
  sere::Parser leakParser(leakDiagnostics, leakLexer.tokenizeAll());
  std::unique_ptr<sere::Module> leakModule = leakParser.parseModule();
  sere::TypeContext leakTypes;
  sere::TypeChecker leakChecker(leakTypes, leakDiagnostics);
  if (leakModule != nullptr && leakChecker.check(*leakModule)) {
    return fail("private field access from main must fail");
  }

  const std::string introspect =
      "type Number = i32 | i64\n"
      "class Box:\n"
      "    @public value: i32\n"
      "    def add(self, n: i32) -> i32:\n"
      "        return self.value + n\n"
      "def scale(value: i32) -> i32:\n"
      "    return value\n"
      "def main() -> i32:\n"
      "    xs: list[i32] = [1, 2, 3, 4]\n"
      "    tail: list[i32] = xs[1:]\n"
      "    head: list[i32] = xs[:2]\n"
      "    mid: list[i32] = xs[1:3]\n"
      "    copy: list[i32] = xs[:]\n"
      "    small: i32 = 1\n"
      "    wide: i64 = 2\n"
      "    ok: bool = small < wide\n"
      "    total: i64 = small + wide\n"
      "    n: Number = 3\n"
      "    name: str = small.__name__\n"
      "    kind: str = typeof(small)\n"
      "    typed: bool = isinstance[i32](small)\n"
      "    also: bool = isinstance(small, i32)\n"
      "    info: str = inspect(scale)\n"
      "    members: list[str] = dir(Box)\n"
      "    return n as i32\n";
  sere::DiagnosticEngine introDiagnostics;
  sere::SourceManager introSource("sema_intro.sere", introspect);
  sere::Lexer introLexer(introSource, introDiagnostics);
  sere::Parser introParser(introDiagnostics, introLexer.tokenizeAll());
  std::unique_ptr<sere::Module> introModule = introParser.parseModule();
  sere::TypeContext introTypes;
  sere::TypeChecker introChecker(introTypes, introDiagnostics);
  if (introModule == nullptr || !introChecker.check(*introModule)) {
    introDiagnostics.printAll(introSource);
    return fail("slices, mixed integers, aliases, and inspect should type check");
  }

  const std::string inferred =
      "def main() -> void:\n"
      "    n = 1\n"
      "    wide: i64 = 2\n"
      "    ok: bool = n < wide\n"
      "    total = n + wide\n"
      "    return void\n";
  sere::DiagnosticEngine inferDiagnostics;
  sere::SourceManager inferSource("sema_infer.sere", inferred);
  sere::Lexer inferLexer(inferSource, inferDiagnostics);
  sere::Parser inferParser(inferDiagnostics, inferLexer.tokenizeAll());
  std::unique_ptr<sere::Module> inferModule = inferParser.parseModule();
  sere::TypeContext inferTypes;
  sere::TypeChecker inferChecker(inferTypes, inferDiagnostics);
  if (inferModule == nullptr || !inferChecker.check(*inferModule)) {
    inferDiagnostics.printAll(inferSource);
    return fail("inferred locals, mixed integers, and void returns should type check");
  }

  const std::string floats =
      "class Vec2:\n"
      "    x: f64\n"
      "    y: f64\n"
      "def main() -> f64:\n"
      "    a: Vec2 = Vec2(1, 2)\n"
      "    b: Vec2 = Vec2(1.0, 2.0)\n"
      "    c: f32 = 1.0f\n"
      "    d: f64 = c\n"
      "    xs: list[f64] = [1, 2.0, 3]\n"
      "    n: f64 = 1\n"
      "    n += 0.5\n"
      "    return a.x + b.y + d + xs[0] + n\n";
  sere::DiagnosticEngine floatDiagnostics;
  sere::SourceManager floatSource("sema_float.sere", floats);
  sere::Lexer floatLexer(floatSource, floatDiagnostics);
  sere::Parser floatParser(floatDiagnostics, floatLexer.tokenizeAll());
  std::unique_ptr<sere::Module> floatModule = floatParser.parseModule();
  sere::TypeContext floatTypes;
  sere::TypeChecker floatChecker(floatTypes, floatDiagnostics);
  if (floatModule == nullptr || !floatChecker.check(*floatModule)) {
    floatDiagnostics.printAll(floatSource);
    return fail("int-to-float constructors, literals, and lists should type check");
  }

  const std::string aliases =
      "def abs(value: i32) -> i32:\n"
      "    if value < 0:\n"
      "        return -value\n"
      "    return value\n"
      "def main() -> void:\n"
      "    donut = print\n"
      "    donut(\"Hello, world!\")\n"
      "    magnitude = abs\n"
      "    print(magnitude(-4))\n"
      "    echo = donut\n"
      "    echo(\"aliased\")\n"
      "    return void\n";
  sere::DiagnosticEngine aliasDiagnostics;
  sere::SourceManager aliasSource("sema_alias.sere", aliases);
  sere::Lexer aliasLexer(aliasSource, aliasDiagnostics);
  sere::Parser aliasParser(aliasDiagnostics, aliasLexer.tokenizeAll());
  std::unique_ptr<sere::Module> aliasModule = aliasParser.parseModule();
  sere::TypeContext aliasTypes;
  sere::TypeChecker aliasChecker(aliasTypes, aliasDiagnostics);
  if (aliasModule == nullptr || !aliasChecker.check(*aliasModule)) {
    aliasDiagnostics.printAll(aliasSource);
    return fail("function and intrinsic name aliases should type check");
  }

  const std::string quotes =
      "def main() -> i32:\n"
      "    a: str = 'hi'\n"
      "    b: str = \"\"\"multi\nline\"\"\"\n"
      "    c: regex = `a+`\n"
      "    d: str = c as str\n"
      "    e: regex = d as regex\n"
      "    print(a, b, d, len(c))\n"
      "    return 0\n";
  sere::DiagnosticEngine quoteDiagnostics;
  sere::SourceManager quoteSource("sema_quotes.sere", quotes);
  sere::Lexer quoteLexer(quoteSource, quoteDiagnostics);
  sere::Parser quoteParser(quoteDiagnostics, quoteLexer.tokenizeAll());
  std::unique_ptr<sere::Module> quoteModule = quoteParser.parseModule();
  sere::TypeContext quoteTypes;
  sere::TypeChecker quoteChecker(quoteTypes, quoteDiagnostics);
  if (quoteModule == nullptr || !quoteChecker.check(*quoteModule)) {
    quoteDiagnostics.printAll(quoteSource);
    return fail("single, triple, regex, casts, and len(regex) should type check");
  }

  const std::string regexAssign =
      "def main() -> i32:\n"
      "    s: str = `a+`\n"
      "    return 0\n";
  sere::DiagnosticEngine regexDiagnostics;
  sere::SourceManager regexSource("sema_regex_assign.sere", regexAssign);
  sere::Lexer regexLexer(regexSource, regexDiagnostics);
  sere::Parser regexParser(regexDiagnostics, regexLexer.tokenizeAll());
  std::unique_ptr<sere::Module> regexModule = regexParser.parseModule();
  sere::TypeContext regexTypes;
  sere::TypeChecker regexChecker(regexTypes, regexDiagnostics);
  if (regexModule != nullptr && regexChecker.check(*regexModule)) {
    return fail("regex must not assign to str without a cast");
  }

  const std::string pointers =
      "def main() -> i32:\n"
      "    n: i32 = 10\n"
      "    p: Ptr[i32] = &n\n"
      "    *p = 20\n"
      "    owned: Unique[i32] = unique[i32](1)\n"
      "    *owned = 3\n"
      "    raw: Ptr[i32] = alloc[i32]()\n"
      "    *raw = *p\n"
      "    v: i32 = *raw + *owned\n"
      "    free(raw)\n"
      "    return v\n";
  sere::DiagnosticEngine pointerDiagnostics;
  sere::SourceManager pointerSource("sema_ptr.sere", pointers);
  sere::Lexer pointerLexer(pointerSource, pointerDiagnostics);
  sere::Parser pointerParser(pointerDiagnostics, pointerLexer.tokenizeAll());
  std::unique_ptr<sere::Module> pointerModule = pointerParser.parseModule();
  sere::TypeContext pointerTypes;
  sere::TypeChecker pointerChecker(pointerTypes, pointerDiagnostics);
  if (pointerModule == nullptr || !pointerChecker.check(*pointerModule)) {
    pointerDiagnostics.printAll(pointerSource);
    return fail("& and * pointer operators should type check");
  }

  const std::string badDeref =
      "def main() -> i32:\n"
      "    return *1\n";
  sere::DiagnosticEngine badDerefDiagnostics;
  sere::SourceManager badDerefSource("sema_bad_deref.sere", badDeref);
  sere::Lexer badDerefLexer(badDerefSource, badDerefDiagnostics);
  sere::Parser badDerefParser(badDerefDiagnostics, badDerefLexer.tokenizeAll());
  std::unique_ptr<sere::Module> badDerefModule = badDerefParser.parseModule();
  sere::TypeContext badDerefTypes;
  sere::TypeChecker badDerefChecker(badDerefTypes, badDerefDiagnostics);
  if (badDerefModule != nullptr && badDerefChecker.check(*badDerefModule)) {
    return fail("dereferencing a non-pointer must fail");
  }

  const std::string badAddr =
      "def main() -> i32:\n"
      "    p: Ptr[i32] = &1\n"
      "    return 0\n";
  sere::DiagnosticEngine badAddrDiagnostics;
  sere::SourceManager badAddrSource("sema_bad_addr.sere", badAddr);
  sere::Lexer badAddrLexer(badAddrSource, badAddrDiagnostics);
  sere::Parser badAddrParser(badAddrDiagnostics, badAddrLexer.tokenizeAll());
  std::unique_ptr<sere::Module> badAddrModule = badAddrParser.parseModule();
  sere::TypeContext badAddrTypes;
  sere::TypeChecker badAddrChecker(badAddrTypes, badAddrDiagnostics);
  if (badAddrModule != nullptr && badAddrChecker.check(*badAddrModule)) {
    return fail("taking the address of a temporary must fail");
  }

  const std::string negEnum =
      "enum ButtonRole:\n"
      "    Invalid = -1\n"
      "    Accept = 0\n"
      "    Reject = +1\n"
      "def main() -> i32:\n"
      "    role: ButtonRole = ButtonRole.Invalid\n"
      "    return role.value\n";
  sere::DiagnosticEngine enumDiagnostics;
  sere::SourceManager enumSource("sema_enum_neg.sere", negEnum);
  sere::Lexer enumLexer(enumSource, enumDiagnostics);
  sere::Parser enumParser(enumDiagnostics, enumLexer.tokenizeAll());
  std::unique_ptr<sere::Module> enumModule = enumParser.parseModule();
  sere::TypeContext enumTypes;
  sere::TypeChecker enumChecker(enumTypes, enumDiagnostics);
  if (enumModule == nullptr || !enumChecker.check(*enumModule)) {
    enumDiagnostics.printAll(enumSource);
    return fail("negative enum values should type check");
  }
  const sere::Type* roleType = enumTypes.record("ButtonRole");
  const sere::RecordField* invalid = roleType == nullptr ? nullptr : roleType->findField("Invalid");
  if (invalid == nullptr || invalid->llvmName != "-1") {
    return fail("Invalid discriminant should be -1");
  }

  const std::string badEnum =
      "enum Bad:\n"
      "    X = 1 + 2\n"
      "def main() -> i32:\n"
      "    return 0\n";
  sere::DiagnosticEngine badEnumDiagnostics;
  sere::SourceManager badEnumSource("sema_enum_bad.sere", badEnum);
  sere::Lexer badEnumLexer(badEnumSource, badEnumDiagnostics);
  sere::Parser badEnumParser(badEnumDiagnostics, badEnumLexer.tokenizeAll());
  std::unique_ptr<sere::Module> badEnumModule = badEnumParser.parseModule();
  sere::TypeContext badEnumTypes;
  sere::TypeChecker badEnumChecker(badEnumTypes, badEnumDiagnostics);
  if (badEnumModule != nullptr && badEnumChecker.check(*badEnumModule)) {
    return fail("computed enum values must still be rejected");
  }

  const std::string parseText =
      "def main() -> i32:\n"
      "    n: i32 = parse[i32](\"123\")\n"
      "    hexed: i32 = parse[i32](\"0x10\")\n"
      "    maybe: i32 | None = try_parse[i32](\"nope\")\n"
      "    flag: bool = parse[bool](\"True\")\n"
      "    return n\n";
  sere::DiagnosticEngine parseDiagnostics;
  sere::SourceManager parseSource("sema_parse.sere", parseText);
  sere::Lexer parseLexer(parseSource, parseDiagnostics);
  sere::Parser parseParser(parseDiagnostics, parseLexer.tokenizeAll());
  std::unique_ptr<sere::Module> parseModule = parseParser.parseModule();
  sere::TypeContext parseTypes;
  sere::TypeChecker parseChecker(parseTypes, parseDiagnostics);
  if (parseModule == nullptr || !parseChecker.check(*parseModule)) {
    parseDiagnostics.printAll(parseSource);
    return fail("parse[T] and try_parse[T] should typecheck");
  }

  const std::string anyNone =
      "def take(value: Any, maybe: i32 | None = None) -> i32 | None:\n"
      "    other: i32 | None = void\n"
      "    boxed: Any = 1\n"
      "    return maybe\n"
      "def main() -> void:\n"
      "    take(True)\n"
      "    take(\"hi\", 2)\n";
  sere::DiagnosticEngine anyDiagnostics;
  sere::SourceManager anySource("sema_any.sere", anyNone);
  sere::Lexer anyLexer(anySource, anyDiagnostics);
  sere::Parser anyParser(anyDiagnostics, anyLexer.tokenizeAll());
  std::unique_ptr<sere::Module> anyModule = anyParser.parseModule();
  sere::TypeContext anyTypes;
  sere::TypeChecker anyChecker(anyTypes, anyDiagnostics);
  if (anyModule == nullptr || !anyChecker.check(*anyModule)) {
    anyDiagnostics.printAll(anySource);
    return fail("Any and i32 | None = None should typecheck");
  }

#if defined(_WIN32)
  const std::string platformFold =
      "def main() -> i32:\n"
      "    if __linux__:\n"
      "        no_such_linux_only()\n"
      "    if not __windows__:\n"
      "        also_missing()\n"
      "    if __windows__:\n"
      "        return 1\n"
      "    return 0\n";
  sere::DiagnosticEngine platformDiagnostics;
  sere::SourceManager platformSource("sema_platform.sere", platformFold);
  sere::Lexer platformLexer(platformSource, platformDiagnostics);
  sere::Parser platformParser(platformDiagnostics, platformLexer.tokenizeAll());
  std::unique_ptr<sere::Module> platformModule = platformParser.parseModule();
  sere::TypeContext platformTypes;
  sere::TypeChecker platformChecker(platformTypes, platformDiagnostics);
  if (platformModule == nullptr || !platformChecker.check(*platformModule)) {
    platformDiagnostics.printAll(platformSource);
    return fail("false compile-time platform branches must be skipped");
  }
#endif

  const std::string pythonish =
      "def greet(name):\n"
      "    print(name)\n"
      "def main() -> i32:\n"
      "    xs = [1, 2, 3]\n"
      "    xs.append(4)\n"
      "    greet(\"sere\")\n"
      "    a, b = 1, 2\n"
      "    pair = (3, 4)\n"
      "    c, d = pair\n"
      "    add1 = lambda (x: i32) -> i32: x + 1\n"
      "    n = add1(1)\n"
      "    if (m := n) > 0:\n"
      "        n = m\n"
      "    const limit = 4\n"
      "    n //= 1\n"
      "    return n + a + b + c + d - limit\n";
  sere::DiagnosticEngine pyDiagnostics;
  sere::SourceManager pySource("sema_pythonish.sere", pythonish);
  sere::Lexer pyLexer(pySource, pyDiagnostics);
  sere::Parser pyParser(pyDiagnostics, pyLexer.tokenizeAll());
  std::unique_ptr<sere::Module> pyModule = pyParser.parseModule();
  sere::TypeContext pyTypes;
  sere::TypeChecker pyChecker(pyTypes, pyDiagnostics);
  if (pyModule == nullptr || !pyChecker.check(*pyModule)) {
    pyDiagnostics.printAll(pySource);
    return fail("python-superset program should typecheck");
  }

  const std::string badDel =
      "def main() -> i32:\n"
      "    n: i32 = 1\n"
      "    del n\n"
      "    return 0\n";
  sere::DiagnosticEngine delDiagnostics;
  sere::SourceManager delSource("sema_del.sere", badDel);
  sere::Lexer delLexer(delSource, delDiagnostics);
  sere::Parser delParser(delDiagnostics, delLexer.tokenizeAll());
  std::unique_ptr<sere::Module> delModule = delParser.parseModule();
  sere::TypeContext delTypes;
  sere::TypeChecker delChecker(delTypes, delDiagnostics);
  if (delModule != nullptr && delChecker.check(*delModule)) {
    return fail("del of a name must be rejected");
  }

  const std::string chars =
      "def main() -> i32:\n"
      "    c: i8 = 'A'\n"
      "    b: byte = '\\n'\n"
      "    n: i32 = c\n"
      "    s: str = \"A\"\n"
      "    t: str = 'hi'\n"
      "    return n\n";
  sere::DiagnosticEngine charDiagnostics;
  sere::SourceManager charSource("sema_char.sere", chars);
  sere::Lexer charLexer(charSource, charDiagnostics);
  sere::Parser charParser(charDiagnostics, charLexer.tokenizeAll());
  std::unique_ptr<sere::Module> charModule = charParser.parseModule();
  sere::TypeContext charTypes;
  sere::TypeChecker charChecker(charTypes, charDiagnostics);
  if (charModule == nullptr || !charChecker.check(*charModule)) {
    charDiagnostics.printAll(charSource);
    return fail("single-quoted one-char literals should type as i8/byte");
  }
  return 0;
}
