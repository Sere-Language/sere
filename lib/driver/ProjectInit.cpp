/// @file ProjectInit.cpp
/// Scaffolds a Sere project with src, libs, venv, and scripts/activate.

#include "sere/driver/ProjectInit.h"

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace sere {
namespace {

[[nodiscard]] bool writeText(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  output << text;
  return static_cast<bool>(output);
}

void copyIfExists(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code error;
  if (!std::filesystem::exists(from, error)) {
    return;
  }
  if (std::filesystem::is_directory(from)) {
    std::filesystem::create_directories(to, error);
    std::filesystem::copy(from, to,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing,
                          error);
    return;
  }
  std::filesystem::create_directories(to.parent_path(), error);
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, error);
}

void copyNamed(const std::filesystem::path& directory, std::string_view stem,
               const std::filesystem::path& destDir) {
  const std::string exe = std::string(stem) + ".exe";
  copyIfExists(directory / exe, destDir / exe);
  copyIfExists(directory / stem, destDir / stem);
}

void makeExecutable(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::permissions(path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add, error);
}

[[nodiscard]] std::string tomlText(const std::string& name) {
  std::string output = name;
#ifdef _WIN32
  output += ".exe";
#endif
  return "name = \"" + name +
         "\"\n"
         "src = \"src\"\n"
         "entry = \"src/main.sere\"\n"
         "libs = \"libs\"\n"
         "stdlib = \"venv/stdlib\"\n"
         "output = \"bin/" +
         output +
         "\"\n"
         "opt = \"O0\"\n"
         "native = false\n";
}

[[nodiscard]] bool writeActivateScripts(const std::filesystem::path& root) {
  const char* bashActivate =
      "#!/usr/bin/env bash\n"
      "set -euo pipefail\n"
      "ROOT=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")/..\" && pwd)\"\n"
      "cd \"$ROOT\"\n"
      "if [[ -x \"$ROOT/venv/bin/sere.exe\" ]]; then\n"
      "  SERE=\"$ROOT/venv/bin/sere.exe\"\n"
      "elif [[ -x \"$ROOT/venv/bin/sere\" ]]; then\n"
      "  SERE=\"$ROOT/venv/bin/sere\"\n"
      "else\n"
      "  echo \"sere: missing compiler in venv/bin; re-run 'sere init'\" >&2\n"
      "  exit 1\n"
      "fi\n"
      "if [[ \"${BASH_SOURCE[0]}\" != \"$0\" ]]; then\n"
      "  \"$SERE\" shell --host bash\n"
      "  return $?\n"
      "fi\n"
      "exec \"$SERE\" shell --host bash\n";
  const char* psActivate =
      "$ErrorActionPreference = 'Stop'\n"
      "$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path\n"
      "Set-Location $Root\n"
      "$Sere = Join-Path $Root 'venv\\bin\\sere.exe'\n"
      "if (-not (Test-Path $Sere)) {\n"
      "  $Sere = Join-Path $Root 'venv\\bin\\sere'\n"
      "}\n"
      "if (-not (Test-Path $Sere)) {\n"
      "  Write-Error \"sere: missing compiler in venv/bin; re-run 'sere init'\"\n"
      "  exit 1\n"
      "}\n"
      "& $Sere shell --host powershell\n";
  const char* batActivate =
      "@echo off\n"
      "set \"ROOT=%~dp0..\"\n"
      "pushd \"%ROOT%\" >nul\n"
      "if exist \"venv\\bin\\sere.exe\" (\n"
      "  \"venv\\bin\\sere.exe\" shell --host cmd\n"
      "  popd >nul\n"
      "  exit /b %ERRORLEVEL%\n"
      ")\n"
      "if exist \"venv\\bin\\sere\" (\n"
      "  \"venv\\bin\\sere\" shell --host cmd\n"
      "  popd >nul\n"
      "  exit /b %ERRORLEVEL%\n"
      ")\n"
      "echo sere: missing compiler in venv/bin; re-run 'sere init'\n"
      "popd >nul\n"
      "exit /b 1\n";
  const std::filesystem::path activate = root / "scripts" / "activate";
  const bool ok = writeText(activate, bashActivate) &&
                  writeText(root / "scripts" / "activate.ps1", psActivate) &&
                  writeText(root / "scripts" / "activate.bat", batActivate);
  if (ok) {
    makeExecutable(activate);
  }
  return ok;
}

