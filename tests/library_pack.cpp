/// @file library_pack.cpp
/// Packs a .slib, extracts it, and imports the drop-in library.

#include "sere/driver/Frontend.h"
#include "sere/driver/ImportPath.h"
#include "sere/driver/Library.h"
#include "sere/lsp/ImportCompletion.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << "library_pack: " << message << '\n';
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

[[nodiscard]] std::string readAll(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  if (sere::isSafeLibraryPath("src/lib.sere") == false || sere::isSafeLibraryPath("../x") ||
      sere::isSafeLibraryPath("C:/abs") || sere::isSafeLibraryPath("/abs")) {
    return fail("isSafeLibraryPath rejected a safe path or accepted an unsafe one");
  }

  const std::filesystem::path temp =
      std::filesystem::temp_directory_path() / "sere-library-pack-test";
  std::error_code fsError;
  std::filesystem::remove_all(temp, fsError);
  std::filesystem::create_directories(temp / "libs", fsError);
  const std::filesystem::path slib = temp / "libs" / "mathlib.slib";

  sere::PackedLibrary packed;
  packed.name = "mathlib";
  packed.version = "1.2.3";
  packed.entry = "src/lib.sere";
  packed.files.push_back(sere::LibraryMember{
      "src/lib.sere", "def add(left: i32, right: i32) -> i32:\n    return left + right\n"});
  packed.files.push_back(
      sere::LibraryMember{"src/extra.sere", "def extra() -> i32:\n    return 7\n"});
  std::string error;
  if (!sere::writePackedLibrary(slib, packed, error)) {
    std::cerr << error << '\n';
    return fail("writePackedLibrary failed");
  }

  sere::PackedLibrary loaded;
  if (!sere::readPackedLibrary(slib, loaded, error)) {
    std::cerr << error << '\n';
    return fail("readPackedLibrary failed");
  }
  if (loaded.name != "mathlib" || loaded.version != "1.2.3" || loaded.entry != "src/lib.sere" ||
      loaded.files.size() != 2) {
    return fail("round-trip lost library metadata");
  }

  sere::PackedLibrary traversal;
  traversal.entry = "../escape.sere";
  traversal.files.push_back(sere::LibraryMember{"../escape.sere", "def bad() -> i32:\n    return 0\n"});
  if (sere::writePackedLibrary(temp / "bad.slib", traversal, error)) {
    return fail("writePackedLibrary must reject path traversal");
  }

  const std::filesystem::path entry = sere::ensureLibraryExtracted(slib, error);
  if (entry.empty() || entry.filename() != "lib.sere") {
    std::cerr << error << '\n';
    return fail("ensureLibraryExtracted should return src/lib.sere");
  }
  if (readAll(entry).find("def add") == std::string::npos) {
    return fail("extracted entry is missing source");
  }

  const std::vector<std::filesystem::path> dirs{temp / "libs"};
  const std::filesystem::path resolved =
      sere::resolveImportFile(dirs, sere::splitImportPath("mathlib"));
  if (resolved.empty() || resolved.filename() != "lib.sere") {
    return fail("import mathlib should resolve the packed .slib entry");
  }

  const std::vector<sere::ImportModuleEntry> modules =
      sere::listImportModules(dirs, {}, "", {});
  bool listed = false;
  for (const sere::ImportModuleEntry& module : modules) {
    listed = listed || module.dottedName == "mathlib";
  }
  if (!listed) {
    return fail("listImportModules should include mathlib.slib");
  }

  const std::filesystem::path stdlib = temp / "stdlib";
  if (!writeAll(stdlib / "prelude.sere", "def _prelude() -> i32:\n    return 0\n")) {
    return fail("could not write prelude");
  }
  const std::string user = "import mathlib\n\ndef main() -> i32:\n    return mathlib.add(2, 3)\n";
  if (!writeAll(temp / "app.sere", user)) {
    return fail("could not write app.sere");
  }
  sere::Frontend frontend;
  if (!frontend.analyze((temp / "app.sere").string(), user, stdlib)) {
    frontend.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("importing a packed .slib should typecheck");
  }
  if (frontend.importedModuleNames().empty()) {
    std::filesystem::remove_all(temp, fsError);
    return fail("frontend should record the imported library name");
  }

  const std::vector<sere::ImportCompletionItem> exports =
      sere::importExportCompletions(slib, "ad");
  bool hasAdd = false;
  for (const sere::ImportCompletionItem& item : exports) {
    hasAdd = hasAdd || item.label == "add";
  }
  if (!hasAdd) {
    std::filesystem::remove_all(temp, fsError);
    return fail("from-import completion should read exports out of a .slib");
  }

  const std::filesystem::path beside = temp / "beside";
  std::filesystem::create_directories(beside, fsError);
  if (!writeAll(beside / "local.sere", "def local() -> i32:\n    return 1\n") ||
      !writeAll(beside / "user.sere",
                "import local\nimport mathlib\n\ndef main() -> i32:\n    return local.local() + mathlib.add(1, 1)\n") ||
      !writeAll(beside / "mathlib.slib",
                readAll(slib))) {
    std::filesystem::remove_all(temp, fsError);
    return fail("could not write same-directory import fixtures");
  }
  const std::filesystem::path previous = std::filesystem::current_path();
  std::filesystem::current_path(beside, fsError);
  sere::Frontend relative;
  const std::string relativeText = readAll(beside / "user.sere");
  const bool relativeOk = relative.analyze("user.sere", relativeText, stdlib);
  std::filesystem::current_path(previous, fsError);
  if (!relativeOk) {
    relative.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("relative sere main.sere should import sibling .sere and .slib");
  }

  const std::filesystem::path folderRoot = temp / "libs" / "nativelib";
  if (!writeAll(folderRoot / "lib.sere",
                "def add(left: i32, right: i32) -> i32:\n    return left + right\n") ||
      !writeAll(folderRoot / "unused.sere", "def unused() -> i32:\n    return 0\n")) {
    std::filesystem::remove_all(temp, fsError);
    return fail("could not write folder library");
  }
  const std::filesystem::path folderEntry = sere::folderLibraryEntry(folderRoot);
  if (folderEntry.empty() || folderEntry.filename() != "lib.sere") {
    std::filesystem::remove_all(temp, fsError);
    return fail("folderLibraryEntry should find lib.sere");
  }
  const std::vector<std::filesystem::path> folderDirs{temp / "libs"};
  const std::filesystem::path folderResolved =
      sere::resolveImportFile(folderDirs, sere::splitImportPath("nativelib"));
  if (folderResolved.empty() || folderResolved.filename() != "lib.sere") {
    std::filesystem::remove_all(temp, fsError);
    return fail("import nativelib should resolve a folder library");
  }
  if (!writeAll(temp / "folder_app.sere",
                "import nativelib\n\ndef main() -> i32:\n    return nativelib.add(2, 3)\n")) {
    std::filesystem::remove_all(temp, fsError);
    return fail("could not write folder app");
  }
  sere::Frontend folderFrontend;
  if (!folderFrontend.analyze((temp / "folder_app.sere").string(),
                              readAll(temp / "folder_app.sere"), stdlib)) {
    folderFrontend.diagnostics().printAll();
    std::filesystem::remove_all(temp, fsError);
    return fail("importing a folder library should typecheck");
  }

  std::filesystem::remove_all(temp, fsError);
  return 0;
}
