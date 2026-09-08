/// @file project_cli.cpp
/// Checks sere.toml loading, init scaffolding, and project subcommand parsing.

#include "sere/Version.h"
#include "sere/driver/Options.h"
#include "sere/driver/Project.h"
#include "sere/driver/ProjectInit.h"
#include "sere/driver/Toolchain.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << "project_cli: " << message << '\n';
  return 1;
}

[[nodiscard]] bool writeAll(const std::filesystem::path& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  output << text;
  return static_cast<bool>(output);
}

[[nodiscard]] bool pathSame(const std::filesystem::path& left, const std::filesystem::path& right) {
  std::error_code leftError;
  std::error_code rightError;
  return std::filesystem::weakly_canonical(left, leftError) ==
             std::filesystem::weakly_canonical(right, rightError) &&
         !leftError && !rightError;
}

[[nodiscard]] bool containsPath(const std::vector<std::filesystem::path>& dirs,
                                const std::filesystem::path& wanted) {
  for (const std::filesystem::path& directory : dirs) {
    if (pathSame(directory, wanted)) {
      return true;
    }
  }
  return false;
}

struct SavedEnv {
  std::string stdlib;
  std::string active;
  std::string root;
};

[[nodiscard]] SavedEnv captureSereEnv() {
  SavedEnv saved;
  if (const char* value = std::getenv("SERE_STDLIB")) {
    saved.stdlib = value;
  }
  if (const char* value = std::getenv("SERE_ACTIVE")) {
    saved.active = value;
  }
  if (const char* value = std::getenv("SERE_PROJECT_ROOT")) {
    saved.root = value;
  }
  return saved;
}

void restoreSereEnv(const SavedEnv& saved) {
  sere::setEnvironmentVariable("SERE_STDLIB", saved.stdlib);
  sere::setEnvironmentVariable("SERE_ACTIVE", saved.active);
  sere::setEnvironmentVariable("SERE_PROJECT_ROOT", saved.root);
}

class EnvScope {
public:
  EnvScope() : previous_(captureSereEnv()) {}
  ~EnvScope() { restoreSereEnv(previous_); }
  EnvScope(const EnvScope&) = delete;
  EnvScope& operator=(const EnvScope&) = delete;

private:
  SavedEnv previous_;
};

int checkProjectStdlibWins(const std::filesystem::path& project,
                           const std::filesystem::path& otherStdlib) {
  sere::setEnvironmentVariable("SERE_ACTIVE", "");
  sere::setEnvironmentVariable("SERE_PROJECT_ROOT", "");
  sere::setEnvironmentVariable("SERE_STDLIB", otherStdlib.string());
  const sere::LanguageContext fromFile =
      sere::resolveLanguageContext(project / "src" / "main.sere");
  if (!fromFile.project.has_value() || !pathSame(fromFile.project->root, project)) {
    return fail("resolveLanguageContext missed the init project");
  }
  if (!pathSame(fromFile.stdlib, project / "venv" / "stdlib")) {
    return fail("init project should use venv/stdlib over a stray SERE_STDLIB");
  }
  std::vector<std::filesystem::path> dirs;
  sere::appendLanguageContextDirs(dirs, fromFile);
  if (!containsPath(dirs, project / "libs") || !containsPath(dirs, project / "src") ||
      !containsPath(dirs, project)) {
    return fail("appendLanguageContextDirs should add libs, src, and root");
  }
  return 0;
}

int checkEmptyStdlibEnvFallsBack(const std::filesystem::path& project) {
  sere::setEnvironmentVariable("SERE_ACTIVE", "1");
  sere::setEnvironmentVariable("SERE_PROJECT_ROOT", project.string());
  sere::setEnvironmentVariable("SERE_STDLIB", (project / "venv" / "missing-stdlib").string());
  const sere::LanguageContext context =
      sere::resolveLanguageContext(project / "src" / "main.sere");
  if (!pathSame(context.stdlib, project / "venv" / "stdlib")) {
    return fail("empty SERE_STDLIB should fall back to the project venv stdlib");
  }
  return 0;
}