[[nodiscard]] bool writeShellRc(const std::filesystem::path& root) {
  const char* bashRc =
      "# Sere project shell. Loaded by scripts/activate and `sere shell`.\n"
      "if [ -f \"${HOME}/.bashrc\" ]; then\n"
      "  # shellcheck disable=SC1091\n"
      "  . \"${HOME}/.bashrc\"\n"
      "fi\n"
      "export PATH=\"${SERE_VENV_BIN}:${PATH}\"\n"
      "export SERE_STDLIB=\"${SERE_PROJECT_ROOT}/venv/stdlib\"\n"
      "PS1=\"(sere:${SERE_PROJECT_NAME}) \\w \\$ \"\n"
      "alias deactivate='exit'\n"
      "alias build='sere build'\n"
      "alias run='sere run'\n";
  const char* psRc =
      "if ($PROFILE -and (Test-Path $PROFILE)) {\n"
      "  . $PROFILE\n"
      "}\n"
      "$env:PATH = \"$env:SERE_VENV_BIN;$env:PATH\"\n"
      "$env:SERE_STDLIB = Join-Path $env:SERE_PROJECT_ROOT 'venv\\stdlib'\n"
      "function global:prompt {\n"
      "  \"(sere:$env:SERE_PROJECT_NAME) $($executionContext.SessionState.Path.CurrentLocation)> \"\n"
      "}\n"
      "function global:deactivate { exit }\n"
      "function global:build { sere build @args }\n"
      "function global:run { sere run @args }\n";
  const char* cmdRc =
      "@echo off\n"
      "set \"PATH=%SERE_VENV_BIN%;%PATH%\"\n"
      "set \"SERE_STDLIB=%SERE_PROJECT_ROOT%\\venv\\stdlib\"\n"
      "prompt (sere:%SERE_PROJECT_NAME%) $P$G\n"
      "doskey deactivate=exit\n"
      "doskey build=sere build $*\n"
      "doskey run=sere run $*\n";
  return writeText(root / "venv" / "shell.bash", bashRc) &&
         writeText(root / "venv" / "shell.ps1", psRc) &&
         writeText(root / "venv" / "shell.cmd", cmdRc);
}

[[nodiscard]] bool writeScaffoldSources(const std::filesystem::path& root,
                                        const std::string& name) {
  const char* mainSere =
      "from inspect import label\n"
      "\n"
      "type Number = i32 | i64\n"
      "\n"
      "def main() -> i32:\n"
      "    values: list[i32] = [1, 2, 3, 4]\n"
      "    tail: list[i32] = values[1:]\n"
      "    print(\"hello from sere\")\n"
      "    print(label(\"main\"), typeof(tail), len(tail))\n"
      "    return 0\n";
  const char* nativeCpp =
      "#include \"sere/api/sere_mod.h\"\n"
      "\n"
      "extern \"C\" int32_t native_add(int32_t left, int32_t right) {\n"
      "  return left + right;\n"
      "}\n"
      "\n"
      "static Sere_Object* native_add_obj(Sere_Object* const* args, int32_t nargs) {\n"
      "  if (nargs < 2) {\n"
      "    return Sere_Long_FromI32(0);\n"
      "  }\n"
      "  const int32_t sum = Sere_Long_AsI32(args[0]) + Sere_Long_AsI32(args[1]);\n"
      "  return Sere_Long_FromI32(sum);\n"
      "}\n"
      "\n"
      "extern \"C\" void sere_mod_init(void) {\n"
      "  Sere_DefineFunction(\"add\", native_add_obj, 2);\n"
      "}\n";
  const char* nativeCmake =
      "cmake_minimum_required(VERSION 3.20)\n"
      "project(sere_native LANGUAGES C CXX)\n"
      "add_library(sere_native STATIC example.cpp)\n"
      "target_include_directories(sere_native PUBLIC "
      "\"${CMAKE_CURRENT_SOURCE_DIR}/../../venv/include\")\n";
  const char* nativeSere =
      "extern \"C\" \"native_add\"\n"
      "def add(left: i32, right: i32) -> i32\n";
  const char* gitignore =
      "bin/\n"
      "venv/lib/\n"
      "venv/bin/\n"
      "libs/native/build/\n"
      "*.exe\n"
      "*.obj\n"
      "*.ll\n";
  const std::string readme =
      std::string("Sere project: ") + name +
      "\n"
      "====================\n"
      "src/       Sere sources (entry: src/main.sere)\n"
      "libs/      Sere modules and native C++ extensions\n"
      "bin/       built executables\n"
      "venv/      local stdlib, headers, compiler, and shell rc files\n"
      "scripts/   activate the project shell\n"
      "\n"
      "Activate (from the parent directory or the project root):\n"
      "  ./scripts/activate\n"
      "  .\\scripts\\activate          (PowerShell / cmd)\n"
      "\n"
      "Inside the Sere shell (normal shell commands still work):\n"
      "  sere build\n"
      "  sere run\n"
      "  sere run -- arg1 arg2\n"
      "  sere clean\n"
      "  deactivate\n"
      "\n"
      "Set native = true in sere.toml to auto-build libs/native on sere build.\n"
      "Any .sere file dropped in venv/stdlib is importable.\n"
      "prelude.sere is imported automatically.\n";
  return writeText(root / "src" / "main.sere", mainSere) &&
         writeText(root / "libs" / "native" / "example.cpp", nativeCpp) &&
         writeText(root / "libs" / "native" / "CMakeLists.txt", nativeCmake) &&
         writeText(root / "libs" / "native.sere", nativeSere) &&
         writeText(root / "sere.toml", tomlText(name)) &&
         writeText(root / ".gitignore", gitignore) &&
         writeText(root / "README.txt", readme) && writeActivateScripts(root) &&
         writeShellRc(root);
}

