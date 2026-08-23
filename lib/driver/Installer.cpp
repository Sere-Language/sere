/// @file Installer.cpp
/// Stages the Sere toolchain and compiles a Windows Inno Setup installer.

#include "sere/driver/Installer.h"

#include "sere/Version.h"
#include "sere/ToolchainPaths.h"
#include "sere/driver/Toolchain.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace sere {
namespace {

[[nodiscard]] bool isFile(const std::filesystem::path& path) {
  return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

[[nodiscard]] bool isDir(const std::filesystem::path& path) {
  return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

void replaceAll(std::string& text, std::string_view from, std::string_view to) {
  std::size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

[[nodiscard]] std::string slashPath(const std::filesystem::path& path) {
  std::string text = path.lexically_normal().string();
  replaceAll(text, "/", "\\");
  return text;
}

[[nodiscard]] bool writeText(const std::filesystem::path& path, std::string_view text,
                             std::string& error) {
  std::error_code fsError;
  std::filesystem::create_directories(path.parent_path(), fsError);
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    error = "cannot write '" + path.string() + "'";
    return false;
  }
  output << text;
  return static_cast<bool>(output);
}

[[nodiscard]] std::optional<std::string> readText(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

[[nodiscard]] bool copyFileTo(const std::filesystem::path& from, const std::filesystem::path& to,
                              std::string& error) {
  std::error_code fsError;
  std::filesystem::create_directories(to.parent_path(), fsError);
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, fsError);
  if (fsError) {
    error = "cannot copy '" + from.string() + "' to '" + to.string() + "': " + fsError.message();
    return false;
  }
  return true;
}

[[nodiscard]] int runProgram(const std::string& program, const std::vector<std::string>& args) {
  llvm::SmallVector<llvm::StringRef, 16> argv;
  argv.push_back(program);
  for (const std::string& argument : args) {
    argv.push_back(argument);
  }
  return llvm::sys::ExecuteAndWait(program, argv);
}

[[nodiscard]] bool copyTree(const std::filesystem::path& from, const std::filesystem::path& to,
                            std::string& error) {
  if (!isDir(from)) {
    error = "missing directory '" + from.string() + "'";
    return false;
  }
  std::error_code fsError;
#ifdef _WIN32
  const llvm::ErrorOr<std::string> robocopy = llvm::sys::findProgramByName("robocopy");
  if (robocopy) {
    std::filesystem::create_directories(to, fsError);
    const int code = runProgram(*robocopy, {from.string(), to.string(), "/E", "/COPY:DAT", "/R:1",
                                            "/W:1", "/NFL", "/NDL", "/NJH", "/NJS", "/NP"});
    if (code <= 7) {
      return true;
    }
    error = "robocopy failed copying '" + from.string() + "' (exit " + std::to_string(code) + ")";
    return false;
  }
#endif
  std::filesystem::copy(from, to,
                        std::filesystem::copy_options::recursive |
                            std::filesystem::copy_options::overwrite_existing,
                        fsError);
  if (fsError) {
    error = "cannot copy '" + from.string() + "': " + fsError.message();
    return false;
  }
  return true;
}

[[nodiscard]] std::optional<std::filesystem::path> firstExistingFile(
    const std::vector<std::filesystem::path>& candidates) {
  for (const std::filesystem::path& candidate : candidates) {
    if (isFile(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::filesystem::path walkForFile(const std::filesystem::path& start,
                                                const std::filesystem::path& relative) {
  std::filesystem::path current = start;
  for (int depth = 0; depth < 8; ++depth) {
    if (isFile(current / relative)) {
      return current;
    }
    const std::filesystem::path parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return {};
}

[[nodiscard]] std::filesystem::path findSourceRoot() {
  const std::filesystem::path packaged = std::filesystem::path(SERE_SOURCE_DIR) / "packaging" /
                                         "sere.iss.in";
  if (isFile(packaged)) {
    return SERE_SOURCE_DIR;
  }
  const std::filesystem::path fromCwd =
      walkForFile(std::filesystem::current_path(), std::filesystem::path("packaging") / "sere.iss.in");
  if (!fromCwd.empty()) {
    return fromCwd;
  }
  return walkForFile(compilerDirectory(), std::filesystem::path("packaging") / "sere.iss.in");
}

[[nodiscard]] std::optional<std::filesystem::path> findLlvmRoot() {
  if (const std::optional<std::filesystem::path> tools = llvmToolsDirectory()) {
    const std::filesystem::path root = tools->parent_path();
    if (isFile(root / "bin" / "clang.exe") || isFile(root / "bin" / "clang")) {
      return root;
    }
  }
  if (const char* fromEnv = std::getenv("SERE_LLVM_DIR")) {
    const std::filesystem::path root(fromEnv);
    if (isFile(root / "bin" / "clang.exe")) {
      return root;
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path> findIscc() {
  std::vector<std::filesystem::path> candidates;
  if (const char* fromEnv = std::getenv("ISCC")) {
    candidates.emplace_back(fromEnv);
  }
  if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
    candidates.push_back(std::filesystem::path(localAppData) / "Programs" / "Inno Setup 6" /
                         "ISCC.exe");
  }
  candidates.emplace_back("C:/Program Files (x86)/Inno Setup 6/ISCC.exe");
  candidates.emplace_back("C:/Program Files/Inno Setup 6/ISCC.exe");
  if (const std::optional<std::filesystem::path> found = firstExistingFile(candidates)) {
    return found;
  }
  const llvm::ErrorOr<std::string> fromPath = llvm::sys::findProgramByName("ISCC");
  if (fromPath) {
    return std::filesystem::path(*fromPath);
  }
  return std::nullopt;
}

[[nodiscard]] std::filesystem::path defaultOutputPath(const std::filesystem::path& sourceRoot) {
  const std::filesystem::path dist = sourceRoot.empty() ? std::filesystem::path("dist")
                                                        : sourceRoot / "dist";
  return dist / ("Sere-" + std::string(SERE_VERSION_STRING) + "-setup.exe");
}

[[nodiscard]] bool copyIfExists(const std::filesystem::path& from, const std::filesystem::path& to,
                                std::string& error) {
  if (!isFile(from)) {
    return true;
  }
  return copyFileTo(from, to, error);
}

[[nodiscard]] bool packageVsix(const std::filesystem::path& sourceRoot,
                               const std::filesystem::path& destVsix, std::string& error) {
  std::error_code fsError;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(sourceRoot / "editors" / "vscode", fsError)) {
    if (entry.path().extension() == ".vsix") {
      return copyFileTo(entry.path(), destVsix, error);
    }
  }
  const std::filesystem::path script = sourceRoot / "scripts" / "package-vsix.ps1";
  if (!isFile(script)) {
    error = "missing " + script.string();
    return false;
  }
  const llvm::ErrorOr<std::string> powershell = llvm::sys::findProgramByName("powershell");
  if (!powershell) {
    error = "powershell not found; cannot package the editor extension";
    return false;
  }
  llvm::outs() << "packaging editor VSIX\n";
  const int code = runProgram(*powershell, {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                                            script.string()});
  if (code != 0) {
    error = "scripts/package-vsix.ps1 failed with exit " + std::to_string(code);
    return false;
  }
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(sourceRoot / "editors" / "vscode", fsError)) {
    if (entry.path().extension() == ".vsix") {
      return copyFileTo(entry.path(), destVsix, error);
    }
  }
  error = "package-vsix.ps1 did not write a .vsix";
  return false;
}

[[nodiscard]] bool stagePayload(const std::filesystem::path& payload,
                                const std::filesystem::path& sourceRoot, bool& hasQt, bool& hasVsix,
                                std::string& error) {
  const std::filesystem::path compilerDir = compilerDirectory();
  std::error_code fsError;
  std::filesystem::create_directories(payload / "bin", fsError);
  std::filesystem::create_directories(payload / "include" / "sere" / "api", fsError);
  std::filesystem::create_directories(payload / "packaging", fsError);
  std::filesystem::create_directories(payload / "editors", fsError);

  const std::filesystem::path exe = compilerDir /
#ifdef _WIN32
                                    "sere.exe"
#else
                                    "sere"
#endif
      ;
  if (!isFile(exe)) {
    error = "sere executable not found next to the running compiler";
    return false;
  }
  if (!copyFileTo(exe, payload / "bin" / exe.filename(), error)) {
    return false;
  }

  const std::optional<std::filesystem::path> runtime = findRuntimeLibrary();
  if (!runtime.has_value()) {
    error = "sere_rt library not found next to the compiler";
    return false;
  }
  if (!copyFileTo(*runtime, payload / "bin" / runtime->filename(), error)) {
    return false;
  }

  const std::filesystem::path stdlib = isDir(compilerDir / "stdlib") ? compilerDir / "stdlib"
                                                                     : sourceRoot / "stdlib";
  if (!copyTree(stdlib, payload / "stdlib", error)) {
    return false;
  }

  const std::filesystem::path apiDir = isFile(compilerDir / "include" / "sere" / "api" / "sere_mod.h")
                                           ? compilerDir / "include" / "sere" / "api"
                                           : sourceRoot / "include" / "sere" / "api";
  if (!copyFileTo(apiDir / "sere_mod.h", payload / "include" / "sere" / "api" / "sere_mod.h",
                  error) ||
      !copyFileTo(apiDir / "sere_gc.h", payload / "include" / "sere" / "api" / "sere_gc.h", error)) {
    return false;
  }

  if (!copyFileTo(sourceRoot / "packaging" / "install-vsix.ps1",
                  payload / "packaging" / "install-vsix.ps1", error) ||
      !copyFileTo(sourceRoot / "packaging" / "ensure-msvc.ps1",
                  payload / "packaging" / "ensure-msvc.ps1", error)) {
    return false;
  }

  const std::optional<std::filesystem::path> llvmRoot = findLlvmRoot();
  if (!llvmRoot.has_value()) {
    error = "LLVM " SERE_PINNED_LLVM_VERSION
            " not found. Run scripts/bootstrap.ps1 before --build-installer.";
    return false;
  }
  llvm::outs() << "copying LLVM " << SERE_PINNED_LLVM_VERSION << " from " << llvmRoot->string()
               << " (this may take a while)\n";
  if (!copyTree(*llvmRoot, payload / "toolchains" / ("llvm-" SERE_PINNED_LLVM_VERSION), error)) {
    return false;
  }

  hasQt = false;
  const std::filesystem::path qtDir = payload / "qt6";
  if (copyIfExists(compilerDir / "sere_qt6.dll", qtDir / "sere_qt6.dll", error) &&
      isFile(qtDir / "sere_qt6.dll")) {
    hasQt = true;
    if (!copyIfExists(compilerDir / "sere_qt6.lib", payload / "bin" / "sere_qt6.lib", error) ||
        !copyIfExists(compilerDir / "Qt6Core.dll", qtDir / "Qt6Core.dll", error) ||
        !copyIfExists(compilerDir / "Qt6Gui.dll", qtDir / "Qt6Gui.dll", error) ||
        !copyIfExists(compilerDir / "Qt6Widgets.dll", qtDir / "Qt6Widgets.dll", error)) {
      return false;
    }
    if (isDir(compilerDir / "platforms") &&
        !copyTree(compilerDir / "platforms", qtDir / "platforms", error)) {
      return false;
    }
  }

  hasVsix = packageVsix(sourceRoot, payload / "editors" / "sere.vsix", error);
  if (!hasVsix) {
    llvm::errs() << "warning: editor VSIX not packaged: " << error << '\n';
    error.clear();
  }
  return true;
}

[[nodiscard]] bool writeIss(const std::filesystem::path& sourceRoot,
                            const std::filesystem::path& payload, const std::filesystem::path& issPath,
                            const std::filesystem::path& output, bool hasQt, bool hasVsix,
                            std::string& error) {
  const std::optional<std::string> body = readText(sourceRoot / "packaging" / "sere.iss.in");
  if (!body.has_value()) {
    error = "missing packaging/sere.iss.in";
    return false;
  }
  std::string text = *body;
  replaceAll(text, "@SERE_VERSION@", SERE_VERSION_STRING);
  replaceAll(text, "@PAYLOAD_DIR@", slashPath(payload));
  replaceAll(text, "@OUTPUT_DIR@", slashPath(output.parent_path()));
  replaceAll(text, "@OUTPUT_BASE@", output.stem().string());
  replaceAll(text, "@HAS_QT@", hasQt ? "1" : "0");
  replaceAll(text, "@HAS_VSIX@", hasVsix ? "1" : "0");
  replaceAll(text, "@LLVM_VERSION@", SERE_PINNED_LLVM_VERSION);
  return writeText(issPath, text, error);
}

}  // namespace

int buildInstaller(const CompilerOptions& options) {
#ifndef _WIN32
  llvm::errs() << "error: sere --build-installer is only supported on Windows\n";
  return 1;
#else
  const std::filesystem::path sourceRoot = findSourceRoot();
  if (sourceRoot.empty()) {
    llvm::errs() << "error: cannot find packaging/sere.iss.in (build from the Sere source tree)\n";
    return 1;
  }
  const std::optional<std::filesystem::path> iscc = findIscc();
  if (!iscc.has_value()) {
    llvm::errs() << "error: Inno Setup compiler (ISCC.exe) not found.\n"
                 << "  Install it with:  winget install --id JRSoftware.InnoSetup -e\n"
                 << "  or run:           .\\scripts\\bootstrap-innosetup.ps1\n";
    return 1;
  }

  const std::filesystem::path output =
      options.outputPath.empty() ? defaultOutputPath(sourceRoot) : options.outputPath;
  std::error_code fsError;
  std::filesystem::create_directories(output.parent_path(), fsError);
  const std::filesystem::path stage = output.parent_path() / "installer-stage";
  std::filesystem::remove_all(stage, fsError);
  const std::filesystem::path payload = stage / "payload";

  llvm::outs() << "staging Sere " << SERE_VERSION_STRING << " installer payload\n";
  std::string error;
  bool hasQt = false;
  bool hasVsix = false;
  if (!stagePayload(payload, sourceRoot, hasQt, hasVsix, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  const std::filesystem::path issPath = stage / "sere.iss";
  if (!writeIss(sourceRoot, payload, issPath, output, hasQt, hasVsix, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  llvm::outs() << "compiling installer with " << iscc->string() << '\n';
  const int code = runProgram(iscc->string(), {issPath.string()});
  if (code != 0) {
    llvm::errs() << "error: ISCC failed with exit code " << code << '\n';
    return code == 0 ? 1 : code;
  }
  llvm::outs() << "wrote " << output.string() << '\n';
  return 0;
#endif
}

}  // namespace sere
