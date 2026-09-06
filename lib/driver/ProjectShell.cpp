/// @file ProjectShell.cpp
/// Spawns a nested system shell with the project compiler on PATH.

#include "sere/driver/ProjectShell.h"

#include "sere/Version.h"
#include "sere/driver/Prelude.h"
#include "sere/driver/Project.h"
#include "sere/driver/ProjectInit.h"
#include "sere/driver/Toolchain.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace sere {
namespace {

[[nodiscard]] std::string lowerCopy(std::string text) {
  for (char& ch : text) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return text;
}

[[nodiscard]] std::string detectHost(std::string_view requested) {
  const std::string host = lowerCopy(std::string(requested));
  if (host == "powershell" || host == "pwsh" || host == "cmd" || host == "bash") {
    return host == "pwsh" ? "powershell" : host;
  }
  if (std::getenv("MSYSTEM") != nullptr) {
    return "bash";
  }
  if (const char* shell = std::getenv("SHELL")) {
    const std::string value = lowerCopy(shell);
    if (value.find("bash") != std::string::npos || value.find("zsh") != std::string::npos) {
      return "bash";
    }
  }
  if (std::getenv("PSModulePath") != nullptr) {
    return "powershell";
  }
#ifdef _WIN32
  return "cmd";
#else
  return "bash";
#endif
}

[[nodiscard]] std::optional<std::string> findProgram(const char* name) {
  const llvm::ErrorOr<std::string> found = llvm::sys::findProgramByName(name);
  if (found) {
    return *found;
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<std::string> findPowerShell() {
  if (const std::optional<std::string> pwsh = findProgram("pwsh")) {
    return pwsh;
  }
  if (const std::optional<std::string> powershell = findProgram("powershell")) {
    return powershell;
  }
#ifdef _WIN32
  return std::string("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
#else
  return std::nullopt;
#endif
}

[[nodiscard]] std::optional<std::string> findCmd() {
  if (const char* comspec = std::getenv("ComSpec")) {
    return std::string(comspec);
  }
  return findProgram("cmd.exe");
}

[[nodiscard]] std::optional<std::string> findBash() {
  if (const char* shell = std::getenv("SHELL")) {
    const std::string value(shell);
    if (value.find("bash") != std::string::npos) {
      return value;
    }
  }
  return findProgram("bash");
}

[[nodiscard]] int waitFor(const std::string& program, const std::vector<std::string>& args) {
  llvm::SmallVector<llvm::StringRef, 16> refs;
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

void exportProjectEnv(const ProjectManifest& manifest) {
  const std::filesystem::path venvBin = manifest.root / "venv" / "bin";
  prepareProjectStdlib(manifest);
  setEnvironmentVariable("SERE_ACTIVE", "1");
  setEnvironmentVariable("SERE_PROJECT_ROOT", manifest.root.string());
  setEnvironmentVariable("SERE_PROJECT_NAME", manifest.name);
  setEnvironmentVariable("SERE_VENV_BIN", venvBin.string());
  if (std::filesystem::exists(manifest.stdlib / "prelude.sere")) {
    setEnvironmentVariable("SERE_STDLIB", manifest.stdlib.string());
  } else {
    setEnvironmentVariable("SERE_STDLIB", findStdlibDirectory(compilerDirectory()).string());
  }
  prependToPath(venvBin);
  prependToPath(compilerDirectory());
}

void printBanner(const ProjectManifest& manifest) {
  std::cout << "sere " << SERE_VERSION_STRING << "  " << manifest.name << '\n';
  std::cout << "  nested shell  your original terminal is still open\n";
  std::cout << "  sere build    compile " << manifest.entry.filename().string() << '\n';
  std::cout << "  sere run      build and run\n";
  std::cout << "  sere clean    remove bin artifacts\n";
  std::cout << "  deactivate    close this nested shell (or type exit)\n";
}

[[nodiscard]] int spawnPowerShell(const ProjectManifest& manifest) {
  const std::optional<std::string> exe = findPowerShell();
  if (!exe.has_value()) {
    llvm::errs() << "error: powershell not found\n";
    return 1;
  }
  const std::filesystem::path rc = manifest.root / "venv" / "shell.ps1";
  std::vector<std::string> args{
      *exe, "-NoProfile", "-NoLogo", "-NoExit", "-ExecutionPolicy", "Bypass"};
  if (std::filesystem::exists(rc)) {
    args.insert(args.end(), {"-File", rc.string()});
  } else {
    args.insert(args.end(),
                {"-Command",
                 "function prompt { '(sere:' + $env:SERE_PROJECT_NAME + ') ' + "
                 "(Get-Location).Path + '> ' }; function deactivate { exit }"});
  }
  return waitFor(*exe, args);
}

[[nodiscard]] int spawnCmd(const ProjectManifest& manifest) {
  const std::optional<std::string> exe = findCmd();
  if (!exe.has_value()) {
    llvm::errs() << "error: cmd.exe not found\n";
    return 1;
  }
  const std::filesystem::path rc = manifest.root / "venv" / "shell.cmd";
  std::vector<std::string> args{*exe, "/K"};
  if (std::filesystem::exists(rc)) {
    args.push_back(rc.string());
  } else {
    args.push_back("prompt (sere: " + manifest.name + ") $P$G && doskey deactivate=exit");
  }
  return waitFor(*exe, args);
}

[[nodiscard]] int spawnBash(const ProjectManifest& manifest) {
  const std::optional<std::string> exe = findBash();
  if (!exe.has_value()) {
    llvm::errs() << "error: bash not found\n";
    return 1;
  }
  const std::filesystem::path rc = manifest.root / "venv" / "shell.bash";
  std::vector<std::string> args{*exe, "-i"};
  if (std::filesystem::exists(rc)) {
    args = {*exe, "--rcfile", rc.string(), "-i"};
  }
  return waitFor(*exe, args);
}

} // namespace

int enterProjectShell(const CompilerOptions& options) {
  if (std::getenv("SERE_ACTIVE") != nullptr) {
    std::cout << "already in a Sere shell for " << std::getenv("SERE_PROJECT_NAME") << '\n';
    return 0;
  }
  std::string error;
  const std::optional<std::filesystem::path> root =
      findProjectRoot(std::filesystem::current_path());
  if (!root.has_value()) {
    llvm::errs() << "error: no sere.toml found; run scripts/activate from a Sere project\n";
    return 1;
  }
  ProjectManifest manifest;
  if (!loadProjectManifest(*root, manifest, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  std::filesystem::current_path(manifest.root);
  writeProjectShellRc(manifest.root);
  exportProjectEnv(manifest);
  printBanner(manifest);
  const std::string host = detectHost(options.shellHost);
  if (host == "powershell") {
    return spawnPowerShell(manifest);
  }
  if (host == "cmd") {
    return spawnCmd(manifest);
  }
  return spawnBash(manifest);
}

} // namespace sere