int checkActivatedShellStdlib(const std::filesystem::path& project,
                              const std::filesystem::path& otherStdlib) {
  sere::setEnvironmentVariable("SERE_ACTIVE", "1");
  sere::setEnvironmentVariable("SERE_PROJECT_ROOT", project.string());
  sere::setEnvironmentVariable("SERE_STDLIB", otherStdlib.string());
  const sere::LanguageContext fromShell =
      sere::resolveLanguageContext(project / "src" / "main.sere");
  if (!pathSame(fromShell.stdlib, otherStdlib)) {
    return fail("activated SERE_STDLIB should win when SERE_PROJECT_ROOT matches");
  }
  return 0;
}

int checkLanguageContext(const std::filesystem::path& project,
                         const std::filesystem::path& temp) {
  const EnvScope restoreEnv;
  const std::filesystem::path otherStdlib = temp / "other-stdlib";
  std::error_code fsError;
  std::filesystem::create_directories(project / "venv" / "stdlib", fsError);
  std::filesystem::create_directories(otherStdlib, fsError);
  if (!writeAll(project / "venv" / "stdlib" / "prelude.sere",
                "def _prelude() -> i32:\n    return 0\n") ||
      !writeAll(otherStdlib / "prelude.sere", "def _other() -> i32:\n    return 0\n")) {
    return fail("could not write test stdlib files");
  }
  if (const int code = checkProjectStdlibWins(project, otherStdlib); code != 0) {
    return code;
  }
  if (const int code = checkEmptyStdlibEnvFallsBack(project); code != 0) {
    return code;
  }
  return checkActivatedShellStdlib(project, otherStdlib);
}

[[nodiscard]] bool parseArgs(const std::vector<std::string>& args, sere::CompilerOptions& options,
                             std::string& error) {
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (const std::string& argument : args) {
    argv.push_back(const_cast<char*>(argument.c_str()));
  }
  return sere::parseCommandLine(static_cast<int>(argv.size()), argv.data(), options, error);
}

[[nodiscard]] std::string readAll(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  return text;
}

}  // namespace

