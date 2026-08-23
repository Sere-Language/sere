/// @file Project.cpp
/// Discovers sere.toml, builds native libs, and compiles the project entry.

#include "sere/driver/Project.h"

#include "sere/driver/Compiler.h"
#include "sere/driver/Frontend.h"
#include "sere/driver/ImportPath.h"
#include "sere/driver/Library.h"
#include "sere/driver/Prelude.h"
#include "sere/driver/Toolchain.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <string_view>
#include <system_error>

namespace sere {
namespace {

[[nodiscard]] std::string trimCopy(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.remove_suffix(1);
  }
  return std::string(text);
}

[[nodiscard]] std::string unquote(std::string_view text) {
  const std::string trimmed = trimCopy(text);
  if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
    return trimmed.substr(1, trimmed.size() - 2);
  }
  return trimmed;
}

[[nodiscard]] std::filesystem::path resolvePath(const std::filesystem::path& root,
                                                const std::filesystem::path& value) {
  if (value.empty() || value.is_absolute()) {
    return value;
  }
  return root / value;
}

[[nodiscard]] std::filesystem::path defaultOutput(const std::filesystem::path& root,
                                                  const std::string& name) {
#ifdef _WIN32
  return root / "bin" / (name + ".exe");
#else
  return root / "bin" / name;
#endif
}

void applyTomlKey(ProjectManifest& manifest, std::string_view key, std::string_view value) {
  if (key == "name") {
    manifest.name = std::string(value);
    return;
  }
  if (key == "version") {
    manifest.version = std::string(value);
    return;
  }
  if (key == "kind") {
    if (value == "lib" || value == "library") {
      manifest.kind = ProjectKind::Lib;
    } else {
      manifest.kind = ProjectKind::App;
    }
    return;
  }
  if (key == "src") {
    manifest.src = value;
    return;
  }
  if (key == "entry") {
    manifest.entry = value;
    return;
  }
  if (key == "libs") {
    manifest.libs = value;
    return;
  }
  if (key == "stdlib") {
    manifest.stdlib = value;
    return;
  }
  if (key == "output") {
    manifest.output = value;
    return;
  }
  if (key == "opt") {
    std::string parseError;
    OptLevel level = OptLevel::O0;
    if (parseOptLevel(value, level, parseError)) {
      manifest.optLevel = level;
    }
    return;
  }
  if (key == "native") {
    manifest.native = (value == "true" || value == "1" || value == "yes");
  }
}

[[nodiscard]] int runProcess(const std::string& program, const std::vector<std::string>& args) {
  llvm::SmallVector<llvm::StringRef, 16> refs;
  refs.reserve(args.size());
  for (const std::string& argument : args) {
    refs.push_back(argument);
  }
  std::string launchError;
  bool failed = false;
  const int code =
      llvm::sys::ExecuteAndWait(program, refs, std::nullopt, {}, 0, 0, &launchError, &failed);
  if (failed) {
    llvm::errs() << "error: failed to launch '" << program << "': " << launchError << '\n';
    return 1;
  }
  return code;
}

void collectLinkLibraries(const std::filesystem::path& directory,
                          std::vector<std::filesystem::path>& libraries) {
  collectNativeLinkFiles(directory, libraries);
}

[[nodiscard]] int configureNative(const std::string& cmake, const std::filesystem::path& nativeDir) {
  const std::filesystem::path buildDir = nativeDir / "build";
  const std::vector<std::string> configure{cmake, "-S", nativeDir.string(), "-B",
                                           buildDir.string()};
  return runProcess(cmake, configure);
}

[[nodiscard]] int compileNative(const std::string& cmake, const std::filesystem::path& nativeDir) {
  const std::filesystem::path buildDir = nativeDir / "build";
  const std::vector<std::string> build{cmake, "--build", buildDir.string()};
  const int code = runProcess(cmake, build);
  if (code == 0) {
    return 0;
  }
  const std::vector<std::string> release{cmake, "--build", buildDir.string(), "--config",
                                         "Release"};
  return runProcess(cmake, release);
}

