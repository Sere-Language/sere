/// @file import_private.cpp
/// @private names are not exported from imported modules or .slib files.

#include "sere/driver/Frontend.h"
#include "sere/lsp/ImportCompletion.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << "import_private: " << message << '\n';
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

[[nodiscard]] bool hasMessage(const sere::Frontend& frontend, std::string_view needle) {
  for (const sere::Diagnostic& diagnostic : frontend.diagnostics().diagnostics()) {
    if (diagnostic.message.find(std::string(needle)) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  const std::filesystem::path temp =
      std::filesystem::temp_directory_path() / "sere-import-private-test";
  std::error_code fsError;
  std::filesystem::remove_all(temp, fsError);
  const std::filesystem::path stdlib = temp / "stdlib";
  if (!writeAll(stdlib / "prelude.sere", "def _prelude() -> i32:\n    return 0\n") ||
      !writeAll(temp / "mod.sere",
                "@private def hidden() -> i32:\n"
                "    return 1\n"
                "@public def shown() -> i32:\n"
                "    return hidden()\n"
                "@private class Secret:\n"
                "    value: i32\n"
                "class Visible:\n"
                "    def __init__(self) -> void:\n"
                "        pass\n"
                "    @private def sneak(self) -> i32:\n"
                "        return 2\n"
                "    def ok(self) -> i32:\n"
                "        return self.sneak()\n")) {
    return fail("could not write fixtures");
  }

  const std::string okText =
      "from mod import shown, Visible\n\n"
      "def main() -> i32:\n"
      "    v: Visible = Visible()\n"
      "    return shown() + v.ok()\n";
  const std::string badText = "import mod\n\ndef main() -> i32:\n    return mod.hidden()\n";
  const std::string fromText = "from mod import hidden\n\ndef main() -> i32:\n    return hidden()\n";
  const std::string methodText =
      "from mod import Visible\n\n"
      "def main() -> i32:\n"
      "    v: Visible = Visible()\n"
      "    return v.sneak()\n";
  if (!writeAll(temp / "ok.sere", okText) || !writeAll(temp / "bad.sere", badText) ||
      !writeAll(temp / "from.sere", fromText) || !writeAll(temp / "method.sere", methodText)) {
    std::filesystem::remove_all(temp, fsError);
    return fail("could not write user programs");
  }

  sere::Frontend allowed;
  if (!allowed.analyze((temp / "ok.sere").string(), okText, stdlib)) {
    allowed.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("public exports should import and typecheck");
  }

  sere::Frontend blocked;
  if (blocked.analyze((temp / "bad.sere").string(), badText, stdlib) ||
      !hasMessage(blocked, "is private")) {
    blocked.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("mod.hidden should be a PermissionError");
  }

  sere::Frontend fromImport;
  if (fromImport.analyze((temp / "from.sere").string(), fromText, stdlib) ||
      !hasMessage(fromImport, "is private")) {
    fromImport.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("from mod import hidden should be rejected");
  }

  sere::Frontend method;
  if (method.analyze((temp / "method.sere").string(), methodText, stdlib) ||
      !hasMessage(method, "is private")) {
    method.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("private class method should not be callable from the importer");
  }

  const std::vector<sere::ImportCompletionItem> exports =
      sere::importExportCompletions(temp / "mod.sere", "");
  for (const sere::ImportCompletionItem& item : exports) {
    if (item.label == "hidden" || item.label == "Secret") {
      std::filesystem::remove_all(temp, fsError);
      return fail("from-import completion must omit @private names");
    }
  }
  bool hasShown = false;
  for (const sere::ImportCompletionItem& item : exports) {
    hasShown = hasShown || item.label == "shown" || item.label == "Visible";
  }
  if (!hasShown) {
    std::filesystem::remove_all(temp, fsError);
    return fail("from-import completion should still list public names");
  }

  std::filesystem::remove_all(temp, fsError);
  return 0;
}
