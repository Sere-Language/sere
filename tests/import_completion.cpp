/// @file import_completion.cpp
/// Checks import-statement completion context, module listing, and export names.

#include "sere/driver/ImportPath.h"
#include "sere/lsp/ImportCompletion.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << "import_completion: " << message << '\n';
  return 1;
}

void writeFile(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << text;
}

[[nodiscard]] bool hasLabel(const std::vector<sere::ImportCompletionItem>& items,
                            std::string_view label) {
  for (const sere::ImportCompletionItem& item : items) {
    if (item.label == label) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool hasModule(const std::vector<sere::ImportModuleEntry>& items,
                             std::string_view name) {
  for (const sere::ImportModuleEntry& item : items) {
    if (item.dottedName == name) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] sere::ImportCompletionQuery queryAt(const std::string& text) {
  return sere::detectImportCompletion(text, static_cast<std::uint32_t>(text.size()));
}

int testDetect() {
  const sere::ImportCompletionQuery emptyImport = queryAt("import ");
  if (emptyImport.kind != sere::ImportCompletionKind::ModulePath || !emptyImport.typedPath.empty()) {
    return fail("import space should complete modules");
  }
  const sere::ImportCompletionQuery prefix = queryAt("import ut");
  if (prefix.kind != sere::ImportCompletionKind::ModulePath || prefix.typedPath != "ut" ||
      prefix.prefix != "ut") {
    return fail("import ut should prefix-filter modules");
  }
  const sere::ImportCompletionQuery dotted = queryAt("import pkg.");
  if (dotted.kind != sere::ImportCompletionKind::ModulePath || dotted.typedPath != "pkg.") {
    return fail("import pkg. should complete submodules");
  }
  const sere::ImportCompletionQuery asKeyword = queryAt("import util ");
  if (asKeyword.kind != sere::ImportCompletionKind::ImportAsKeyword) {
    return fail("import util space should complete 'as'");
  }
  const sere::ImportCompletionQuery fromMod = queryAt("from ut");
  if (fromMod.kind != sere::ImportCompletionKind::ModulePath || fromMod.typedPath != "ut") {
    return fail("from ut should complete modules");
  }
  const sere::ImportCompletionQuery fromImport = queryAt("from util ");
  if (fromImport.kind != sere::ImportCompletionKind::FromImportKeyword) {
    return fail("from util space should complete 'import'");
  }
  const sere::ImportCompletionQuery names = queryAt("from util import d");
  if (names.kind != sere::ImportCompletionKind::FromNames || names.modulePath != "util" ||
      names.prefix != "d") {
    return fail("from util import d should complete exports");
  }
  const sere::ImportCompletionQuery star = queryAt("from util import *");
  if (star.kind != sere::ImportCompletionKind::None) {
    return fail("star import should not complete names");
  }
  const sere::ImportCompletionQuery notImport = queryAt("x = import");
  if (notImport.kind != sere::ImportCompletionKind::None) {
    return fail("non-import lines must stay ordinary completions");
  }
  return 0;
}

int testModulesAndExports() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "sere-import-completion-test";
  std::filesystem::remove_all(root);
  const std::filesystem::path stdlib = root / "stdlib";
  writeFile(stdlib / "prelude.sere", "def abs(value: i32) -> i32:\n    return value\n");
  writeFile(stdlib / "util.sere", "def double(value: i32) -> i32:\n    return value + value\n");
  writeFile(stdlib / "io.sere", "def write() -> void:\n    pass\n");
  writeFile(root / "pkg" / "inner.sere", "def inner() -> i32:\n    return 1\n");
  writeFile(root / "app.sere", "def main() -> i32:\n    return 0\n");

  const std::vector<std::filesystem::path> dirs{root, stdlib};
  const std::vector<sere::ImportModuleEntry> top =
      sere::listImportModules(dirs, stdlib, "", root / "app.sere");
  if (!hasModule(top, "util") || !hasModule(top, "io") || !hasModule(top, "pkg")) {
    return fail("expected stdlib modules and workspace package");
  }
  if (hasModule(top, "prelude") || hasModule(top, "app")) {
    return fail("prelude and current file must be omitted");
  }
  const std::vector<sere::ImportModuleEntry> filtered =
      sere::listImportModules(dirs, stdlib, "ut", {});
  if (!hasModule(filtered, "util") || hasModule(filtered, "io")) {
    return fail("typed prefix ut should only match util");
  }
  const std::vector<sere::ImportModuleEntry> nested =
      sere::listImportModules(dirs, stdlib, "pkg.", {});
  if (!hasModule(nested, "pkg.inner")) {
    return fail("pkg. should list pkg.inner");
  }
  writeFile(root / "util.sere", "def local() -> i32:\n    return 1\n");
  const std::filesystem::path shadowed =
      sere::resolveImportFile(dirs, sere::splitImportPath("util"));
  if (shadowed.empty() || shadowed.filename() != "util.sere" ||
      shadowed.parent_path().filename() != root.filename()) {
    return fail("workspace util.sere should shadow stdlib without skipFile");
  }
  const std::filesystem::path skipped =
      sere::resolveImportFile(dirs, sere::splitImportPath("util"), root / "util.sere");
  if (skipped.empty() || skipped == shadowed) {
    return fail("skipFile should fall through to stdlib util.sere");
  }
  const std::filesystem::path resolved =
      sere::resolveImportFile(dirs, sere::splitImportPath("pkg.inner"));
  if (resolved.empty() || resolved.filename() != "inner.sere") {
    return fail("pkg.inner should resolve to inner.sere");
  }
  const std::vector<sere::ImportCompletionItem> exports =
      sere::importExportCompletions(stdlib / "util.sere", "d");
  if (!hasLabel(exports, "double") || hasLabel(exports, "*")) {
    return fail("from-import prefix d should offer double and not *");
  }
  const std::vector<sere::ImportCompletionItem> allExports =
      sere::importExportCompletions(stdlib / "util.sere", "");
  if (!hasLabel(allExports, "*") || !hasLabel(allExports, "double")) {
    return fail("empty from-import prefix should offer * and exports");
  }
  std::filesystem::remove_all(root);
  return 0;
}

}  // namespace

int main() {
  if (const int status = testDetect(); status != 0) {
    return status;
  }
  return testModulesAndExports();
}