[[nodiscard]] int buildNativeCMake(const std::filesystem::path& nativeDir) {
  if (!std::filesystem::exists(nativeDir / "CMakeLists.txt")) {
    return 0;
  }
  const llvm::ErrorOr<std::string> cmake = llvm::sys::findProgramByName("cmake");
  if (!cmake) {
    llvm::errs() << "note: cmake not found; skipping native cmake in " << nativeDir.string()
                 << '\n';
    return 0;
  }
  const int configured = configureNative(*cmake, nativeDir);
  if (configured != 0) {
    return configured;
  }
  return compileNative(*cmake, nativeDir);
}

void collectLooseNativeSources(const std::filesystem::path& directory,
                               std::vector<std::filesystem::path>& sources) {
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error) || error) {
    return;
  }
  const std::filesystem::directory_iterator end{};
  for (std::filesystem::directory_iterator it(directory, error); !error && it != end;
       it.increment(error)) {
    if (it->is_regular_file(error) && isNativeSourceFile(it->path())) {
      sources.push_back(it->path());
    }
  }
}

[[nodiscard]] bool archiveNewerThanSources(const std::filesystem::path& archive,
                                           const std::vector<std::filesystem::path>& sources) {
  std::error_code error;
  if (!std::filesystem::exists(archive, error) || error) {
    return false;
  }
  const auto archiveTime = std::filesystem::last_write_time(archive, error);
  if (error) {
    return false;
  }
  for (const std::filesystem::path& source : sources) {
    const auto sourceTime = std::filesystem::last_write_time(source, error);
    if (error || sourceTime > archiveTime) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::optional<std::string> findLlvmAr() {
  const std::optional<std::filesystem::path> tools = llvmToolsDirectory();
  if (tools.has_value()) {
#ifdef _WIN32
    const std::filesystem::path ar = *tools / "llvm-ar.exe";
#else
    const std::filesystem::path ar = *tools / "llvm-ar";
#endif
    std::error_code error;
    if (std::filesystem::exists(ar, error)) {
      return ar.string();
    }
  }
  const llvm::ErrorOr<std::string> found = llvm::sys::findProgramByName("llvm-ar");
  if (!found) {
    return std::nullopt;
  }
  return *found;
}

[[nodiscard]] int compileLooseNative(const std::filesystem::path& nativeDir) {
  std::vector<std::filesystem::path> sources;
  collectLooseNativeSources(nativeDir, sources);
  if (sources.empty()) {
    return 0;
  }
#ifdef _WIN32
  const std::filesystem::path archive = nativeDir / "sere_native.lib";
#else
  const std::filesystem::path archive = nativeDir / "libsere_native.a";
#endif
  std::vector<std::filesystem::path> existing;
  collectNativeLinkFiles(nativeDir, existing);
  if (!existing.empty()) {
    bool fresh = true;
    for (const std::filesystem::path& library : existing) {
      fresh = fresh && archiveNewerThanSources(library, sources);
    }
    if (fresh) {
      return 0;
    }
  }
  const std::optional<std::string> clang = findClang();
  if (!clang.has_value()) {
    llvm::errs() << "note: clang not found; skipping native sources in " << nativeDir.string()
                 << '\n';
    return 0;
  }
  std::vector<std::filesystem::path> objects;
  for (const std::filesystem::path& source : sources) {
#ifdef _WIN32
    const std::filesystem::path object = nativeDir / (source.stem().string() + ".obj");
#else
    const std::filesystem::path object = nativeDir / (source.stem().string() + ".o");
#endif
    const std::vector<std::string> compile{*clang, "-c", source.string(), "-o", object.string()};
    if (runProcess(*clang, compile) != 0) {
      return 1;
    }
    objects.push_back(object);
  }
  const std::optional<std::string> ar = findLlvmAr();
  if (!ar.has_value()) {
    llvm::errs() << "note: llvm-ar not found; native objects were compiled in "
                 << nativeDir.string() << '\n';
    return 0;
  }
  std::vector<std::string> archiveArgs{*ar, "rcs", archive.string()};
  for (const std::filesystem::path& object : objects) {
    archiveArgs.push_back(object.string());
  }
  return runProcess(*ar, archiveArgs);
}

[[nodiscard]] bool pathIsUnderRoot(const std::filesystem::path& file,
                                   const std::filesystem::path& root) {
  if (file.empty() || root.empty()) {
    return false;
  }
  std::error_code error;
  const std::string fileText = std::filesystem::weakly_canonical(file, error).generic_string();
  const std::string rootText = std::filesystem::weakly_canonical(root, error).generic_string();
  if (error || rootText.empty() || fileText.size() < rootText.size()) {
    return false;
  }
  if (!fileText.starts_with(rootText)) {
    return false;
  }
  return fileText.size() == rootText.size() || fileText[rootText.size()] == '/';
}

[[nodiscard]] std::optional<ProjectManifest> requireManifest(std::string& error) {
  const std::optional<std::filesystem::path> root = findProjectRoot(std::filesystem::current_path());
  if (!root.has_value()) {
    error = "no sere.toml found; run this command from a Sere project (or sere init <name>)";
    return std::nullopt;
  }
  ProjectManifest manifest;
  if (!loadProjectManifest(*root, manifest, error)) {
    return std::nullopt;
  }
  return manifest;
}

[[nodiscard]] CompilerOptions compileOptionsFor(const ProjectManifest& manifest,
                                                const CompilerOptions& options) {
  CompilerOptions compile = options;
  compile.projectCommand = ProjectCommand::None;
  compile.inputPath = manifest.entry;
  if (options.outputPath.empty()) {
    compile.outputPath = manifest.output;
  }
  if (!options.optOverridden) {
    compile.optLevel = manifest.optLevel;
  }
  collectLinkLibraries(manifest.libs, compile.linkLibraries);
  return compile;
}

[[nodiscard]] bool stdlibUsable(const std::filesystem::path& directory) {
  std::error_code error;
  return !directory.empty() && std::filesystem::exists(directory / "prelude.sere", error);
}

[[nodiscard]] std::filesystem::path envPath(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return {};
  }
  return std::filesystem::path(value);
}

[[nodiscard]] bool samePath(const std::filesystem::path& left, const std::filesystem::path& right) {
  if (left.empty() || right.empty()) {
    return false;
  }
  std::error_code error;
  const std::filesystem::path a = std::filesystem::weakly_canonical(left, error);
  const std::filesystem::path b = std::filesystem::weakly_canonical(right, error);
  return !error && a == b;
}

[[nodiscard]] std::optional<ProjectManifest> manifestFromRoot(const std::filesystem::path& root) {
  if (root.empty()) {
    return std::nullopt;
  }
  std::string error;
  ProjectManifest manifest;
  if (!loadProjectManifest(root, manifest, error)) {
    return std::nullopt;
  }
  return manifest;
}

void copyStdlibTree(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code error;
  if (!stdlibUsable(from) || from.empty() || to.empty() || samePath(from, to)) {
    return;
  }
  std::filesystem::create_directories(to, error);
  std::filesystem::copy(from, to,
                        std::filesystem::copy_options::recursive |
                            std::filesystem::copy_options::overwrite_existing,
                        error);
}

[[nodiscard]] std::optional<std::string> readBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  std::ostringstream stream;
  stream << input.rdbuf();
  return stream.str();
}