int main() {
  std::string error;
  sere::CompilerOptions buildOptions;
  if (!parseArgs({"sere", "build"}, buildOptions, error)) {
    return fail("failed to parse 'sere build'");
  }
  if (buildOptions.projectCommand != sere::ProjectCommand::Build) {
    return fail("expected ProjectCommand::Build");
  }

  sere::CompilerOptions runOptions;
  if (!parseArgs({"sere", "run", "--", "a", "b"}, runOptions, error)) {
    return fail("failed to parse 'sere run'");
  }
  if (runOptions.projectCommand != sere::ProjectCommand::Run ||
      runOptions.programArgs.size() != 2) {
    return fail("expected run args a b");
  }

  sere::CompilerOptions asmOptions;
  if (!parseArgs({"sere", "--emit-asm", "main.sere", "-o", "main.s"}, asmOptions, error)) {
    return fail("failed to parse --emit-asm");
  }
  if (!asmOptions.emitAsm || asmOptions.emitLlvm) {
    return fail("expected emitAsm from --emit-asm");
  }
  sere::CompilerOptions asmAlias;
  if (!parseArgs({"sere", "-S", "main.sere"}, asmAlias, error)) {
    return fail("failed to parse -S");
  }
  if (!asmAlias.emitAsm) {
    return fail("expected emitAsm from -S");
  }
  sere::CompilerOptions bothEmit;
  if (parseArgs({"sere", "--emit-llvm", "--emit-asm", "main.sere"}, bothEmit, error)) {
    return fail("expected --emit-llvm and --emit-asm together to fail");
  }

  sere::CompilerOptions dumpAst;
  if (!parseArgs({"sere", "--dump-ast", "main.sere"}, dumpAst, error)) {
    return fail("failed to parse --dump-ast");
  }
  if (!dumpAst.dumpAst || dumpAst.emitLlvm || dumpAst.emitAsm) {
    return fail("expected dumpAst from --dump-ast");
  }
  sere::CompilerOptions dumpSymbols;
  if (!parseArgs({"sere", "--dump-symbols", "main.sere"}, dumpSymbols, error)) {
    return fail("failed to parse --dump-symbols");
  }
  if (!dumpSymbols.dumpSymbols || dumpSymbols.emitLlvm || dumpSymbols.emitAsm) {
    return fail("expected dumpSymbols from --dump-symbols");
  }
  sere::CompilerOptions dumpLlvmIrRaw;
  if (!parseArgs({"sere", "--dump-llvm-ir-raw", "main.sere"}, dumpLlvmIrRaw, error)) {
    return fail("failed to parse --dump-llvm-ir-raw");
  }
  if (!dumpLlvmIrRaw.dumpLlvmIrRaw || dumpLlvmIrRaw.emitLlvm || dumpLlvmIrRaw.emitAsm) {
    return fail("expected dumpLlvmIrRaw from --dump-llvm-ir-raw");
  }

  sere::CompilerOptions shellOptions;
  if (!parseArgs({"sere", "shell", "--host", "powershell"}, shellOptions, error)) {
    return fail("failed to parse 'sere shell'");
  }
  if (shellOptions.projectCommand != sere::ProjectCommand::Shell ||
      shellOptions.shellHost != "powershell") {
    return fail("expected shell --host powershell");
  }

  sere::CompilerOptions installerOptions;
  if (!parseArgs({"sere", "--build-installer", "-o", "dist/Sere-setup.exe"}, installerOptions,
                 error)) {
    return fail("failed to parse --build-installer");
  }
  if (installerOptions.projectCommand != sere::ProjectCommand::BuildInstaller ||
      installerOptions.outputPath.generic_string() != "dist/Sere-setup.exe") {
    return fail("expected BuildInstaller with -o dist/Sere-setup.exe");
  }
  sere::CompilerOptions installerAlias;
  if (!parseArgs({"sere", "build-installer"}, installerAlias, error)) {
    return fail("failed to parse build-installer");
  }
  if (installerAlias.projectCommand != sere::ProjectCommand::BuildInstaller) {
    return fail("expected ProjectCommand::BuildInstaller from build-installer");
  }

  sere::CompilerOptions refreshOptions;
  if (!parseArgs({"sere", "refresh-bin"}, refreshOptions, error)) {
    return fail("failed to parse refresh-bin");
  }
  if (refreshOptions.projectCommand != sere::ProjectCommand::RefreshBin) {
    return fail("expected ProjectCommand::RefreshBin from refresh-bin");
  }
  sere::CompilerOptions refreshFlag;
  if (!parseArgs({"sere", "--refresh-bin"}, refreshFlag, error)) {
    return fail("failed to parse --refresh-bin");
  }
  if (refreshFlag.projectCommand != sere::ProjectCommand::RefreshBin) {
    return fail("expected ProjectCommand::RefreshBin from --refresh-bin");
  }

  sere::CompilerOptions updateOptions;
  if (!parseArgs({"sere", "update"}, updateOptions, error)) {
    return fail("failed to parse update");
  }
  if (updateOptions.projectCommand != sere::ProjectCommand::Update) {
    return fail("expected ProjectCommand::Update from update");
  }
  sere::CompilerOptions updateFlag;
  sere::CompilerOptions localUpdateOptions;
  if (!parseArgs({"sere", "update-local"}, localUpdateOptions, error) ||
      localUpdateOptions.projectCommand != sere::ProjectCommand::UpdateLocal) {
    return fail("failed to parse update-local");
  }
  if (!parseArgs({"sere", "--update"}, updateFlag, error)) {
    return fail("failed to parse --update");
  }
  if (updateFlag.projectCommand != sere::ProjectCommand::Update) {
    return fail("expected ProjectCommand::Update from --update");
  }

  sere::CompilerOptions initLib;
  if (!parseArgs({"sere", "init-lib", "mathlib"}, initLib, error)) {
    return fail("failed to parse init-lib");
  }
  if (initLib.projectCommand != sere::ProjectCommand::InitLib ||
      initLib.initName.string() != "mathlib") {
    return fail("expected InitLib mathlib");
  }
  sere::CompilerOptions initLibFlag;
  if (!parseArgs({"sere", "--init-lib", "mathlib"}, initLibFlag, error)) {
    return fail("failed to parse --init-lib");
  }
  if (initLibFlag.projectCommand != sere::ProjectCommand::InitLib) {
    return fail("expected InitLib from --init-lib");
  }
  sere::CompilerOptions initWithLib;
  if (!parseArgs({"sere", "init", "mathlib", "--lib"}, initWithLib, error)) {
    return fail("failed to parse init --lib");
  }
  if (initWithLib.projectCommand != sere::ProjectCommand::InitLib ||
      initWithLib.initName.string() != "mathlib") {
    return fail("expected init --lib to become InitLib mathlib");
  }
  sere::CompilerOptions packCmd;
  if (!parseArgs({"sere", "pack", "src/lib.sere", "-o", "dist/mathlib.slib"}, packCmd, error)) {
    return fail("failed to parse pack");
  }
  if (packCmd.projectCommand != sere::ProjectCommand::Pack ||
      packCmd.inputPath.generic_string() != "src/lib.sere" ||
      packCmd.outputPath.generic_string() != "dist/mathlib.slib") {
    return fail("expected pack src/lib.sere -o dist/mathlib.slib");
  }
  sere::CompilerOptions packFlag;
  if (!parseArgs({"sere", "--pack"}, packFlag, error)) {
    return fail("failed to parse --pack");
  }
  if (packFlag.projectCommand != sere::ProjectCommand::Pack) {
    return fail("expected Pack from --pack");
  }

  const std::filesystem::path temp =
      std::filesystem::temp_directory_path() / "sere-project-cli-test";
  std::error_code fsError;
  std::filesystem::remove_all(temp, fsError);
  std::filesystem::create_directories(temp / "app", fsError);
  std::filesystem::create_directories(temp / "stdlib" / "stdlib", fsError);
  std::filesystem::create_directories(temp / "stdlib" / "package", fsError);
  (void)writeAll(temp / "stdlib" / "prelude.sere", "# prelude\n");
  (void)writeAll(temp / "stdlib" / "stdlib" / "prelude.sere", "# legacy duplicate\n");
  (void)writeAll(temp / "stdlib" / "package" / "module.sere", "# nested module\n");
  const std::filesystem::path project = temp / "demo";
  if (sere::initSereProject(project, temp, error) != 0) {
    std::cerr << error << '\n';
    return fail("initSereProject failed");
  }
  if (!std::filesystem::exists(project / "scripts" / "activate") ||
      !std::filesystem::exists(project / "scripts" / "activate.ps1") ||
      !std::filesystem::exists(project / "scripts" / "activate.bat") ||
      !std::filesystem::exists(project / "bin" / "sere-path.ps1") ||
      !std::filesystem::exists(project / "bin" / "sere-path.cmd") ||
      !std::filesystem::exists(project / "venv" / "shell.ps1") ||
      !std::filesystem::exists(project / "sere.toml") ||
      !std::filesystem::exists(project / "src" / "main.sere")) {
    return fail("init did not write expected files");
  }
  sere::copyProjectToolchain(temp, project);
  if (!std::filesystem::exists(project / "venv/stdlib/prelude.sere") ||
      !std::filesystem::exists(project / "venv/stdlib/package/module.sere") ||
      std::filesystem::exists(project / "venv/stdlib/stdlib")) {
    return fail("stdlib contents must be copied once, preserving nested modules");
  }
  const auto generated = readAll(project / "sere.toml");
  for (const char* section : {"[project]", "[toolchain]", "[paths]", "[build]"}) {
    if (generated.find(section) == std::string::npos) return fail("missing manifest section");
  }
  (void)writeAll(project / "sere.toml", generated + "\n[tool.example]\nname = \"ignored\"\nopt = \"O3\"\n");
  const std::string activate = readAll(project / "scripts" / "activate.ps1");
  if (activate.find("SERE_PROJECT_ROOT") == std::string::npos ||
      activate.find("deactivate") == std::string::npos) {
    return fail("activate.ps1 is not an in-process project activate");
  }
  const std::string shell = readAll(project / "venv" / "shell.ps1");
  if (shell.find(". $PROFILE") != std::string::npos ||
      shell.find("Test-Path $PROFILE") != std::string::npos) {
    return fail("shell.ps1 must not source the user profile");
  }
  sere::ProjectManifest manifest;
  if (!sere::loadProjectManifest(project, manifest, error)) {
    return fail("loadProjectManifest failed");
  }
  if (manifest.name != "demo") {
    return fail("manifest name should be demo");
  }
  if (manifest.entry.filename() != "main.sere") {
    return fail("manifest entry should be main.sere");
  }
  if (const int contextCode = checkLanguageContext(project, temp); contextCode != 0) {
    std::filesystem::remove_all(temp, fsError);
    return contextCode;
  }
  if (manifest.name != "demo" || manifest.optLevel != sere::OptLevel::O0) {
    return fail("custom table keys must not override compiler settings");
  }
  (void)writeAll(temp / "app/sere.toml",
      "name = \"legacy\" # comment\nentry = 'src/a#b.sere'\nopt = \"O2\"\n");
  sere::ProjectManifest legacy;
  if (!sere::loadProjectManifest(temp / "app", legacy, error) || legacy.name != "legacy" ||
      legacy.entry.filename() != "a#b.sere" || legacy.optLevel != sere::OptLevel::O2) {
    return fail("legacy flat manifests and quoted comments must remain supported");
  }
  const std::filesystem::path library = temp / "mathlib";
  if (sere::initSereLibrary(library, error) != 0) {
    std::cerr << error << '\n';
    std::filesystem::remove_all(temp, fsError);
    return fail("initSereLibrary failed");
  }
  if (!std::filesystem::exists(library / "src" / "lib.sere") ||
      !std::filesystem::exists(library / "sere.toml")) {
    std::filesystem::remove_all(temp, fsError);
    return fail("init-lib did not write expected files");
  }
  sere::ProjectManifest libManifest;
  if (!sere::loadProjectManifest(library, libManifest, error)) {
    std::filesystem::remove_all(temp, fsError);
    return fail("loadProjectManifest failed for library");
  }
  if (libManifest.kind != sere::ProjectKind::Lib || libManifest.name != "mathlib" ||
      libManifest.entry.filename() != "lib.sere") {
    std::filesystem::remove_all(temp, fsError);
    return fail("library manifest should be kind=lib with src/lib.sere");
  }

  if (!writeAll(project / "venv" / "sere.cfg",
                "home = C:/old-sere\nstdlib = venv/stdlib\nversion = pre-0.1.0\n")) {
    std::filesystem::remove_all(temp, fsError);
    return fail("could not write an old venv/sere.cfg");
  }
  const std::string originalMain = readAll(project / "src" / "main.sere");
  std::string updateError;
  if (sere::updateSereEnvironment(project, updateError) != 0) {
    std::cerr << updateError << '\n';
    std::filesystem::remove_all(temp, fsError);
    return fail("updateSereEnvironment failed");
  }
  const std::string cfg = readAll(project / "venv" / "sere.cfg");
  if (cfg.find(SERE_VERSION_STRING) == std::string::npos) {
    std::filesystem::remove_all(temp, fsError);
    return fail("update should write the installed compiler version into venv/sere.cfg");
  }
  if (readAll(project / "src" / "main.sere") != originalMain) {
    std::filesystem::remove_all(temp, fsError);
    return fail("update must not change project source");
  }
  const std::string toml = readAll(project / "sere.toml");
  if (toml.find(std::string("sere = \"") + SERE_VERSION_STRING + "\"") == std::string::npos) {
    std::filesystem::remove_all(temp, fsError);
    return fail("update should pin sere.toml to the installed compiler");
  }

  std::filesystem::remove_all(temp, fsError);
  return 0;
}
