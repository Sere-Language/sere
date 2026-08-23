/// @file lsp_context.cpp
/// Stdlib overlays and language-context stamps refresh analysis without a restart.

#include "sere/driver/Frontend.h"
#include "sere/driver/Project.h"
#include "sere/driver/SourceOverlay.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int fail(const char* message) {
  std::cerr << "lsp_context: " << message << '\n';
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

[[nodiscard]] bool hasSymbol(const sere::Frontend& frontend, std::string_view name) {
  if (frontend.checker() == nullptr) {
    return false;
  }
  for (const sere::SemanticSymbol& symbol : frontend.checker()->symbols()) {
    if (symbol.name == name) {
      return true;
    }
  }
  return false;
}

int checkPreludeOverlay(const std::filesystem::path& root) {
  const std::filesystem::path stdlib = root / "stdlib";
  if (!writeAll(stdlib / "prelude.sere", "def disk_only() -> i32:\n    return 1\n") ||
      !writeAll(root / "app.sere", "def main() -> i32:\n    return live()\n")) {
    return fail("could not write prelude overlay fixtures");
  }
  sere::SourceOverlay overlay;
  overlay.set(stdlib / "prelude.sere", "def live() -> i32:\n    return 2\n");
  sere::Frontend stale;
  if (stale.analyze((root / "app.sere").string(),
                    "def main() -> i32:\n    return live()\n", stdlib)) {
    return fail("disk prelude should not define live()");
  }
  if (!hasMessage(stale, "live")) {
    return fail("expected NameError for live() against disk prelude");
  }
  sere::Frontend live;
  if (!live.analyze((root / "app.sere").string(),
                    "def main() -> i32:\n    return live()\n", stdlib, &overlay)) {
    live.diagnostics().printAll();
    return fail("overlay prelude should define live()");
  }
  if (!hasSymbol(live, "live")) {
    return fail("overlay prelude symbol live() missing");
  }
  return 0;
}

int checkImportOverlay(const std::filesystem::path& root) {
  const std::filesystem::path stdlib = root / "stdlib";
  if (!writeAll(stdlib / "prelude.sere", "def _p() -> i32:\n    return 0\n") ||
      !writeAll(stdlib / "util.sere", "def old() -> i32:\n    return 1\n") ||
      !writeAll(root / "user.sere", "from util import fresh\n\ndef main() -> i32:\n    return fresh()\n")) {
    return fail("could not write import overlay fixtures");
  }
  const std::string user =
      "from util import fresh\n\ndef main() -> i32:\n    return fresh()\n";
  sere::Frontend stale;
  if (stale.analyze((root / "user.sere").string(), user, stdlib) || !hasMessage(stale, "fresh")) {
    return fail("disk util.sere should not export fresh()");
  }
  sere::SourceOverlay overlay;
  overlay.set(stdlib / "util.sere", "def fresh() -> i32:\n    return 7\n");
  sere::Frontend live;
  if (!live.analyze((root / "user.sere").string(), user, stdlib, &overlay)) {
    live.diagnostics().printAll();
    return fail("overlay util.sere should export fresh()");
  }
  return 0;
}

int checkContextPaths(const std::filesystem::path& root) {
  const std::filesystem::path stdlib = root / "ctx-stdlib";
  if (!writeAll(stdlib / "prelude.sere", "def _p() -> i32:\n    return 0\n") ||
      !writeAll(root / "project" / "sere.toml",
                "name = \"demo\"\nsrc = \"src\"\nentry = \"src/main.sere\"\nlibs = \"libs\"\n") ||
      !writeAll(root / "project" / "src" / "main.sere", "def main() -> i32:\n    return 0\n") ||
      !writeAll(root / "project" / "libs" / "helper.sere", "def help() -> i32:\n    return 1\n")) {
    return fail("could not write context path fixtures");
  }
  sere::LanguageContext context;
  context.stdlib = stdlib;
  if (!sere::isLanguageContextPath(stdlib / "io.sere", context) ||
      !sere::isLanguageContextPath(root / "project" / "sere.toml", context) ||
      sere::isLanguageContextPath(root / "unrelated.sere", context)) {
    return fail("stdlib and sere.toml should be context paths");
  }
  const sere::LanguageContext resolved =
      sere::resolveLanguageContext(root / "project" / "src" / "main.sere");
  if (!resolved.project.has_value()) {
    return fail("expected project context from sere.toml");
  }
  if (sere::isLanguageContextPath(root / "project" / "src" / "main.sere", resolved) ||
      sere::isLanguageContextPath(root / "project" / "libs" / "helper.sere", resolved)) {
    return fail("project src/libs modules should refresh dependents, not the whole context");
  }
  if (!sere::isLanguageContextPath(root / "project" / "sere.toml", resolved)) {
    return fail("project sere.toml should be a context path");
  }
  const std::string before = sere::languageContextStamp(resolved);
  if (!writeAll(root / "project" / "sere.toml",
                "name = \"demo2\"\nsrc = \"src\"\nentry = \"src/main.sere\"\nlibs = \"libs\"\n")) {
    return fail("could not update sere.toml");
  }
  const sere::LanguageContext afterContext =
      sere::resolveLanguageContext(root / "project" / "src" / "main.sere");
  const std::string after = sere::languageContextStamp(afterContext);
  if (before == after) {
    return fail("languageContextStamp should change when sere.toml changes");
  }
  return 0;
}

}  // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "sere-lsp-context-test";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  if (const int code = checkPreludeOverlay(root); code != 0) {
    std::filesystem::remove_all(root, error);
    return code;
  }
  if (const int code = checkImportOverlay(root / "imports"); code != 0) {
    std::filesystem::remove_all(root, error);
    return code;
  }
  if (const int code = checkContextPaths(root / "context"); code != 0) {
    std::filesystem::remove_all(root, error);
    return code;
  }
  std::filesystem::remove_all(root, error);
  return 0;
}