[[nodiscard]] std::string relativeGeneric(const std::filesystem::path& root,
                                          const std::filesystem::path& file) {
  std::error_code error;
  const std::filesystem::path relative = std::filesystem::relative(file, root, error);
  if (error || relative.empty()) {
    return file.filename().generic_string();
  }
  return relative.generic_string();
}

void addPackedFile(PackedLibrary& library, const std::filesystem::path& root,
                   const std::filesystem::path& file) {
  if (!isSafeLibraryPath(relativeGeneric(root, file))) {
    return;
  }
  const std::string relative = relativeGeneric(root, file);
  for (const LibraryMember& existing : library.files) {
    if (existing.relativePath == relative) {
      return;
    }
  }
  const std::optional<std::string> bytes = readBytes(file);
  if (!bytes.has_value()) {
    return;
  }
  LibraryMember member;
  member.relativePath = relative;
  member.bytes = *bytes;
  library.files.push_back(std::move(member));
}

void collectPackedNative(PackedLibrary& library, const std::filesystem::path& root,
                         const std::filesystem::path& directory) {
  std::vector<std::filesystem::path> files;
  collectNativeLinkFiles(directory, files);
  collectNativeRuntimeFiles(directory, files);
  for (const std::filesystem::path& file : files) {
    addPackedFile(library, root, file);
  }
}

