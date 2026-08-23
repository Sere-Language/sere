/// @file Toolchain.cpp
/// Discovers the LLVM tools used to link Sere programs.

#include "sere/driver/Toolchain.h"

#include "sere/ToolchainPaths.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <stdlib.h>
#include <windows.h>
#endif

namespace sere {
namespace {

[[nodiscard]] bool isFile(const std::filesystem::path& path) {
  return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

[[nodiscard]] std::optional<std::filesystem::path> firstExisting(
    const std::vector<std::filesystem::path>& candidates) {
  for (const std::filesystem::path& candidate : candidates) {
    if (isFile(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

}  // namespace

std::filesystem::path compilerDirectory() {
  const std::string executable =
      llvm::sys::fs::getMainExecutable(nullptr, reinterpret_cast<void*>(&compilerDirectory));
  return std::filesystem::path(executable).parent_path();
}

std::optional<std::filesystem::path> llvmToolsDirectory() {
  if (const char* fromEnv = std::getenv("SERE_LLVM_DIR")) {
    const std::filesystem::path bin = std::filesystem::path(fromEnv) / "bin";
    if (isFile(bin / "clang.exe") || isFile(bin / "clang")) {
      return bin;
    }
  }
  const std::filesystem::path compiledIn(SERE_LLVM_TOOLS_DIR);
  if (isFile(compiledIn / "clang.exe") || isFile(compiledIn / "clang")) {
    return compiledIn;
  }
  const std::filesystem::path compilerDir = compilerDirectory();
  const std::filesystem::path installed =
      compilerDir.parent_path() / "toolchains" / ("llvm-" SERE_PINNED_LLVM_VERSION) / "bin";
  if (isFile(installed / "clang.exe") || isFile(installed / "clang")) {
    return installed;
  }
  if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
    const std::filesystem::path pinned = std::filesystem::path(localAppData) / "sere" / "toolchains" /
                                         ("llvm-" SERE_PINNED_LLVM_VERSION) / "bin";
    if (isFile(pinned / "clang.exe")) {
      return pinned;
    }
  }
  const std::filesystem::path nextToCompiler = compilerDir / "llvm" / "bin";
  if (isFile(nextToCompiler / "clang.exe") || isFile(nextToCompiler / "clang")) {
    return nextToCompiler;
  }
  return std::nullopt;
}

std::optional<std::string> findClang() {
  std::vector<std::filesystem::path> candidates;
  if (const std::optional<std::filesystem::path> tools = llvmToolsDirectory()) {
    candidates.push_back(*tools / "clang.exe");
    candidates.push_back(*tools / "clang");
  }
  if (const std::optional<std::filesystem::path> found = firstExisting(candidates)) {
    return found->string();
  }
  const llvm::ErrorOr<std::string> fromPath = llvm::sys::findProgramByName("clang");
  if (fromPath) {
    return *fromPath;
  }
  return std::nullopt;
}

std::optional<std::string> findLld() {
  std::vector<std::filesystem::path> candidates;
  if (const std::optional<std::filesystem::path> tools = llvmToolsDirectory()) {
    candidates.push_back(*tools / "lld-link.exe");
    candidates.push_back(*tools / "ld.lld");
  }
  if (const std::optional<std::filesystem::path> found = firstExisting(candidates)) {
    return found->string();
  }
  const llvm::ErrorOr<std::string> fromPath = llvm::sys::findProgramByName("lld-link");
  if (fromPath) {
    return *fromPath;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> findNativeLibrary(std::string_view stem) {
  const std::filesystem::path directory = compilerDirectory();
  const std::filesystem::path parent = directory.parent_path();
  const std::string name(stem);
  std::vector<std::filesystem::path> candidates = {
      directory / (name + ".lib"),
      directory / (name + ".a"),
      directory / ("lib" + name + ".a"),
      directory / (name + ".dll.a"),
      parent / "lib" / (name + ".lib"),
      parent / "lib" / (name + ".a"),
      parent / "lib" / ("lib" + name + ".a"),
      parent / "runtime" / (name + ".lib"),
      parent / "runtime" / (name + ".a"),
      parent / "runtime" / ("lib" + name + ".a"),
  };
  if (const char* home = std::getenv("SERE_HOME")) {
    const std::filesystem::path homeDir(home);
    candidates.push_back(homeDir / (name + ".lib"));
    candidates.push_back(homeDir / (name + ".a"));
    candidates.push_back(homeDir / ("lib" + name + ".a"));
  }
  return firstExisting(candidates);
}

std::optional<std::filesystem::path> findRuntimeLibrary() { return findNativeLibrary("sere_rt"); }

void setEnvironmentVariable(std::string_view name, std::string_view value) {
  const std::string nameText(name);
  const std::string valueText(value);
#ifdef _WIN32
  _putenv_s(nameText.c_str(), valueText.c_str());
#else
  setenv(nameText.c_str(), valueText.c_str(), 1);
#endif
}

void prependToPath(const std::filesystem::path& directory) {
  const char* existing = std::getenv("PATH");
  std::string path = directory.string();
#ifdef _WIN32
  path += ';';
#else
  path += ':';
#endif
  if (existing != nullptr) {
    path += existing;
  }
  setEnvironmentVariable("PATH", path);
}

void prependLlvmToolsToPath() {
  const std::optional<std::filesystem::path> tools = llvmToolsDirectory();
  if (!tools.has_value()) {
    return;
  }
  prependToPath(*tools);
}

namespace {

#ifdef _WIN32

[[nodiscard]] std::filesystem::path programFilesX86() {
  if (const char* value = std::getenv("ProgramFiles(x86)")) {
    return value;
  }
  return "C:/Program Files (x86)";
}

[[nodiscard]] std::filesystem::path programFiles() {
  if (const char* value = std::getenv("ProgramFiles")) {
    return value;
  }
  return "C:/Program Files";
}

[[nodiscard]] std::optional<std::filesystem::path> newestChildWithFile(
    const std::filesystem::path& parent, const std::filesystem::path& relative) {
  std::error_code error;
  if (!std::filesystem::is_directory(parent, error)) {
    return std::nullopt;
  }
  std::filesystem::path best;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(parent, error)) {
    if (!entry.is_directory()) {
      continue;
    }
    if (!isFile(entry.path() / relative)) {
      continue;
    }
    if (best.empty() || entry.path().filename().string() > best.filename().string()) {
      best = entry.path();
    }
  }
  if (best.empty()) {
    return std::nullopt;
  }
  return best;
}

[[nodiscard]] std::vector<std::filesystem::path> windowsLibDirectories() {
  std::vector<std::filesystem::path> dirs;
  const std::filesystem::path vsRoots[] = {
      programFilesX86() / "Microsoft Visual Studio" / "2022" / "BuildTools",
      programFiles() / "Microsoft Visual Studio" / "2022" / "BuildTools",
      programFiles() / "Microsoft Visual Studio" / "2022" / "Community",
      programFilesX86() / "Microsoft Visual Studio" / "2022" / "Community",
      programFiles() / "Microsoft Visual Studio" / "2022" / "Professional",
      programFiles() / "Microsoft Visual Studio" / "2022" / "Enterprise",
      programFiles() / "Microsoft Visual Studio" / "2025" / "BuildTools",
      programFiles() / "Microsoft Visual Studio" / "2026" / "BuildTools",
      programFilesX86() / "Microsoft Visual Studio" / "2019" / "BuildTools",
  };
  for (const std::filesystem::path& root : vsRoots) {
    const auto version = newestChildWithFile(root / "VC" / "Tools" / "MSVC",
                                             std::filesystem::path("lib") / "x64" / "msvcprt.lib");
    if (!version.has_value()) {
      continue;
    }
    dirs.push_back(*version / "lib" / "x64");
    break;
  }
  const auto sdk = newestChildWithFile(programFilesX86() / "Windows Kits" / "10" / "Lib",
                                       std::filesystem::path("um") / "x64" / "user32.lib");
  if (sdk.has_value()) {
    dirs.push_back(*sdk / "um" / "x64");
    dirs.push_back(*sdk / "ucrt" / "x64");
  }
  return dirs;
}

#endif

}  // namespace

void applyHostLinkEnvironment() {
#ifdef _WIN32
  const std::vector<std::filesystem::path> dirs = windowsLibDirectories();
  if (dirs.empty()) {
    return;
  }
  std::string lib;
  for (const std::filesystem::path& dir : dirs) {
    if (!lib.empty()) {
      lib += ';';
    }
    lib += dir.string();
  }
  if (const char* existing = std::getenv("LIB")) {
    lib += ';';
    lib += existing;
  }
  setEnvironmentVariable("LIB", lib);
#endif
}

std::optional<std::filesystem::path> findSystemLibrary(std::string_view name) {
#ifdef _WIN32
  const std::string file(name);
  if (const char* lib = std::getenv("LIB")) {
    std::string_view rest(lib);
    while (!rest.empty()) {
      const std::size_t split = rest.find(';');
      const std::string_view part = split == std::string_view::npos ? rest : rest.substr(0, split);
      if (!part.empty()) {
        const std::filesystem::path candidate = std::filesystem::path(part) / file;
        if (isFile(candidate)) {
          return candidate;
        }
      }
      if (split == std::string_view::npos) {
        break;
      }
      rest = rest.substr(split + 1);
    }
  }
  for (const std::filesystem::path& dir : windowsLibDirectories()) {
    const std::filesystem::path candidate = dir / file;
    if (isFile(candidate)) {
      return candidate;
    }
  }
#else
  (void)name;
#endif
  return std::nullopt;
}

namespace {

bool copyFileOverwrite(const std::filesystem::path& from, const std::filesystem::path& to,
                       std::string& error) {
  std::error_code code;
  std::filesystem::create_directories(to.parent_path(), code);
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, code);
  if (code) {
    error = "could not copy " + from.string() + " -> " + to.string() + ": " + code.message();
    return false;
  }
  return true;
}

bool installRunningSafe(const std::filesystem::path& from, const std::filesystem::path& dest,
                        std::string& error) {
  if (!std::filesystem::exists(from)) {
    error = "missing " + from.string();
    return false;
  }
  const std::filesystem::path neu = std::filesystem::path(dest.string() + ".new");
  const std::filesystem::path old = std::filesystem::path(dest.string() + ".old");
  if (!copyFileOverwrite(from, neu, error)) {
    return false;
  }
  std::error_code code;
  std::filesystem::copy_file(neu, dest, std::filesystem::copy_options::overwrite_existing, code);
  if (!code) {
    std::filesystem::remove(neu, code);
    std::filesystem::remove(old, code);
    return true;
  }
  std::filesystem::remove(old, code);
  std::filesystem::rename(dest, old, code);
  if (code) {
    error = "queued " + neu.string() + " (run sere refresh-bin after closing the language server)";
    return false;
  }
  if (!copyFileOverwrite(neu, dest, error)) {
    return false;
  }
  std::filesystem::remove(neu, code);
  return true;
}

}  // namespace

int refreshCompilerBin(const std::filesystem::path& destBin, std::string& error) {
  const std::filesystem::path fromDir = compilerDirectory();
#ifdef _WIN32
  const std::filesystem::path exeName = "sere.exe";
#else
  const std::filesystem::path exeName = "sere";
#endif
  const std::filesystem::path dest =
      destBin.empty() ? (std::filesystem::current_path() / "bin") : destBin;
  std::error_code code;
  std::filesystem::create_directories(dest, code);
  if (!installRunningSafe(fromDir / exeName, dest / exeName, error)) {
    return 1;
  }
  if (const auto runtime = findRuntimeLibrary()) {
    const std::filesystem::path runtimeDest = dest / runtime->filename();
    (void)copyFileOverwrite(*runtime, runtimeDest, error);
  }
  const std::filesystem::path stdlibFrom = fromDir / "stdlib";
  const std::filesystem::path stdlibSrc =
      std::filesystem::exists(stdlibFrom) ? stdlibFrom
                                          : fromDir.parent_path().parent_path().parent_path() / "stdlib";
  if (std::filesystem::exists(stdlibSrc)) {
    std::filesystem::copy(stdlibSrc, dest / "stdlib",
                          std::filesystem::copy_options::overwrite_existing |
                              std::filesystem::copy_options::recursive,
                          code);
  }
  error.clear();
  std::cout << "updated " << (dest / exeName).string() << '\n';
  return 0;
}

}  // namespace sere
