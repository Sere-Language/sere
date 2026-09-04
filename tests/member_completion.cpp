/// @file member_completion.cpp
/// Member autocomplete for local classes, sibling modules, and stdlib imports.

#include "sere/driver/Frontend.h"
#include "sere/lsp/MemberCompletion.h"
#include "sere/types/Type.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef SERE_STDLIB_DIR
#define SERE_STDLIB_DIR ""
#endif

namespace {

int fail(const char* message) {
  std::cerr << "member_completion: " << message << '\n';
  return 1;
}

[[nodiscard]] std::filesystem::path stdlibDir() {
  if (const char* fromEnv = std::getenv("SERE_STDLIB"); fromEnv != nullptr && fromEnv[0] != '\0') {
    return fromEnv;
  }
  return SERE_STDLIB_DIR;
}

void writeFile(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << text;
}

[[nodiscard]] bool hasLabel(const std::vector<sere::MemberCompletionItem>& items,
                            std::string_view label) {
  for (const sere::MemberCompletionItem& item : items) {
    if (item.label == label) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] const sere::MemberCompletionItem*
findItem(const std::vector<sere::MemberCompletionItem>& items, std::string_view label) {
  for (const sere::MemberCompletionItem& item : items) {
    if (item.label == label) {
      return &item;
    }
  }
  return nullptr;
}

int testDetect() {
  const sere::MemberAccessQuery afterDot = sere::detectMemberAccessLine("    foo.", 9);
  if (!afterDot.active || afterDot.receiver != std::vector<std::string>{"foo"} ||
      !afterDot.prefix.empty()) {
    return fail("foo. should be a member access on foo");
  }
  const sere::MemberAccessQuery prefix = sere::detectMemberAccessLine("    foo.ba", 11);
  if (!prefix.active || prefix.receiver != std::vector<std::string>{"foo"} ||
      prefix.prefix != "ba") {
    return fail("foo.ba should keep prefix ba");
  }
  const sere::MemberAccessQuery dotted = sere::detectMemberAccessLine("    gl.Window.", 15);
  if (!dotted.active || dotted.receiver != std::vector<std::string>{"gl", "Window"}) {
    return fail("gl.Window. should walk the dotted receiver");
  }
  const sere::MemberAccessQuery stringLiteral =
      sere::detectMemberAccessLine("    \"hello\".up", 14);
  if (!stringLiteral.active || stringLiteral.receiverType != "str" ||
      stringLiteral.prefix != "up") {
    return fail("string literals should retain their type and member prefix");
  }
  const sere::MemberAccessQuery none = sere::detectMemberAccessLine("    foo = Foo(0)", 8);
  if (none.active) {
    return fail("assignment should not look like member access");
  }
  return 0;
}

int testLocalClass() {
  const std::string text = "class Foo:\n"
                           "    x: i32\n"
                           "    def __init__(self, x: i32) -> void:\n"
                           "        self.x = x\n"
                           "    def bar(self) -> i32:\n"
                           "        return self.x\n"
                           "def main() -> i32:\n"
                           "    foo = Foo(0)\n"
                           "    return foo.bar()\n";
  sere::Frontend frontend;
  (void)frontend.analyze("local_class.sere", text, stdlibDir());
  if (frontend.checker() == nullptr) {
    frontend.diagnostics().printAll();
    return fail("local class sample should produce a type checker");
  }
  const sere::MemberAccessQuery query = sere::detectMemberAccessLine("    foo.", 9);
  const sere::Type* type = sere::resolveMemberType(&frontend, query);
  const std::vector<sere::MemberCompletionItem> items = sere::collectMemberCompletions(type);
  if (!hasLabel(items, "bar") || !hasLabel(items, "x")) {
    return fail("foo. should complete bar and x");
  }
  if (hasLabel(items, "__init__")) {
    return fail("dunder methods should stay hidden");
  }
  const sere::MemberAccessQuery literalQuery = sere::detectMemberAccessLine("    \"hello\".", 12);
  const std::vector<sere::MemberCompletionItem> stringItems =
      sere::collectMemberCompletions(sere::resolveMemberType(&frontend, literalQuery));
  const sere::MemberCompletionItem* upper = findItem(stringItems, "upper");
  if (upper == nullptr || upper->detail != "upper() -> str" || upper->insertText != "upper()") {
    return fail("string literal completion should offer upper() with a callable snippet");
  }
  return 0;
}

int testLexicalScopes() {
  const std::string text = "def first(alpha: i32) -> i32:\n"
                           "    local_first = alpha\n"
                           "    return local_first\n"
                           "def second(beta: str) -> str:\n"
                           "    local_second = beta\n"
                           "    return local_second\n";
  sere::Frontend frontend;
  if (!frontend.analyze("completion_scopes.sere", text, stdlibDir()) ||
      frontend.checker() == nullptr) {
    frontend.diagnostics().printAll();
    return fail("scope completion sample should typecheck");
  }
  const std::uint32_t firstCursor = static_cast<std::uint32_t>(text.find("return local_first"));
  const std::uint32_t secondCursor = static_cast<std::uint32_t>(text.find("return local_second"));
  auto visible = [](const std::vector<const sere::SemanticSymbol*>& symbols,
                    std::string_view name) {
    for (const sere::SemanticSymbol* symbol : symbols) {
      if (symbol != nullptr && symbol->name == name) {
        return true;
      }
    }
    return false;
  };
  const auto first = frontend.checker()->visibleSymbolsAt(firstCursor);
  const auto second = frontend.checker()->visibleSymbolsAt(secondCursor);
  if (!visible(first, "alpha") || !visible(first, "local_first") || visible(first, "beta") ||
      visible(first, "local_second")) {
    return fail("first() completions should contain only first() locals");
  }
  if (!visible(second, "beta") || !visible(second, "local_second") || visible(second, "alpha") ||
      visible(second, "local_first")) {
    return fail("second() completions should contain only second() locals");
  }
  return 0;
}

int testSiblingImport() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "sere-member-completion-test";
  std::filesystem::remove_all(root);
  writeFile(root / "util.sere", "def double(value: i32) -> i32:\n    return value + value\n");
  writeFile(root / "app.sere", "import util\n\ndef main() -> i32:\n    return util.double(2)\n");
  sere::Frontend frontend;
  const std::string text = "import util\n"
                           "\n"
                           "def main() -> i32:\n"
                           "    return util.double(2)\n";
  if (!frontend.analyze((root / "app.sere").string(), text, stdlibDir())) {
    frontend.diagnostics().printAll();
    std::filesystem::remove_all(root);
    return fail("sibling import util should typecheck");
  }
  const sere::MemberAccessQuery query = sere::detectMemberAccessLine("    return util.", 16);
  const sere::Type* type = sere::resolveMemberType(&frontend, query);
  const std::vector<sere::MemberCompletionItem> items = sere::collectMemberCompletions(type);
  std::filesystem::remove_all(root);
  if (!hasLabel(items, "double")) {
    return fail("util. should complete double");
  }
  return 0;
}

int testStdlibImport() {
  const std::string text = "import math\n"
                           "\n"
                           "def main() -> i32:\n"
                           "    value = math.floor(1.5)\n"
                           "    return 0\n";
  sere::Frontend frontend;
  if (!frontend.analyze("stdlib_import.sere", text, stdlibDir())) {
    frontend.diagnostics().printAll();
    return fail("import math should typecheck against stdlib");
  }
  const sere::MemberAccessQuery query = sere::detectMemberAccessLine("    return math.", 16);
  const sere::Type* type = sere::resolveMemberType(&frontend, query);
  const std::vector<sere::MemberCompletionItem> items = sere::collectMemberCompletions(type);
  if (!hasLabel(items, "floor")) {
    return fail("math. should complete floor");
  }
  return 0;
}

int testLocalWindowVsGl() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "sere-window-shadow-completion";
  std::filesystem::remove_all(root);
  const std::filesystem::path stdlib = root / "stdlib";
  writeFile(stdlib / "prelude.sere", "def _prelude() -> i32:\n    return 0\n");
  writeFile(stdlib / "gl.sere",
            "class Window:\n"
            "    w: i32\n"
            "    def __init__(self, w: i32) -> void:\n"
            "        self.w = w\n"
            "    def make_current(self) -> void:\n"
            "        pass\n");
  const std::string text = "import gl\n"
                           "\n"
                           "class Window:\n"
                           "    title: str\n"
                           "    def __init__(self, title: str) -> void:\n"
                           "        self.title = title\n"
                           "    def label(self) -> str:\n"
                           "        return self.title\n"
                           "    def close(self) -> void:\n"
                           "        pass\n"
                           "    def poll(self) -> void:\n"
                           "        pass\n"
                           "    def tick(self) -> void:\n"
                           "        pass\n"
                           "\n"
                           "def main() -> i32:\n"
                           "    local = Window(\"ok\")\n"
                           "    remote = gl.Window(8)\n"
                           "    return 0\n";
  writeFile(root / "app.sere", text);
  sere::Frontend frontend;
  if (!frontend.analyze((root / "app.sere").string(), text, stdlib)) {
    frontend.diagnostics().printAll();
    std::filesystem::remove_all(root);
    return fail("local Window and gl.Window should typecheck together");
  }
  const sere::Type* selfType = nullptr;
  for (const sere::SemanticSymbol& symbol : frontend.checker()->symbols()) {
    if (symbol.name == "self" && symbol.kind == "variable") {
      selfType = symbol.type;
    }
  }
  const sere::Type* localWindow = frontend.checker()->typeOfName("Window");
  const sere::Type* remoteWindow = frontend.checker()->typeOfPath({"gl", "Window"});
  if (remoteWindow != nullptr && remoteWindow->isTypeObject()) {
    remoteWindow = remoteWindow->typeObjectInstance();
  }
  if (localWindow == nullptr || remoteWindow == nullptr ||
      localWindow->canonical() == remoteWindow->canonical()) {
    std::filesystem::remove_all(root);
    return fail("unqualified Window must be the local class");
  }
  if (remoteWindow->display().find("gl.") == std::string::npos) {
    std::filesystem::remove_all(root);
    return fail("gl.Window must display as a module-qualified type");
  }
  if (selfType != nullptr && selfType->canonical() != localWindow->canonical()) {
    std::filesystem::remove_all(root);
    return fail("self must have the local Window type");
  }
  const sere::MemberAccessQuery query = sere::detectMemberAccessLine("        self.", 13);
  const sere::Type* type = sere::resolveMemberType(&frontend, query);
  const std::vector<sere::MemberCompletionItem> items = sere::collectMemberCompletions(type);
  std::filesystem::remove_all(root);
  if (!hasLabel(items, "label") || hasLabel(items, "make_current")) {
    return fail("self. must complete local Window methods, not gl.Window");
  }
  return 0;
}

} // namespace

int main() {
  if (const int status = testDetect(); status != 0) {
    return status;
  }
  if (const int status = testLocalClass(); status != 0) {
    return status;
  }
  if (const int status = testLexicalScopes(); status != 0) {
    return status;
  }
  if (const int status = testSiblingImport(); status != 0) {
    return status;
  }
  if (const int status = testStdlibImport(); status != 0) {
    return status;
  }
  return testLocalWindowVsGl();
}