[[nodiscard]] bool libraryHasNativeBinary(const PackedLibrary& library) {
  for (const LibraryMember& file : library.files) {
    const std::filesystem::path path(file.relativePath);
    if (isNativeLinkFile(path) || isNativeRuntimeFile(path)) {
      return true;
    }
  }
  return false;
}

void collectPackedNativeSources(PackedLibrary& library, const std::filesystem::path& root,
                                const std::filesystem::path& directory) {
  if (libraryHasNativeBinary(library)) {
    return;
  }
  std::vector<std::filesystem::path> sources;
  collectLooseNativeSources(directory, sources);
  for (const std::filesystem::path& file : sources) {
    addPackedFile(library, root, file);
  }
  const std::filesystem::path cmake = directory / "CMakeLists.txt";
  std::error_code error;
  if (std::filesystem::is_regular_file(cmake, error)) {
    addPackedFile(library, root, cmake);
  }
}

void collectReachableSere(PackedLibrary& library, const std::filesystem::path& root,
                          const std::filesystem::path& stdlib, const std::filesystem::path& entry,
                          const std::vector<std::filesystem::path>& imported) {
  addPackedFile(library, root, entry);
  for (const std::filesystem::path& file : imported) {
    if (isExtractedLibraryPath(file) || pathIsUnderRoot(file, stdlib) ||
        !pathIsUnderRoot(file, root) || file.extension() != ".sere") {
      continue;
    }
    addPackedFile(library, root, file);
  }
}

void collectModuleNative(PackedLibrary& library, const std::filesystem::path& root,
                         const std::filesystem::path& moduleFile) {
  const std::filesystem::path parent = moduleFile.parent_path();
  const std::string stem = moduleFile.stem().string();
  std::error_code error;
  for (const char* ext : {".lib", ".a", ".dll", ".so", ".dylib"}) {
    const std::filesystem::path sibling = parent / (stem + ext);
    if (std::filesystem::is_regular_file(sibling, error)) {
      addPackedFile(library, root, sibling);
    }
  }
  collectPackedNative(library, root, parent / "native");
  const std::filesystem::path nativeRoot = libraryNativeRoot(moduleFile);
  if (!nativeRoot.empty()) {
    collectPackedNative(library, root, nativeRoot);
    collectPackedNativeSources(library, root, nativeRoot);
    collectPackedNativeSources(library, root, nativeRoot / "native");
  }
  collectPackedNativeSources(library, root, parent / "native");
}

void buildPackNative(const std::filesystem::path& root) {
  static_cast<void>(buildNativeLibs(root / "native"));
  static_cast<void>(buildNativeLibs(root / "libs" / "native"));
  if (!folderLibraryEntry(root).empty()) {
    static_cast<void>(buildNativeLibs(root));
  }
}

struct PackCheck {
  int code = 1;
  std::vector<std::filesystem::path> imported;
};

[[nodiscard]] PackCheck typecheckPackedEntry(const std::filesystem::path& entry,
                                             const std::filesystem::path& stdlib,
                                             ColorMode colorMode) {
  PackCheck result;
  const std::optional<std::string> text = readBytes(entry);
  if (!text.has_value()) {
    llvm::errs() << "error: cannot read library entry '" << entry.string() << "'\n";
    return result;
  }
  Frontend frontend;
  const bool ok = frontend.analyze(entry.string(), *text, stdlib);
  frontend.diagnostics().setColorMode(colorMode);
  if (!ok || frontend.diagnostics().hasErrors()) {
    frontend.diagnostics().printAll();
    return result;
  }
  result.code = 0;
  result.imported = frontend.importedModulePaths();
  return result;
}

