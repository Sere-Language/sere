/// @file Project.cpp
/// Discovers sere.toml, builds native libs, and compiles the project entry.

#include "sere/driver/Project.h"

#include "sere/driver/Compiler.h"
#include "sere/driver/ImportPath.h"
#include "sere/driver/Prelude.h"
#include "sere/driver/Toolchain.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
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

[[nodiscard]] bool isLinkLibrary(const std::filesystem::path& path) {
  const std::string ext = path.extension().string();
  return ext == ".lib" || ext == ".a";
}

[[nodiscard]] bool skipLibPath(const std::filesystem::path& path) {
  const std::string text = path.generic_string();
  return text.find("/CMakeFiles/") != std::string::npos ||
         text.find("\\CMakeFiles\\") != std::string::npos;
}

void collectLinkLibraries(const std::filesystem::path& directory,
                          std::vector<std::filesystem::path>& libraries) {
  std::error_code error;
  if (!std::filesystem::exists(directory, error)) {
    return;
  }
  const std::filesystem::recursive_directory_iterator end;
  for (std::filesystem::recursive_directory_iterator it(directory, error); it != end;
       it.increment(error)) {
    if (error) {
      break;
    }
    const std::filesystem::path path = it->path();
    if (!it->is_regular_file(error) || skipLibPath(path) || !isLinkLibrary(path)) {
      continue;
    }
    libraries.push_back(path);
  }
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

[[nodiscard]] int buildNativeLibs(const std::filesystem::path& nativeDir) {
  if (!std::filesystem::exists(nativeDir / "CMakeLists.txt")) {
    return 0;
  }
  const llvm::ErrorOr<std::string> cmake = llvm::sys::findProgramByName("cmake");
  if (!cmake) {
    llvm::errs() << "note: cmake not found; skipping native libs in " << nativeDir.string()
                 << '\n';
    return 0;
  }
  const int configured = configureNative(*cmake, nativeDir);
  if (configured != 0) {
    return configured;
  }
  return compileNative(*cmake, nativeDir);
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

}  // namespace

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
    manifest.output = defaultOutput(root, manifest.name);
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

int buildProject(const CompilerOptions& options) {
  std::string error;
  const std::optional<ProjectManifest> manifest = requireManifest(error);
  if (!manifest.has_value()) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  std::error_code fsError;
  std::filesystem::create_directories(manifest->output.parent_path(), fsError);
  if (manifest->native) {
    const int nativeCode = buildNativeLibs(manifest->libs / "native");
    if (nativeCode != 0) {
      llvm::errs() << "note: native library build failed; continuing with Sere sources\n";
    }
  }
  if (std::getenv("SERE_STDLIB") == nullptr) {
    setEnvironmentVariable("SERE_STDLIB", manifest->stdlib.string());
  }
  const CompilerOptions compile = compileOptionsFor(*manifest, options);
  std::cout << "sere build " << manifest->name << " -> " << compile.outputPath.string() << '\n';
  return compileInput(compile);
}

int runProject(const CompilerOptions& options) {
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
  const std::filesystem::path nativeBuild = manifest->libs / "native" / "build";
  std::error_code fsError;
  std::filesystem::remove_all(bin, fsError);
  std::filesystem::remove_all(nativeBuild, fsError);
  std::filesystem::create_directories(bin, fsError);
  std::cout << "sere clean " << manifest->name << '\n';
  return 0;
}

}  // namespace sere
