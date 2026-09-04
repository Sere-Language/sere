/// @file sema_shadow_types.cpp
/// Local classes do not collide with imported names; isinstance, sized lists,
/// properties, and __exports__ type-check.

#include "sere/driver/Frontend.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/sema/TypeChecker.h"
#include "sere/source/SourceManager.h"
#include "sere/types/TypeContext.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int fail(const char* message) {
  std::cerr << "sema_shadow_types: " << message << '\n';
  return 1;
}

[[nodiscard]] bool writeAll(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  output << text;
  return static_cast<bool>(output);
}

}  // namespace

int main() {
  const std::string local =
      "class Vec2:\n"
      "    @private x: i32\n"
      "    def __init__(self, x: i32) -> void:\n"
      "        self.x = x\n"
      "    @public x.get:\n"
      "        return self.x\n"
      "    @public x.set(self, value: i32) -> void:\n"
      "        self.x = value\n"
      "class Student:\n"
      "    name: str\n"
      "    def __init__(self, name: str) -> void:\n"
      "        self.name = name\n"
      "def main() -> i32:\n"
      "    v: Vec2 = Vec2(1)\n"
      "    v.x = 4\n"
      "    xs: list[i32] = [1, 2, 3]\n"
      "    room: list[Student, 2] = [Student(\"a\"), Student(\"b\")]\n"
      "    ok: bool = isinstance(xs, list[i32])\n"
      "    also: bool = isinstance(v, Vec2)\n"
      "    same: bool = typeof(v) is Vec2\n"
      "    return v.x\n";
  sere::DiagnosticEngine localDiag;
  sere::SourceManager localSource("sema_local.sere", local);
  sere::Lexer localLexer(localSource, localDiag);
  sere::Parser localParser(localDiag, localLexer.tokenizeAll());
  std::unique_ptr<sere::Module> localModule = localParser.parseModule();
  if (localModule == nullptr || localDiag.hasErrors()) {
    localDiag.printAll(localSource);
    return fail("local properties, sized lists, and isinstance should parse");
  }
  sere::TypeContext localTypes;
  sere::TypeChecker localChecker(localTypes, localDiag);
  if (!localChecker.check(*localModule)) {
    localDiag.printAll(localSource);
    return fail("local properties, sized lists, and isinstance should typecheck");
  }

  const std::string statics =
      "class Counter:\n"
      "    @public static total: i32 = 0\n"
      "    def __init__(self) -> void:\n"
      "        Counter.total = Counter.total + 1\n"
      "        self.total = self.total + 0\n"
      "class Other:\n"
      "    @static\n"
      "    count: i32 = 2\n"
      "def main() -> i32:\n"
      "    a: Counter = Counter()\n"
      "    b: Counter = Counter()\n"
      "    return Counter.total + Other.count + a.total\n";
  sere::DiagnosticEngine staticDiag;
  sere::SourceManager staticSource("sema_static.sere", statics);
  sere::Lexer staticLexer(staticSource, staticDiag);
  sere::Parser staticParser(staticDiag, staticLexer.tokenizeAll());
  std::unique_ptr<sere::Module> staticModule = staticParser.parseModule();
  if (staticModule == nullptr || staticDiag.hasErrors()) {
    staticDiag.printAll(staticSource);
    return fail("class static fields should parse");
  }
  sere::TypeContext staticTypes;
  sere::TypeChecker staticChecker(staticTypes, staticDiag);
  if (!staticChecker.check(*staticModule)) {
    staticDiag.printAll(staticSource);
    return fail("class static fields should typecheck");
  }

  const std::filesystem::path temp =
      std::filesystem::temp_directory_path() / "sere-shadow-types-test";
  std::error_code fsError;
  std::filesystem::remove_all(temp, fsError);
  const std::filesystem::path stdlib = temp / "stdlib";
  if (!writeAll(stdlib / "prelude.sere", "def _prelude() -> i32:\n    return 0\n") ||
      !writeAll(stdlib / "gl.sere",
                "class Window:\n"
                "    w: i32\n"
                "    def __init__(self, w: i32) -> void:\n"
                "        self.w = w\n"
                "    def width(self) -> i32:\n"
                "        return self.w\n") ||
      !writeAll(temp / "lib.sere",
                "class Greeter:\n"
                "    def hello(self) -> i32:\n"
                "        return 7\n"
                "def version() -> i32:\n"
                "    return 1\n"
                "from Greeter import hello\n"
                "__exports__ += [hello]\n") ||
      !writeAll(temp / "user.sere",
                "import gl\n"
                "import lib\n"
                "\n"
                "class Window:\n"
                "    title: str\n"
                "    def __init__(self, title: str) -> void:\n"
                "        self.title = title\n"
                "    def label(self) -> str:\n"
                "        return self.title\n"
                "\n"
                "def main() -> i32:\n"
                "    local: Window = Window(\"ok\")\n"
                "    remote: gl.Window = gl.Window(8)\n"
                "    hit: bool = isinstance(remote, gl.Window)\n"
                "    miss: bool = typeof(local) is gl.Window\n"
                "    return remote.width() + lib.version() + lib.hello(Greeter())\n")) {
    return fail("could not write fixtures");
  }

  sere::Frontend frontend;
  if (!frontend.analyze((temp / "user.sere").string(),
                        "import gl\n"
                        "import lib\n"
                        "\n"
                        "class Window:\n"
                        "    title: str\n"
                        "    def __init__(self, title: str) -> void:\n"
                        "        self.title = title\n"
                        "    def label(self) -> str:\n"
                        "        return self.title\n"
                        "\n"
                        "def main() -> i32:\n"
                        "    local: Window = Window(\"ok\")\n"
                        "    remote: gl.Window = gl.Window(8)\n"
                        "    hit: bool = isinstance(remote, gl.Window)\n"
                        "    miss: bool = typeof(local) is gl.Window\n"
                        "    return remote.width() + lib.version() + lib.hello(lib.Greeter())\n",
                        stdlib)) {
    frontend.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("local Window must not collide with gl.Window");
  }

  sere::Frontend fieldFrontend;
  if (!fieldFrontend.analyze((temp / "holder.sere").string(),
                             "import gl\n"
                             "\n"
                             "class Holder:\n"
                             "    window: gl.Window\n"
                             "    def __init__(self) -> void:\n"
                             "        self.window = gl.Window(4)\n"
                             "    def width(self) -> i32:\n"
                             "        return self.window.width()\n"
                             "\n"
                             "def main() -> i32:\n"
                             "    return Holder().width()\n",
                             stdlib)) {
    fieldFrontend.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("class fields must accept gl.Window");
  }

  sere::Frontend leakFrontend;
  if (leakFrontend.analyze((temp / "leak.sere").string(),
                           "import gl\n"
                           "\n"
                           "def main() -> i32:\n"
                           "    remote: Window = Window(8)\n"
                           "    return remote.width()\n",
                           stdlib)) {
    std::filesystem::remove_all(temp, fsError);
    return fail("import gl must not bind unqualified Window");
  }

  sere::Frontend ctorFrontend;
  if (!ctorFrontend.analyze((temp / "ctors.sere").string(),
                            "import gl\n"
                            "\n"
                            "class Window:\n"
                            "    title: str\n"
                            "    def __init__(self, title: str) -> void:\n"
                            "        self.title = title\n"
                            "\n"
                            "def main() -> i32:\n"
                            "    local: Window = Window(\"ok\")\n"
                            "    remote: gl.Window = gl.Window(8)\n"
                            "    return remote.width()\n",
                            stdlib)) {
    ctorFrontend.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("gl.Window(8) must not use the local Window constructor");
  }

  sere::Frontend mixedFrontend;
  if (mixedFrontend.analyze((temp / "mixed.sere").string(),
                            "import gl\n"
                            "\n"
                            "class Window:\n"
                            "    title: str\n"
                            "    def __init__(self, title: str) -> void:\n"
                            "        self.title = title\n"
                            "\n"
                            "def main() -> i32:\n"
                            "    remote: gl.Window = gl.Window(\"no\")\n"
                            "    return 0\n",
                            stdlib)) {
    std::filesystem::remove_all(temp, fsError);
    return fail("gl.Window must reject the local Window constructor arguments");
  }

  sere::Frontend importOnly;
  if (!importOnly.analyze((temp / "import_only.sere").string(),
                          "import gl\n"
                          "\n"
                          "def main() -> i32:\n"
                          "    return 0\n",
                          stdlib)) {
    importOnly.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("import gl should typecheck on its own");
  }
  if (importOnly.checker() != nullptr && importOnly.checker()->typeOfName("Window") != nullptr) {
    std::filesystem::remove_all(temp, fsError);
    return fail("import gl must not inject Window into the importer's scope");
  }

  if (!writeAll(temp / "widget.sere",
                "class Application:\n"
                "    def __init__(self) -> void:\n"
                "        pass\n"
                "    def id(self) -> i32:\n"
                "        return 3\n"
                "class Window:\n"
                "    def size(self) -> i32:\n"
                "        return 5\n") ||
      !writeAll(temp / "barrel.sere",
                "from widget import Application, Window as BaseWindow\n"
                "\n"
                "def version() -> i32:\n"
                "    return 1\n"
                "\n"
                "__exports__ += [\n"
                "    Application,\n"
                "    BaseWindow\n"
                "]\n") ||
      !writeAll(temp / "app.sere",
                "import barrel as ui\n"
                "\n"
                "def main() -> i32:\n"
                "    return ui.Application().id() + ui.BaseWindow().size() + ui.version()\n") ||
      !writeAll(temp / "child.sere",
                "import widget\n"
                "\n"
                "class Main(widget.Application):\n"
                "    def __init__(self) -> void:\n"
                "        super().__init__()\n"
                "    def id(self) -> i32:\n"
                "        return super().id() + 1\n"
                "\n"
                "def main() -> i32:\n"
                "    return Main().id()\n")) {
    std::filesystem::remove_all(temp, fsError);
    return fail("could not write barrel fixtures");
  }

  sere::Frontend barrelFrontend;
  if (!barrelFrontend.analyze((temp / "app.sere").string(),
                              "import barrel as ui\n"
                              "\n"
                              "def main() -> i32:\n"
                              "    return ui.Application().id() + ui.BaseWindow().size() + "
                              "ui.version()\n",
                              stdlib)) {
    barrelFrontend.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("__exports__ must re-export from-imported names");
  }

  sere::Frontend inheritFrontend;
  if (!inheritFrontend.analyze((temp / "child.sere").string(),
                               "import widget\n"
                               "\n"
                               "class Main(widget.Application):\n"
                               "    def __init__(self) -> void:\n"
                               "        super().__init__()\n"
                               "    def id(self) -> i32:\n"
                               "        return super().id() + 1\n"
                               "\n"
                               "def main() -> i32:\n"
                               "    return Main().id()\n",
                               stdlib)) {
    inheritFrontend.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("class Main(widget.Application) must typecheck like an imported Application base");
  }

  sere::Frontend kwargsFrontend;
  if (!kwargsFrontend.analyze((temp / "kwargs.sere").string(),
                              "class Base:\n"
                              "    def __init__(self, x: i32, **extra: dict[str, i32]) -> void:\n"
                              "        pass\n"
                              "class Child(Base):\n"
                              "    def __init__(self, x: i32, **extra: dict[str, i32]) -> void:\n"
                              "        super().__init__(x, **extra)\n"
                              "def main() -> i32:\n"
                              "    return 0\n",
                              stdlib)) {
    kwargsFrontend.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("super().__init__(x, **extra) must parse and typecheck");
  }

  std::filesystem::remove_all(temp, fsError);
  return 0;
}