[[nodiscard]] int writeLibraryArchive(const PackedLibrary& library,
                                      const std::filesystem::path& output) {
  std::string error;
  if (!writePackedLibrary(output, library, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  std::cout << "sere pack " << library.name << " -> " << output.string() << '\n';
  return 0;
}

[[nodiscard]] int packStandalone(const CompilerOptions& options) {
  const std::filesystem::path entry = std::filesystem::absolute(options.inputPath);
  std::error_code error;
  if (!std::filesystem::is_regular_file(entry, error)) {
    llvm::errs() << "error: cannot pack '" << entry.string() << "'\n";
    return 1;
  }
  const std::filesystem::path root = entry.parent_path();
  PackedLibrary library;
  library.name = entry.stem().string();
  library.entry = entry.filename().generic_string();
  const LanguageContext language = resolveLanguageContext(entry);
  const std::filesystem::path stdlib =
      language.stdlib.empty() ? findStdlibDirectory(compilerDirectory()) : language.stdlib;
  const PackCheck check = typecheckPackedEntry(entry, stdlib, options.colorMode);
  if (check.code != 0) {
    return 1;
  }
  buildPackNative(root);
  collectReachableSere(library, root, stdlib, entry, check.imported);
  collectPackedNative(library, root, root / "native");
  collectPackedNative(library, root, root / "libs" / "native");
  collectModuleNative(library, root, entry);
  for (const std::filesystem::path& imported : check.imported) {
    collectModuleNative(library, root, imported);
  }
  const std::filesystem::path output =
      options.outputPath.empty() ? std::filesystem::current_path() / (library.name + ".slib")
                                 : options.outputPath;
  return writeLibraryArchive(library, output);
}

}  // namespace

int buildNativeLibs(const std::filesystem::path& nativeDir) {
  std::error_code error;
  if (!std::filesystem::exists(nativeDir, error) || error) {
    return 0;
  }
  const int cmakeCode = buildNativeCMake(nativeDir);
  if (cmakeCode != 0) {
    return cmakeCode;
  }
  return compileLooseNative(nativeDir);
}

void prepareImportedLibraryNative(const std::vector<std::filesystem::path>& importedPaths) {
  std::vector<std::filesystem::path> dirs;
  for (const std::filesystem::path& imported : importedPaths) {
    const std::filesystem::path parent = imported.parent_path();
    if (!parent.empty()) {
      appendImportSearchDir(dirs, parent / "native");
    }
    const std::filesystem::path root = libraryNativeRoot(imported);
    if (!root.empty()) {
      appendImportSearchDir(dirs, root);
      appendImportSearchDir(dirs, root / "native");
    }
  }
  for (const std::filesystem::path& directory : dirs) {
    static_cast<void>(buildNativeLibs(directory));
  }
}

void copyIfPresent(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code error;
  if (!std::filesystem::exists(from, error)) {
    return;
  }
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, error);
}

