/// @file generic_constraints.cpp
/// Checks generic constraints through the shared frontend, including imports.

#include "sere/driver/Frontend.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Case {
  std::string name;
  std::string source;
  std::string error;
};

} // namespace

int main() {
  const auto root = std::filesystem::current_path() / "generic_constraints_fixtures";
  std::filesystem::create_directories(root / "stdlib");
  std::ofstream(root / "stdlib" / "prelude.sere") << "";
  const std::string identity = "def identity[T: i32 | f64](value: T) -> T:\n    return value\n";
  const std::string box = "class Box[T: i32 | str]:\n    value: T\n";
  const std::string method = "class Tools:\n"
                             "    def identity[T: i32 | str](self, value: T) -> T:\n"
                             "        return value\n";
  const std::string enumeration = "enum Value[T: i32 | str]:\n    Item(T)\n";
  std::ofstream(root / "restricted.sere") << identity << box << method << enumeration;

  const std::vector<Case> cases = {
      {"explicit", identity + "def main() -> i32:\n    return identity[i32](3)\n", ""},
      {"inferred",
       identity + "def main() -> i32:\n    value: f64 = identity(2.5)\n    return 0\n",
       ""},
      {"explicit rejected",
       identity + "def main() -> i32:\n    identity[str](\"bad\")\n    return 0\n",
       "must be one of"},
      {"inferred rejected",
       identity + "def main() -> i32:\n    identity(\"bad\")\n    return 0\n",
       "must be one of"},
      {"exact widths",
       identity + "def main() -> i32:\n    value: i64 = 3\n    identity(value)\n    return 0\n",
       "must be one of"},
      {"Any rejected",
       identity + "def main() -> i32:\n    value: Any = 3\n    identity(value)\n    return 0\n",
       "must be one of"},
      {"class",
       box + "def main() -> i32:\n    value: Box[i32] = Box[i32](3)\n    return value.value\n",
       ""},
      {"class rejected",
       box + "def main() -> i32:\n    value: Box[f64] = Box[f64](2.5)\n    return 0\n",
       "must be one of"},
      {"bare constrained annotation",
       box + "def use(value: Box) -> void:\n    pass\n",
       "must be one of"},
      {"struct",
       "struct Box[T: i32 | str]:\n    value: T\ndef main() -> i32:\n    value = Box[i32](3)\n    "
       "return value.value\n",
       ""},
      {"method",
       method + "def main() -> i32:\n    tools = Tools()\n    return tools.identity(3)\n",
       ""},
      {"method rejected",
       method +
           "def main() -> i32:\n    tools = Tools()\n    tools.identity[f64](2.5)\n    return 0\n",
       "must be one of"},
      {"enum inferred",
       enumeration + "def main() -> i32:\n    value = Value.Item(3)\n    return 0\n",
       ""},
      {"enum inferred rejected",
       enumeration + "def main() -> i32:\n    value = Value.Item(2.5)\n    return 0\n",
       "must be one of"},
      {"enum explicit rejected",
       enumeration + "def main() -> i32:\n    value = Value.Item[f64](2.5)\n    return 0\n",
       "must be one of"},
      {"independent parameter scopes",
       identity + "def text[T: str](value: T) -> T:\n    return value\n"
                  "def unrestricted[T](value: T) -> T:\n    return value\n"
                  "def main() -> i32:\n    text(\"yes\")\n    unrestricted(True)\n    return identity(3)\n",
       ""},
      {"alias constraint",
       "type Number = i32 | f64\ndef identity[T: Number](value: T) -> T:\n    return value\ndef "
       "main() -> i32:\n    return identity(3)\n",
       ""},
      {"collection constraint",
       "def identity[T: list[i32] | str](value: T) -> T:\n    return value\ndef main() -> i32:\n   "
       " value: list[i32] = identity([1, 2])\n    return value[0]\n",
       ""},
      {"unknown constraint",
       "def identity[T: Missing](value: T) -> T:\n    return value\n",
       "unknown type"},
      {"nonconcrete constraint",
       "def identity[T: Any](value: T) -> T:\n    return value\n",
       "must name concrete types"},
      {"duplicate parameter",
       "def identity[T: i32, T: str](value: T) -> T:\n    return value\n",
       "duplicate type parameter"},
      {"missing constraint",
       "def identity[T:](value: T) -> T:\n    return value\n",
       "expected type name"},
      {"imported function",
       "import restricted\ndef main() -> i32:\n    return restricted.identity(3)\n",
       ""},
      {"qualified function rejected",
       "import restricted\ndef main() -> i32:\n    restricted.identity(\"bad\")\n    return 0\n",
       "must be one of"},
      {"qualified class",
       "import restricted\ndef main() -> i32:\n    value = restricted.Box[i32](3)\n    return "
       "value.value\n",
       ""},
      {"imported function rejected",
       "from restricted import identity\ndef main() -> i32:\n    identity(\"bad\")\n    return 0\n",
       "must be one of"},
      {"imported class rejected",
       "import restricted\ndef main() -> i32:\n    value: restricted.Box[f64] = "
       "restricted.Box[f64](2.5)\n    return 0\n",
       "must be one of"},
      {"imported method rejected",
       "from restricted import Tools\ndef main() -> i32:\n    tools = Tools()\n    "
       "tools.identity(2.5)\n    return 0\n",
       "must be one of"},
      {"enum alias constraint", "type Text = str\nenum Value[T: Text | i32]:\n    Item(T)\ndef main() -> i32:\n    value = Value.Item(\"yes\")\n    return 0\n", ""},
      {"alias of invalid record", box + "type Bad = Box[f64]\n", "must be one of"},
      {"invalid unused class", "class Box[T: Missing]:\n    value: T\n", "unknown type"},
      {"constrained record parameter", box + "def use[T: i32 | str](value: Box[T]) -> void:\n    pass\n", ""},
      {"unconstrained record parameter rejected", box + "def use[T](value: Box[T]) -> void:\n    pass\n", "must be one of"},
      {"broad record parameter rejected", box + "def use[T: i32 | f64](value: Box[T]) -> void:\n    pass\n", "must be one of"},
      {"struct rejected", "struct Box[T: i32 | str]:\n    value: T\ndef main() -> i32:\n    value = Box[f64](2.5)\n    return 0\n", "must be one of"},
      {"imported enum rejected", "from restricted import Value\ndef main() -> i32:\n    value = Value.Item(2.5)\n    return 0\n", "must be one of"},
      {"enum alias payload preserved", enumeration + "type IntegerValue = Value[i32]\ndef main() -> i32:\n    value: IntegerValue = Value.Item(3)\n    return 0\n", ""},
  };
  bool allPassed = true;
  for (const Case& test : cases) {
    std::ofstream(root / "main.sere") << test.source;
    sere::Frontend frontend;
    const bool ok = frontend.analyze((root / "main.sere").string(), test.source, root / "stdlib");
    bool found = test.error.empty();
    for (const auto& diagnostic : frontend.diagnostics().diagnostics()) {
      found = found || diagnostic.message.find(test.error) != std::string::npos;
    }
    if (ok != test.error.empty() || !found) {
      std::cerr << "generic_constraints: " << test.name << '\n';
      frontend.diagnostics().printAll();
      allPassed = false;
    }
  }
  return allPassed ? 0 : 1;
}