void copyToolchain(const std::filesystem::path& compilerDir, const std::filesystem::path& root) {
  const std::filesystem::path cwdStdlib = std::filesystem::current_path() / "stdlib";
  const std::filesystem::path compilerStdlib = compilerDir / "stdlib";
  const std::filesystem::path venvBin = root / "venv" / "bin";
  const std::filesystem::path venvLib = root / "venv" / "lib";
  copyIfExists(std::filesystem::exists(compilerStdlib / "prelude.sere") ? compilerStdlib
                                                                       : cwdStdlib,
               root / "venv" / "stdlib");
  copyIfExists(compilerDir / "sere_rt.lib", venvLib / "sere_rt.lib");
  copyIfExists(compilerDir / "sere_rt.a", venvLib / "sere_rt.a");
  copyIfExists(compilerDir / "sere_rt.lib", venvBin / "sere_rt.lib");
  copyIfExists(compilerDir / "sere_rt.a", venvBin / "sere_rt.a");
  copyNamed(compilerDir, "sere", venvBin);
  const std::filesystem::path apiNextToCompiler =
      compilerDir / "include" / "sere" / "api" / "sere_mod.h";
  const std::filesystem::path apiFromSource =
      std::filesystem::current_path() / "include" / "sere" / "api" / "sere_mod.h";
  copyIfExists(std::filesystem::exists(apiNextToCompiler) ? apiNextToCompiler : apiFromSource,
               root / "venv" / "include" / "sere" / "api" / "sere_mod.h");
  const std::filesystem::path gcNextToCompiler =
      compilerDir / "include" / "sere" / "api" / "sere_gc.h";
  const std::filesystem::path gcFromSource =
      std::filesystem::current_path() / "include" / "sere" / "api" / "sere_gc.h";
  copyIfExists(std::filesystem::exists(gcNextToCompiler) ? gcNextToCompiler : gcFromSource,
               root / "venv" / "include" / "sere" / "api" / "sere_gc.h");
}

}  // namespace

int initSereProject(const std::filesystem::path& name, const std::filesystem::path& compilerDir,
                    std::string& error) {
  const std::filesystem::path root = std::filesystem::absolute(name);
  const std::string projectName = root.filename().string();
  std::error_code fsError;
  std::filesystem::create_directories(root / "src", fsError);
  std::filesystem::create_directories(root / "libs" / "native", fsError);
  std::filesystem::create_directories(root / "bin", fsError);
  std::filesystem::create_directories(root / "scripts", fsError);
  std::filesystem::create_directories(root / "venv" / "bin", fsError);
  std::filesystem::create_directories(root / "venv" / "include" / "sere" / "api", fsError);
  std::filesystem::create_directories(root / "venv" / "lib", fsError);
  std::filesystem::create_directories(root / "venv" / "stdlib", fsError);
  if (fsError) {
    error = "cannot create project directories: " + fsError.message();
    return 1;
  }
  if (!writeScaffoldSources(root, projectName)) {
    error = "cannot write project files";
    return 1;
  }
  copyToolchain(compilerDir, root);
  if (!writeText(root / "venv" / "sere.cfg",
                 "home = " + compilerDir.string() + "\nstdlib = venv/stdlib\n")) {
    error = "cannot write venv/sere.cfg";
    return 1;
  }
  std::cout << "created Sere project '" << root.string() << "'\n";
  std::cout << "  activate:  ./scripts/activate\n";
  std::cout << "  then:      sere build   |   sere run   |   deactivate\n";
  return 0;
}

}  // namespace sere