void prepareProjectStdlib(const ProjectManifest& manifest) {
  if (!stdlibUsable(manifest.stdlib)) {
    copyStdlibTree(findStdlibDirectory(compilerDirectory()), manifest.stdlib);
  }
  const std::filesystem::path dest = manifest.root / "venv" / "bin";
  const std::filesystem::path from = compilerDirectory();
  std::error_code error;
  std::filesystem::create_directories(dest, error);
  if (!std::filesystem::exists(dest / "sere.exe", error) &&
      !std::filesystem::exists(dest / "sere", error)) {
    copyIfPresent(from / "sere.exe", dest / "sere.exe");
    copyIfPresent(from / "sere", dest / "sere");
  }
  for (const char* name :
       {"sere_rt.lib", "sere_qt6.lib", "sere_qt6.dll", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll"}) {
    copyIfPresent(from / name, dest / name);
  }
}

std::optional<std::filesystem::path> findProjectRoot(const std::filesystem::path& start) {
  std::error_code error;
  std::filesystem::path current = std::filesystem::absolute(start, error);
  if (error) {
    return std::nullopt;
  }
  std::error_code fileError;
  if (std::filesystem::is_regular_file(current, fileError)) {
    current = current.parent_path();
  }
  while (true) {
    if (std::filesystem::exists(current / "sere.toml", error)) {
      return current;
    }
    const std::filesystem::path parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return std::nullopt;
}

bool loadProjectManifest(const std::filesystem::path& root, ProjectManifest& manifest,
                         std::string& error) {
  const std::filesystem::path tomlPath = root / "sere.toml";
  std::ifstream input(tomlPath, std::ios::binary);
  if (!input) {
    error = "cannot read '" + tomlPath.string() + "'";
    return false;
  }
  manifest.root = root;
  manifest.name = root.filename().string();
  manifest.version = "0.1.0";
  manifest.kind = ProjectKind::App;
  manifest.src = "src";
  manifest.entry = "src/main.sere";
  manifest.libs = "libs";
  manifest.stdlib = "venv/stdlib";
  manifest.output.clear();

  std::string line;
  while (std::getline(input, line)) {
    const std::string trimmed = trimCopy(line);
    if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == '[') {
      continue;
    }
    const std::size_t eq = trimmed.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const std::string key = trimCopy(trimmed.substr(0, eq));
    const std::string value = unquote(trimmed.substr(eq + 1));
    applyTomlKey(manifest, key, value);
  }
  manifest.src = resolvePath(root, manifest.src);
  manifest.entry = resolvePath(root, manifest.entry);
  manifest.libs = resolvePath(root, manifest.libs);
  manifest.stdlib = resolvePath(root, manifest.stdlib);
  if (manifest.output.empty()) {
    if (manifest.kind == ProjectKind::Lib) {
      manifest.output = root / "dist" / (manifest.name + ".slib");
    } else {
      manifest.output = defaultOutput(root, manifest.name);
    }
  } else {
    manifest.output = resolvePath(root, manifest.output);
  }
  return true;
}

LanguageContext resolveLanguageContext(const std::filesystem::path& start) {
  LanguageContext context;
  const std::filesystem::path envRoot = envPath("SERE_PROJECT_ROOT");
  const std::filesystem::path envStdlib = envPath("SERE_STDLIB");
  const bool envActive = std::getenv("SERE_ACTIVE") != nullptr;
  std::optional<std::filesystem::path> root = findProjectRoot(start);
  if (!root.has_value() && !envRoot.empty()) {
    std::error_code error;
    if (std::filesystem::exists(envRoot / "sere.toml", error)) {
      root = envRoot;
    }
  }
  if (!root.has_value()) {
    root = findProjectRoot(std::filesystem::current_path());
  }
  if (root.has_value()) {
    context.project = manifestFromRoot(*root);
  }
  if (context.project.has_value()) {
    const bool envMatchesProject = envActive && samePath(envRoot, context.project->root);
    if (envMatchesProject && stdlibUsable(envStdlib)) {
      context.stdlib = envStdlib;
    } else if (stdlibUsable(context.project->stdlib)) {
      context.stdlib = context.project->stdlib;
    } else if (stdlibUsable(envStdlib)) {
      context.stdlib = envStdlib;
    }
  } else if (stdlibUsable(envStdlib)) {
    context.stdlib = envStdlib;
  }
  if (context.stdlib.empty()) {
    context.stdlib = findStdlibDirectory(compilerDirectory());
  }
  return context;
}

void appendLanguageContextDirs(std::vector<std::filesystem::path>& dirs,
                               const LanguageContext& context) {
  if (!context.project.has_value()) {
    return;
  }
  appendImportSearchDir(dirs, context.project->libs);
  appendImportSearchDir(dirs, context.project->src);
  appendImportSearchDir(dirs, context.project->root);
}

int packLibrary(const CompilerOptions& options) {
  if (!options.inputPath.empty()) {
    return packStandalone(options);
  }
  std::string error;
  const std::optional<ProjectManifest> manifest = requireManifest(error);
  if (!manifest.has_value()) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  std::error_code fsError;
  if (manifest->native) {
    const int nativeCode = buildNativeLibs(manifest->libs / "native");
    if (nativeCode != 0) {
      llvm::errs() << "note: native library build failed; packing Sere sources only\n";
    }
  }
  prepareProjectStdlib(*manifest);
  const std::filesystem::path stdlib = stdlibUsable(manifest->stdlib)
                                           ? manifest->stdlib
                                           : findStdlibDirectory(compilerDirectory());
  const PackCheck check = typecheckPackedEntry(manifest->entry, stdlib, options.colorMode);
  if (check.code != 0) {
    return 1;
  }
  PackedLibrary library;
  library.name = manifest->name;
  library.version = manifest->version;
  library.entry = relativeGeneric(manifest->root, manifest->entry);
  collectReachableSere(library, manifest->root, stdlib, manifest->entry, check.imported);
  collectPackedNative(library, manifest->root, manifest->libs);
  collectPackedNative(library, manifest->root, manifest->libs / "native");
  collectModuleNative(library, manifest->root, manifest->entry);
  for (const std::filesystem::path& imported : check.imported) {
    collectModuleNative(library, manifest->root, imported);
  }
  const std::filesystem::path output =
      options.outputPath.empty() ? manifest->output : options.outputPath;
  std::filesystem::create_directories(output.parent_path(), fsError);
  return writeLibraryArchive(library, output);
}

int buildProject(const CompilerOptions& options) {
  std::string error;
  const std::optional<ProjectManifest> manifest = requireManifest(error);
  if (!manifest.has_value()) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  if (manifest->kind == ProjectKind::Lib) {
    return packLibrary(options);
  }
  std::error_code fsError;
  std::filesystem::create_directories(manifest->output.parent_path(), fsError);
  if (manifest->native) {
    const int nativeCode = buildNativeLibs(manifest->libs / "native");
    if (nativeCode != 0) {
      llvm::errs() << "note: native library build failed; continuing with Sere sources\n";
    }
  }
  prepareProjectStdlib(*manifest);
  const std::filesystem::path stdlib = stdlibUsable(manifest->stdlib)
                                           ? manifest->stdlib
                                           : findStdlibDirectory(compilerDirectory());
  if (stdlibUsable(stdlib)) {
    setEnvironmentVariable("SERE_STDLIB", stdlib.string());
  }
  const CompilerOptions compile = compileOptionsFor(*manifest, options);
  std::cout << "sere build " << manifest->name << " -> " << compile.outputPath.string() << '\n';
  return compileInput(compile);
}

int runProject(const CompilerOptions& options) {
  std::string manifestError;
  const std::optional<ProjectManifest> current = requireManifest(manifestError);
  if (current.has_value() && current->kind == ProjectKind::Lib) {
    llvm::errs() << "error: '" << current->name
                 << "' is a library; use sere pack and import the .slib\n";
    return 1;
  }
  const int built = buildProject(options);
  if (built != 0) {
    return built;
  }
  std::string error;
  const std::optional<ProjectManifest> manifest = requireManifest(error);
  if (!manifest.has_value()) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  const std::filesystem::path exe =
      options.outputPath.empty() ? manifest->output : options.outputPath;
  std::vector<std::string> args{exe.string()};
  args.insert(args.end(), options.programArgs.begin(), options.programArgs.end());
  std::cout << "sere run " << exe.string() << '\n';
  return runProcess(exe.string(), args);
}

int cleanProject(const CompilerOptions& options) {
  (void)options;
  std::string error;
  const std::optional<ProjectManifest> manifest = requireManifest(error);
  if (!manifest.has_value()) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  const std::filesystem::path bin = manifest->root / "bin";
  const std::filesystem::path dist = manifest->root / "dist";
  const std::filesystem::path nativeBuild = manifest->libs / "native" / "build";
  const std::filesystem::path extracted = manifest->libs / ".sere-lib";
  std::error_code fsError;
  std::filesystem::remove_all(bin, fsError);
  std::filesystem::remove_all(dist, fsError);
  std::filesystem::remove_all(nativeBuild, fsError);
  std::filesystem::remove_all(extracted, fsError);
  std::filesystem::create_directories(bin, fsError);
  std::cout << "sere clean " << manifest->name << '\n';
  return 0;
}

}  // namespace sere
