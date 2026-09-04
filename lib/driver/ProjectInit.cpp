/// @file ProjectInit.cpp
/// Scaffolds a Sere project with src, libs, venv, and scripts/activate.

#include "sere/driver/ProjectInit.h"

#include "sere/Version.h"
#include "sere/driver/Project.h"
#include "sere/driver/Toolchain.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

[[nodiscard]] bool samePath(const std::filesystem::path& left, const std::filesystem::path& right) {
  if (left.empty() || right.empty()) {
    return false;
  }
  std::error_code error;
  if (std::filesystem::equivalent(left, right, error) && !error) {
    return true;
  }
  std::error_code leftError;
  std::error_code rightError;
  const std::filesystem::path canonicalLeft = std::filesystem::weakly_canonical(left, leftError);
  const std::filesystem::path canonicalRight = std::filesystem::weakly_canonical(right, rightError);
  return !leftError && !rightError && canonicalLeft == canonicalRight;
}

void copyIfExists(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code error;
  if (!std::filesystem::exists(from, error)) {
    return;
  }
  if (samePath(from, to)) {
    return;
  }
  if (std::filesystem::is_directory(from)) {
    std::filesystem::create_directories(to, error);
    // Copy entries explicitly: the destination is the contents directory.
    for (const auto& entry : std::filesystem::directory_iterator(from)) {
      // Ignore legacy duplicated stdlib trees, preserving real module directories.
      if (from.filename() == "stdlib" && entry.path().filename() == "stdlib" &&
          std::filesystem::exists(entry.path() / "prelude.sere")) {
        continue;
      }
      copyIfExists(entry.path(), to / entry.path().filename());
    }
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

[[nodiscard]] std::string escapeToml(std::string_view text) {
  std::string result;
  for (char ch : text) {
    switch (ch) {
    case '\\': result += "\\\\"; break;
    case '"': result += "\\\""; break;
    case '\n': result += "\\n"; break;
    case '\r': result += "\\r"; break;
    case '\t': result += "\\t"; break;
    default: result += ch; break;
    }
  }
  return result;
}

[[nodiscard]] std::string tomlText(const std::string& name) {
  std::string output = name;
#ifdef _WIN32
  output += ".exe";
#endif
  return "# Sere project configuration. Paths are relative to this file.\n"
         "[project]\n"
         "version = \"0.1.0\"\n"
         "kind = \"app\"\n"
         "name = \"" +
         escapeToml(name) +
         "\"\n"
         "\n[toolchain]\n"
         "sere = \"" SERE_VERSION_STRING "\"\n"
         "\n[paths]\n"
         "src = \"src\"\n"
         "entry = \"src/main.sere\"\n"
         "libs = \"libs\"\n"
         "stdlib = \"venv/stdlib\"\n"
         "\n[build]\n"
         "output = \"bin/" +
         escapeToml(output) +
         "\"\n"
         "opt = \"O0\"\n"
         "# Build loose C/C++ files in libs/native when enabled.\n"
         "native = false\n"
         "\n# Custom tool settings belong in [tool.<name>] tables.\n";
}

[[nodiscard]] bool writeActivateScripts(const std::filesystem::path& root) {
  const char* bashActivate = R"SH(#!/usr/bin/env bash
# Source into the current shell:
#   . ./scripts/activate
# Leave with: deactivate

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

find_sere() {
  if [[ -x "$ROOT/venv/bin/sere" ]]; then
    echo "$ROOT/venv/bin/sere"
    return
  fi
  if [[ -x "$ROOT/venv/bin/sere.exe" ]]; then
    echo "$ROOT/venv/bin/sere.exe"
    return
  fi
  if [[ -f "$ROOT/venv/sere.cfg" ]]; then
    local home
    home="$(sed -n 's/^[[:space:]]*home[[:space:]]*=[[:space:]]*//p' "$ROOT/venv/sere.cfg" | head -n 1 | tr -d '"')"
    if [[ -n "$home" && -x "$home/sere" ]]; then
      echo "$home/sere"
      return
    fi
    if [[ -n "$home" && -x "$home/sere.exe" ]]; then
      echo "$home/sere.exe"
      return
    fi
  fi
  command -v sere 2>/dev/null || true
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "Activate in this shell with:"
  echo "  . ./scripts/activate"
  SERE="$(find_sere)"
  if [[ -n "$SERE" ]]; then
    echo
    echo "Starting a clean Sere shell..."
    cd "$ROOT"
    exec "$SERE" shell --host bash
  fi
  exit 1
fi

if [[ -n "${SERE_ACTIVE:-}" ]]; then
  echo "Already in ${SERE_PROJECT_NAME}."
  return 0
fi

_SERE_OLD_PATH="$PATH"
_SERE_OLD_PS1="${PS1:-}"
export SERE_ACTIVE=1
export SERE_PROJECT_ROOT="$ROOT"
export SERE_PROJECT_NAME="$(basename "$ROOT")"
export SERE_VENV_BIN="$ROOT/venv/bin"
if [[ -f "$ROOT/venv/stdlib/prelude.sere" ]]; then
  export SERE_STDLIB="$ROOT/venv/stdlib"
fi
SERE="$(find_sere)"
  SERE_BIN=""
  if [[ -n "$SERE" ]]; then
    SERE_BIN="$(cd "$(dirname "$SERE")" && pwd)"
    export SERE_HOME="$SERE_BIN"
    export PATH="$SERE_BIN:$SERE_VENV_BIN:$PATH"
else
  export PATH="$SERE_VENV_BIN:$PATH"
fi
cd "$ROOT"
PS1="(sere:${SERE_PROJECT_NAME}) \\w \\$ "
deactivate() {
  export PATH="$_SERE_OLD_PATH"
  PS1="$_SERE_OLD_PS1"
  unset SERE_ACTIVE SERE_PROJECT_ROOT SERE_PROJECT_NAME SERE_VENV_BIN
  unset -f deactivate find_sere
  echo "Sere project deactivated."
}
echo "Sere project: ${SERE_PROJECT_NAME}"
if [[ -n "$SERE" ]]; then
  echo "  compiler  $SERE"
else
  echo "  compiler  not found (run bin/sere-path)"
fi
echo "  commands  sere build | sere run | sere clean | deactivate"
)SH";
  const char* psActivate = R"PS(# Activate this Sere project in the current PowerShell:
#   . .\scripts\activate.ps1
# Leave with: deactivate

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Find-SereCompiler([string]$ProjectRoot) {
  $candidates = @(
    (Join-Path $ProjectRoot 'venv\bin\sere.exe'),
    (Join-Path $ProjectRoot 'venv\bin\sere')
  )
  $cfg = Join-Path $ProjectRoot 'venv\sere.cfg'
  if (Test-Path $cfg) {
    foreach ($line in Get-Content $cfg) {
      if ($line -match '^\s*home\s*=\s*(.+)$') {
        $home = $Matches[1].Trim().Trim('"')
        $candidates += (Join-Path $home 'sere.exe')
        $candidates += (Join-Path $home 'sere')
      }
    }
  }
  $cmd = Get-Command sere -ErrorAction SilentlyContinue
  if ($cmd) { $candidates += $cmd.Source }
  foreach ($path in $candidates) {
    if ($path -and (Test-Path $path)) { return $path }
  }
  return $null
}

$Sourced = $MyInvocation.InvocationName -eq '.'
if (-not $Sourced) {
  Write-Host "Activate in this shell with:"
  Write-Host "  . .\scripts\activate.ps1"
  $sere = Find-SereCompiler $Root
  if ($sere) {
    Write-Host ""
    Write-Host "Starting a clean Sere shell..."
    Set-Location $Root
    & $sere shell --host powershell
  }
  return
}

if ($env:SERE_ACTIVE) {
  Write-Host "Already in $($env:SERE_PROJECT_NAME)."
  return
}

$Name = Split-Path $Root -Leaf
$toml = Join-Path $Root 'sere.toml'
if (Test-Path $toml) {
  $Section = ''
  foreach ($line in Get-Content $toml) {
    if ($line -match '^\s*\[([^]]+)\]') { $Section = $Matches[1]; continue }
    if ($Section -in @('', 'project') -and $line -match '^\s*name\s*=\s*"?([^"#]+)"?') { $Name = $Matches[1].Trim() }
  }
}

$Sere = Find-SereCompiler $Root
$SereBin = if ($Sere) { Split-Path $Sere -Parent } else { Join-Path $Root 'venv\bin' }
$VenvBin = Join-Path $Root 'venv\bin'
$Stdlib = Join-Path $Root 'venv\stdlib'
if (-not (Test-Path (Join-Path $Stdlib 'prelude.sere'))) {
  $near = Join-Path $SereBin 'stdlib'
  if (Test-Path (Join-Path $near 'prelude.sere')) { $Stdlib = $near }
}

$global:_SerePrev = @{
  PATH = $env:PATH
  SERE_ACTIVE = $env:SERE_ACTIVE
  SERE_PROJECT_ROOT = $env:SERE_PROJECT_ROOT
  SERE_PROJECT_NAME = $env:SERE_PROJECT_NAME
  SERE_VENV_BIN = $env:SERE_VENV_BIN
  SERE_STDLIB = $env:SERE_STDLIB
  SERE_HOME = $env:SERE_HOME
  Prompt = $function:prompt
}

$env:SERE_ACTIVE = '1'
$env:SERE_PROJECT_ROOT = $Root
$env:SERE_PROJECT_NAME = $Name
$env:SERE_VENV_BIN = $VenvBin
$env:SERE_STDLIB = $Stdlib
$env:SERE_HOME = $SereBin
$env:PATH = "$SereBin;$VenvBin;$env:PATH"
Set-Location $Root

function global:prompt {
  "(sere:$env:SERE_PROJECT_NAME) $($executionContext.SessionState.Path.CurrentLocation.ProviderPath)> "
}

function global:deactivate {
  if (-not $global:_SerePrev) { return }
  $env:PATH = $global:_SerePrev.PATH
  $env:SERE_ACTIVE = $global:_SerePrev.SERE_ACTIVE
  $env:SERE_PROJECT_ROOT = $global:_SerePrev.SERE_PROJECT_ROOT
  $env:SERE_PROJECT_NAME = $global:_SerePrev.SERE_PROJECT_NAME
  $env:SERE_VENV_BIN = $global:_SerePrev.SERE_VENV_BIN
  $env:SERE_STDLIB = $global:_SerePrev.SERE_STDLIB
  $env:SERE_HOME = $global:_SerePrev.SERE_HOME
  if ($global:_SerePrev.Prompt) {
    Set-Item function:global:prompt $global:_SerePrev.Prompt
  }
  Remove-Item function:global:deactivate -ErrorAction SilentlyContinue
  Remove-Variable _SerePrev -Scope Global -ErrorAction SilentlyContinue
  Write-Host "Sere project deactivated."
}

Remove-Item function:Find-SereCompiler -ErrorAction SilentlyContinue
Write-Host "Sere project: $Name"
if ($Sere) { Write-Host "  compiler  $Sere" } else { Write-Host "  compiler  not found (run .\bin\sere-path.ps1)" }
Write-Host "  stdlib    $env:SERE_STDLIB"
Write-Host "  commands  sere build | sere run | sere clean | deactivate"
)PS";
  const char* batActivate = R"CMD(@echo off
rem Stay in this cmd session:
rem   call scripts\activate.bat
rem Leave with: deactivate

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"

if defined SERE_ACTIVE (
  echo Already in %SERE_PROJECT_NAME%.
  exit /b 0
)

set "SERE_OLD_PATH=%PATH%"
set "SERE_OLD_PROMPT=%PROMPT%"
set "SERE_ACTIVE=1"
set "SERE_PROJECT_ROOT=%ROOT%"
for %%I in ("%ROOT%") do set "SERE_PROJECT_NAME=%%~nxI"
set "SERE_VENV_BIN=%ROOT%\venv\bin"
if exist "%ROOT%\venv\stdlib\prelude.sere" set "SERE_STDLIB=%ROOT%\venv\stdlib"
if exist "%ROOT%\venv\bin\sere.exe" set "PATH=%ROOT%\venv\bin;%PATH%"
if exist "%ROOT%\venv\sere.cfg" (
  for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"home" "%ROOT%\venv\sere.cfg"') do (
    set "SERE_HOME=%%~B"
  )
)
if defined SERE_HOME (
  set "SERE_HOME=%SERE_HOME: =%"
  set "PATH=%SERE_HOME%;%PATH%"
)
cd /d "%ROOT%"
prompt (sere:%SERE_PROJECT_NAME%) $P$G
doskey deactivate=set "PATH=%SERE_OLD_PATH%" $T prompt %SERE_OLD_PROMPT% $T set "SERE_ACTIVE=" $T echo Sere project deactivated.
echo Sere project: %SERE_PROJECT_NAME%
echo   commands  sere build ^| sere run ^| sere clean ^| deactivate
echo   note      use "call scripts\activate.bat" so PATH stays in this window
)CMD";
  const std::filesystem::path activate = root / "scripts" / "activate";
  const bool ok = writeText(activate, bashActivate) &&
                  writeText(root / "scripts" / "activate.ps1", psActivate) &&
                  writeText(root / "scripts" / "activate.bat", batActivate);
  if (ok) {
    makeExecutable(activate);
  }
  return ok;
}

[[nodiscard]] bool writePathScripts(const std::filesystem::path& root) {
  const char* ps = R"PS(# Puts this project's Sere compiler on PATH.
#   .\bin\sere-path.ps1
#   .\bin\sere-path.ps1 -Persistent

[CmdletBinding()]
param(
  [switch]$Persistent,
  [switch]$Remove
)

function Add-HomeFromCfg([string]$cfg, [System.Collections.Generic.List[string]]$dirs) {
  if (-not (Test-Path $cfg)) { return }
  foreach ($line in Get-Content $cfg) {
    if ($line -match '^\s*home\s*=\s*(.+)$') {
      $home = $Matches[1].Trim().Trim('"')
      if ($home) { $dirs.Add($home) }
    }
  }
}

function Find-SereBin {
  $here = $PSScriptRoot
  $dirs = [System.Collections.Generic.List[string]]@(
    $here,
    (Join-Path $here "bin"),
    (Join-Path $here "..\bin"),
    (Join-Path $here "..\venv\bin"),
    (Join-Path $here "..\build\windows-clang-cl-relwithdebinfo\bin")
  )
  Add-HomeFromCfg (Join-Path $here "sere.cfg") $dirs
  Add-HomeFromCfg (Join-Path $here "..\venv\sere.cfg") $dirs
  Add-HomeFromCfg (Join-Path $here "..\sere.cfg") $dirs
  foreach ($dir in $dirs) {
    try {
      $resolved = (Resolve-Path $dir -ErrorAction Stop).Path
    } catch { continue }
    if ((Test-Path (Join-Path $resolved "sere.exe")) -or (Test-Path (Join-Path $resolved "sere"))) {
      return $resolved
    }
  }
  $cmd = Get-Command sere -ErrorAction SilentlyContinue
  if ($cmd) { return (Split-Path -Parent $cmd.Source) }
  return $null
}

function Normalize-PathList([string]$text) {
  if ([string]::IsNullOrWhiteSpace($text)) { return @() }
  return @($text.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() })
}

$bin = Find-SereBin
if (-not $bin) {
  Write-Error "sere.exe not found. Build the compiler or run this from the folder that contains it."
  return
}

$sessionParts = [System.Collections.Generic.List[string]](Normalize-PathList $env:PATH)
$userParts = [System.Collections.Generic.List[string]](Normalize-PathList ([Environment]::GetEnvironmentVariable("Path", "User")))

function Remove-Dir([System.Collections.Generic.List[string]]$parts, [string]$dir) {
  $keep = @($parts | Where-Object { $_ -and ($_.TrimEnd('\') -ne $dir.TrimEnd('\')) })
  $parts.Clear()
  foreach ($item in $keep) { $parts.Add($item) }
}

if ($Remove) {
  Remove-Dir $sessionParts $bin
  $env:PATH = ($sessionParts -join ';')
  if ($Persistent) {
    Remove-Dir $userParts $bin
    [Environment]::SetEnvironmentVariable("Path", ($userParts -join ';'), "User")
    Write-Host "Removed from user PATH: $bin"
  }
  Write-Host "Removed from this session: $bin"
  return
}

Remove-Dir $sessionParts $bin
$sessionParts.Insert(0, $bin)
$env:PATH = ($sessionParts -join ';')
Write-Host "This session PATH starts with:"
Write-Host "  $bin"
if ($Persistent) {
  Remove-Dir $userParts $bin
  $userParts.Insert(0, $bin)
  [Environment]::SetEnvironmentVariable("Path", ($userParts -join ';'), "User")
  Write-Host "Saved on your user PATH (new terminals will see it)."
}
$sere = Join-Path $bin "sere.exe"
if (-not (Test-Path $sere)) { $sere = Join-Path $bin "sere" }
Write-Host "sere -> $sere"
Write-Host "Try:  sere --help"
)PS";
  const char* cmd =
      "@echo off\n"
      "set \"HERE=%~dp0\"\n"
      "if \"%HERE:~-1%\"==\"\\\" set \"HERE=%HERE:~0,-1%\"\n"
      "set \"SERE_BIN=\"\n"
      "if exist \"%HERE%\\sere.exe\" set \"SERE_BIN=%HERE%\"\n"
      "if not defined SERE_BIN if exist \"%HERE%\\..\\venv\\bin\\sere.exe\" for %%I in (\"%HERE%\\..\\venv\\bin\") do set \"SERE_BIN=%%~fI\"\n"
      "if not defined SERE_BIN if exist \"%HERE%\\..\\bin\\sere.exe\" for %%I in (\"%HERE%\\..\\bin\") do set \"SERE_BIN=%%~fI\"\n"
      "if not defined SERE_BIN (\n"
      "  echo sere.exe not found. Run this from a Sere project or compiler bin folder.\n"
      "  exit /b 1\n"
      ")\n"
      "if /I \"%~1\"==\"-Remove\" (\n"
      "  powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%HERE%\\sere-path.ps1\" -Remove %*\n"
      "  exit /b %ERRORLEVEL%\n"
      ")\n"
      "set \"PATH=%SERE_BIN%;%PATH%\"\n"
      "echo This session PATH starts with:\n"
      "echo   %SERE_BIN%\n"
      "if /I \"%~1\"==\"-Persistent\" powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%HERE%\\sere-path.ps1\" -Persistent\n"
      "exit /b 0\n";
  const char* sh = R"SH(#!/usr/bin/env bash
#   . ./bin/sere-path.sh
#   . ./bin/sere-path.sh --persist
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -x "$here/sere" || -x "$here/sere.exe" ]]; then
  bin="$here"
elif [[ -x "$here/../venv/bin/sere" || -x "$here/../venv/bin/sere.exe" ]]; then
  bin="$(cd "$here/../venv/bin" && pwd)"
elif [[ -f "$here/../venv/sere.cfg" ]]; then
  bin="$(sed -n 's/^[[:space:]]*home[[:space:]]*=[[:space:]]*//p' "$here/../venv/sere.cfg" | head -n 1 | tr -d '"')"
fi
if [[ -z "${bin:-}" || ! ( -x "$bin/sere" || -x "$bin/sere.exe" ) ]]; then
  echo "sere not found. Run this from a Sere project or compiler bin folder." >&2
  return 1 2>/dev/null || exit 1
fi
export PATH="$bin:${PATH}"
echo "This session PATH starts with:"
echo "  $bin"
if [[ "${1:-}" == "--persist" ]]; then
  touch "${HOME}/.profile"
  grep -Fq "$bin" "${HOME}/.profile" || printf '\nexport PATH="%s:$PATH"\n' "$bin" >> "${HOME}/.profile"
  echo "Saved in ${HOME}/.profile"
fi
)SH";
  const bool ok = writeText(root / "bin" / "sere-path.ps1", ps) &&
                  writeText(root / "bin" / "sere-path.cmd", cmd) &&
                  writeText(root / "bin" / "sere-path.sh", sh) &&
                  writeText(root / "venv" / "bin" / "sere-path.ps1", ps) &&
                  writeText(root / "venv" / "bin" / "sere-path.cmd", cmd) &&
                  writeText(root / "venv" / "bin" / "sere-path.sh", sh);
  if (ok) {
    makeExecutable(root / "bin" / "sere-path.sh");
    makeExecutable(root / "venv" / "bin" / "sere-path.sh");
  }
  return ok;
}

void writeShellRcImpl(const std::filesystem::path& root) {
  const char* bashRc =
      "# Nested `sere shell` only. Does not load ~/.bashrc.\n"
      "# PATH and SERE_* are already set by the parent `sere` process.\n"
      "PS1=\"(sere:${SERE_PROJECT_NAME}) \\w \\$ \"\n"
      "deactivate() { echo \"Leaving nested Sere shell.\"; exit; }\n";
  const char* psRc =
      "# Nested `sere shell` only. Does not load the user profile.\n"
      "# PATH and SERE_* are already set by the parent `sere` process.\n"
      "function global:prompt {\n"
      "  \"(sere:$env:SERE_PROJECT_NAME) $($executionContext.SessionState.Path.CurrentLocation.ProviderPath)> \"\n"
      "}\n"
      "function global:deactivate {\n"
      "  Write-Host 'Leaving nested Sere shell.'\n"
      "  exit\n"
      "}\n";
  const char* cmdRc =
      "@echo off\n"
      "prompt (sere:%SERE_PROJECT_NAME%) $P$G\n"
      "doskey deactivate=echo Leaving nested Sere shell. $T exit\n";
  (void)writeText(root / "venv" / "shell.bash", bashRc);
  (void)writeText(root / "venv" / "shell.ps1", psRc);
  (void)writeText(root / "venv" / "shell.cmd", cmdRc);
}

}  // namespace

void writeProjectShellRc(const std::filesystem::path& root) {
  writeShellRcImpl(root);
}

namespace {

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
      "bin/*\n"
      "!bin/sere-path.ps1\n"
      "!bin/sere-path.cmd\n"
      "!bin/sere-path.sh\n"
      "venv/lib/\n"
      "venv/bin/\n"
      "libs/native/build/\n"
      "libs/.sere-lib/\n"
      "dist/\n"
      "*.exe\n"
      "*.obj\n"
      "*.ll\n";
  const std::string readme =
      std::string("Sere project: ") + name +
      "\n"
      "====================\n"
      "src/       Sere sources (entry: src/main.sere)\n"
      "libs/      Sere modules and native C++ extensions\n"
      "venv/      local stdlib, headers, compiler copy, and nested-shell rc\n"
      "scripts/   activate this project in your current terminal\n"
      "bin/       built programs plus sere-path (puts the compiler on PATH)\n"
      "\n"
      "Stay in this terminal (recommended):\n"
      "  . ./scripts/activate           bash\n"
      "  . .\\scripts\\activate.ps1      PowerShell\n"
      "  call scripts\\activate.bat     cmd\n"
      "  deactivate                     restore PATH and this prompt\n"
      "\n"
      "Put the compiler on PATH (this machine):\n"
      "  .\\bin\\sere-path.ps1\n"
      "  .\\bin\\sere-path.ps1 -Persistent\n"
      "\n"
      "Then:\n"
      "  sere build\n"
      "  sere run\n"
      "  sere run -- arg1 arg2\n"
      "  sere clean\n"
      "\n"
      "Set native = true in sere.toml to auto-build libs/native on sere build.\n"
      "Drop a packed .slib (or .sere) into libs/ and `import` it.\n"
      "Any .sere file dropped in venv/stdlib is importable.\n"
      "prelude.sere is imported automatically.\n";
  return writeText(root / "src" / "main.sere", mainSere) &&
         writeText(root / "libs" / "native" / "example.cpp", nativeCpp) &&
         writeText(root / "libs" / "native" / "CMakeLists.txt", nativeCmake) &&
         writeText(root / "libs" / "native.sere", nativeSere) &&
         writeText(root / "sere.toml", tomlText(name)) &&
         writeText(root / ".gitignore", gitignore) &&
         writeText(root / "README.txt", readme) && writeActivateScripts(root) &&
         writePathScripts(root);
}

[[nodiscard]] std::filesystem::path stdlibBesideCompiler(const std::filesystem::path& compilerDir) {
  std::error_code error;
  const std::filesystem::path nested = compilerDir / "stdlib";
  if (std::filesystem::exists(nested / "prelude.sere", error)) {
    return nested;
  }
  const std::filesystem::path sibling = compilerDir.parent_path() / "stdlib";
  if (std::filesystem::exists(sibling / "prelude.sere", error)) {
    return sibling;
  }
  const std::filesystem::path cwd = std::filesystem::current_path() / "stdlib";
  if (std::filesystem::exists(cwd / "prelude.sere", error)) {
    return cwd;
  }
  return {};
}

void copyToolchain(const std::filesystem::path& compilerDir, const std::filesystem::path& root) {
  const std::filesystem::path venvBin = root / "venv" / "bin";
  const std::filesystem::path venvLib = root / "venv" / "lib";
  copyIfExists(stdlibBesideCompiler(compilerDir), root / "venv" / "stdlib");
  copyIfExists(compilerDir / "sere_rt.lib", venvLib / "sere_rt.lib");
  copyIfExists(compilerDir / "sere_rt.a", venvLib / "sere_rt.a");
  copyIfExists(compilerDir / "libsere_rt.a", venvLib / "libsere_rt.a");
  copyIfExists(compilerDir / "sere_rt.lib", venvBin / "sere_rt.lib");
  copyIfExists(compilerDir / "sere_rt.a", venvBin / "sere_rt.a");
  copyIfExists(compilerDir / "libsere_rt.a", venvBin / "libsere_rt.a");
  copyNamed(compilerDir, "sere", venvBin);
  for (const char* file : {"sere_qt6.lib", "sere_icon.res"}) {
    copyIfExists(compilerDir / file, venvBin / file);
  }
  for (const auto& entry : std::filesystem::directory_iterator(compilerDir)) {
    if (entry.path().extension() == ".dll" || entry.path().extension() == ".so") {
      copyIfExists(entry.path(), venvBin / entry.path().filename());
    }
  }
  copyIfExists(compilerDir / "platforms", venvBin / "platforms");
  copyIfExists(compilerDir / "sere-path.ps1", root / "bin" / "sere-path.ps1");
  copyIfExists(compilerDir / "sere-path.cmd", root / "bin" / "sere-path.cmd");
  copyIfExists(compilerDir / "sere-path.sh", root / "bin" / "sere-path.sh");
  copyIfExists(compilerDir / "sere-path.ps1", venvBin / "sere-path.ps1");
  copyIfExists(compilerDir / "sere-path.cmd", venvBin / "sere-path.cmd");
  copyIfExists(compilerDir / "sere-path.sh", venvBin / "sere-path.sh");
  const std::filesystem::path scripts = compilerDir.parent_path() / "scripts";
  copyIfExists(scripts / "sere-path.ps1", root / "bin" / "sere-path.ps1");
  copyIfExists(scripts / "sere-path.cmd", root / "bin" / "sere-path.cmd");
  copyIfExists(scripts / "sere-path.sh", root / "bin" / "sere-path.sh");
  const std::filesystem::path apiNextToCompiler =
      compilerDir / "include" / "sere" / "api" / "sere_mod.h";
  const std::filesystem::path apiFromSource =
      compilerDir.parent_path() / "include" / "sere" / "api" / "sere_mod.h";
  copyIfExists(std::filesystem::exists(apiNextToCompiler) ? apiNextToCompiler : apiFromSource,
               root / "venv" / "include" / "sere" / "api" / "sere_mod.h");
  const std::filesystem::path gcNextToCompiler =
      compilerDir / "include" / "sere" / "api" / "sere_gc.h";
  const std::filesystem::path gcFromSource =
      compilerDir.parent_path() / "include" / "sere" / "api" / "sere_gc.h";
  copyIfExists(std::filesystem::exists(gcNextToCompiler) ? gcNextToCompiler : gcFromSource,
               root / "venv" / "include" / "sere" / "api" / "sere_gc.h");
}

[[nodiscard]] std::string libraryTomlText(const std::string& name) {
  return "# Sere library configuration. Paths are relative to this file.\n"
         "[project]\n"
         "kind = \"lib\"\n"
         "name = \"" +
         escapeToml(name) +
         "\"\n"
         "version = \"0.1.0\"\n"
         "\n[toolchain]\n"
         "sere = \"" SERE_VERSION_STRING "\"\n"
         "\n[paths]\n"
         "src = \"src\"\n"
         "entry = \"src/lib.sere\"\n"
         "libs = \"libs\"\n"
         "\n[build]\n"
         "output = \"dist/" +
         escapeToml(name) +
         ".slib\"\n"
         "opt = \"O0\"\n"
         "# Build loose C/C++ files in libs/native when enabled.\n"
         "native = false\n"
         "\n# Custom tool settings belong in [tool.<name>] tables.\n";
}

[[nodiscard]] bool writeLibraryScaffold(const std::filesystem::path& root, const std::string& name) {
  const std::string libSere =
      "\"\"\"Drop-in Sere library. Pack with `sere pack`, then copy dist/" + name +
      ".slib into another project's libs/ folder and `import " + name +
      "`.\n"
      "\"\"\"\n"
      "\n"
      "def add(left: i32, right: i32) -> i32:\n"
      "    return left + right\n";
  const char* nativeCpp =
      "#include \"sere/api/sere_mod.h\"\n"
      "\n"
      "extern \"C\" int32_t native_add(int32_t left, int32_t right) {\n"
      "  return left + right;\n"
      "}\n";
  const char* nativeCmake =
      "cmake_minimum_required(VERSION 3.20)\n"
      "project(sere_native LANGUAGES C CXX)\n"
      "add_library(sere_native STATIC example.cpp)\n";
  const char* nativeSere =
      "extern \"C\" \"native_add\"\n"
      "def add(left: i32, right: i32) -> i32\n";
  const char* gitignore =
      "dist/\n"
      ".sere-lib/\n"
      "libs/native/build/\n"
      "*.exe\n"
      "*.obj\n"
      "*.ll\n";
  const std::string readme =
      std::string("Sere library: ") + name +
      "\n"
      "====================\n"
      "src/lib.sere   public API (no main)\n"
      "libs/          extra Sere modules and optional native C++\n"
      "dist/          packed .slib after `sere pack`\n"
      "\n"
      "Build a single file that another Sere program can import:\n"
      "  sere pack\n"
      "  sere pack -o dist/" +
      name +
      ".slib\n"
      "\n"
      "Drop the file into a project:\n"
      "  copy dist/" +
      name +
      ".slib  other-app/libs/\n"
      "\n"
      "Then in that program:\n"
      "  import " +
      name +
      "\n"
      "\n"
      "Set native = true in sere.toml to compile libs/native into the .slib.\n";
  return writeText(root / "src" / "lib.sere", libSere) &&
         writeText(root / "libs" / "native" / "example.cpp", nativeCpp) &&
         writeText(root / "libs" / "native" / "CMakeLists.txt", nativeCmake) &&
         writeText(root / "libs" / "native.sere", nativeSere) &&
         writeText(root / "sere.toml", libraryTomlText(name)) &&
         writeText(root / ".gitignore", gitignore) && writeText(root / "README.txt", readme);
}

}  // namespace

int initSereLibrary(const std::filesystem::path& name, std::string& error) {
  const std::filesystem::path root = std::filesystem::absolute(name);
  const std::string projectName = root.filename().string();
  std::error_code fsError;
  std::filesystem::create_directories(root / "src", fsError);
  std::filesystem::create_directories(root / "libs" / "native", fsError);
  std::filesystem::create_directories(root / "dist", fsError);
  if (fsError) {
    error = "cannot create library directories: " + fsError.message();
    return 1;
  }
  if (!writeLibraryScaffold(root, projectName)) {
    error = "cannot write library files";
    return 1;
  }
  std::cout << "created Sere library '" << root.string() << "'\n";
  std::cout << "  edit:   src/lib.sere\n";
  std::cout << "  pack:   sere pack\n";
  std::cout << "  drop:   copy dist/" << projectName << ".slib into another project's libs/\n";
  std::cout << "  import: import " << projectName << "\n";
  return 0;
}

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
  writeProjectShellRc(root);
  copyProjectToolchain(compilerDir, root);
  if (!writeText(root / "venv" / "sere.cfg",
                 "home = " + compilerDir.string() + "\nstdlib = venv/stdlib\nversion = " +
                     SERE_VERSION_STRING + "\n")) {
    error = "cannot write venv/sere.cfg";
    return 1;
  }
  std::cout << "created Sere project '" << root.string() << "'\n";
  std::cout << "  PowerShell:  . .\\scripts\\activate.ps1\n";
  std::cout << "  cmd:         call scripts\\activate.bat\n";
  std::cout << "  bash:        . ./scripts/activate\n";
  std::cout << "  then:        sere build | sere run | deactivate\n";
  std::cout << "  compiler:    .\\bin\\sere-path.ps1\n";
  std::cout << "               .\\bin\\sere-path.ps1 -Persistent\n";
  return 0;
}

void copyProjectToolchain(const std::filesystem::path& compilerDir,
                          const std::filesystem::path& root) {
  copyToolchain(compilerDir, root);
}

namespace {

[[nodiscard]] std::string trimCopy(std::string_view text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::string unquote(std::string text) {
  text = trimCopy(text);
  if (text.size() >= 2 && ((text.front() == '"' && text.back() == '"') ||
                           (text.front() == '\'' && text.back() == '\''))) {
    return text.substr(1, text.size() - 2);
  }
  return text;
}

[[nodiscard]] std::string shortVersionLabel(std::string_view text) {
  std::string trimmed = trimCopy(text);
  if (trimmed.rfind("sere ", 0) == 0) {
    trimmed = trimCopy(trimmed.substr(5));
  }
  const std::size_t paren = trimmed.find('(');
  if (paren != std::string::npos) {
    trimmed = trimCopy(trimmed.substr(0, paren));
  }
  return trimmed;
}

[[nodiscard]] std::filesystem::path sereExecutable(const std::filesystem::path& directory) {
#ifdef _WIN32
  return directory / "sere.exe";
#else
  return directory / "sere";
#endif
}

[[nodiscard]] std::string readKeyedValue(const std::filesystem::path& path, std::string_view key) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  std::string line;
  while (std::getline(input, line)) {
    const std::string trimmed = trimCopy(line);
    const std::size_t eq = trimmed.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    if (trimCopy(trimmed.substr(0, eq)) == key) {
      return unquote(trimmed.substr(eq + 1));
    }
  }
  return {};
}

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  std::ostringstream stream;
  stream << input.rdbuf();
  return stream.str();
}

bool writeKeyedFile(const std::filesystem::path& path, const std::string& text) {
  return writeText(path, text);
}

[[nodiscard]] std::string queryCompilerVersion(const std::filesystem::path& exe) {
  std::error_code existsError;
  if (exe.empty() || !std::filesystem::exists(exe, existsError)) {
    return {};
  }
  std::error_code selfError;
  const std::filesystem::path self = sereExecutable(compilerDirectory());
  if (std::filesystem::exists(self, selfError) &&
      std::filesystem::equivalent(exe, self, selfError) && !selfError) {
    return SERE_VERSION_STRING;
  }
  llvm::SmallString<260> temp;
  if (llvm::sys::fs::createTemporaryFile("sere-ver", "txt", temp)) {
    return {};
  }
  const std::string tempPath = std::string(temp);
  std::optional<llvm::StringRef> redirects[3] = {std::nullopt, llvm::StringRef(tempPath),
                                                 std::nullopt};
  const std::string exeString = exe.string();
  llvm::SmallVector<llvm::StringRef, 2> args{exeString, "--version"};
  std::string launchError;
  bool failed = false;
  const int code =
      llvm::sys::ExecuteAndWait(exeString, args, std::nullopt, redirects, 8, 0, &launchError,
                                &failed);
  const std::string output = readTextFile(tempPath);
  std::error_code removeError;
  std::filesystem::remove(tempPath, removeError);
  if (code != 0 || failed) {
    return {};
  }
  return shortVersionLabel(output);
}

[[nodiscard]] std::string projectEnvironmentVersion(const ProjectManifest& manifest) {
  const std::string fromCfg = readKeyedValue(manifest.root / "venv" / "sere.cfg", "version");
  if (!fromCfg.empty()) {
    return fromCfg;
  }
  if (!manifest.sereVersion.empty()) {
    return manifest.sereVersion;
  }
  const std::string fromVenv = queryCompilerVersion(sereExecutable(manifest.root / "venv" / "bin"));
  if (!fromVenv.empty()) {
    return fromVenv;
  }
  return {};
}

bool patchTomlSereVersion(const std::filesystem::path& tomlPath, const std::string& version) {
  const std::string original = readTextFile(tomlPath);
  if (original.empty() && !std::filesystem::exists(tomlPath)) {
    return false;
  }
  std::istringstream input(original);
  std::ostringstream output;
  std::string line;
  bool replaced = false;
  std::string section;
  while (std::getline(input, line)) {
    const std::string trimmed = trimCopy(line);
    if (!trimmed.empty() && trimmed.front() == '[') {
      section = trimmed.substr(0, trimmed.find(']') + 1);
    }
    if ((section.empty() || section == "[toolchain]") && trimmed.rfind("sere", 0) == 0) {
      const std::size_t eq = trimmed.find('=');
      if (eq != std::string::npos && trimCopy(trimmed.substr(0, eq)) == "sere") {
        output << "sere = \"" << version << "\"\n";
        replaced = true;
        continue;
      }
    }
    output << line << '\n';
  }
  if (!replaced) {
    return writeKeyedFile(tomlPath, "sere = \"" + version + "\"\n" + output.str());
  }
  return writeKeyedFile(tomlPath, output.str());
}

[[nodiscard]] std::filesystem::path globalInstallPrefix() {
#ifdef _WIN32
  if (const char* local = std::getenv("LOCALAPPDATA")) {
    return std::filesystem::path(local) / "Programs" / "Sere";
  }
#endif
  return {};
}

[[nodiscard]] bool compilerFilesDiffer(const std::filesystem::path& fromExe,
                                       const std::filesystem::path& destExe) {
  std::error_code error;
  if (!std::filesystem::exists(destExe, error)) {
    return true;
  }
  if (samePath(fromExe, destExe)) {
    return false;
  }
  const std::string destVersion = shortVersionLabel(queryCompilerVersion(destExe));
  if (destVersion != SERE_VERSION_STRING) {
    return true;
  }
  error.clear();
  const auto fromSize = std::filesystem::file_size(fromExe, error);
  if (error) {
    return true;
  }
  error.clear();
  const auto destSize = std::filesystem::file_size(destExe, error);
  if (error || fromSize != destSize) {
    return true;
  }
  return false;
}

[[nodiscard]] int updateGlobalInstall(std::string& error) {
  const std::filesystem::path prefix = globalInstallPrefix();
  if (prefix.empty()) {
    error.clear();
    std::cout << "no global install prefix; skip system compiler update\n";
    return 0;
  }
  const std::filesystem::path fromDir = compilerDirectory();
  const std::filesystem::path fromExe = sereExecutable(fromDir);
  const std::filesystem::path destBin = prefix / "bin";
  const std::filesystem::path destExe = sereExecutable(destBin);
  std::error_code existsError;
  if (!std::filesystem::exists(fromExe, existsError)) {
    error.clear();
    std::cout << "this process is not sere.exe; skip system compiler update\n";
    return 0;
  }
  if (samePath(fromDir, destBin) || samePath(fromExe, destExe)) {
    std::cout << "system install is already this compiler (" << SERE_VERSION_STRING << ")\n";
    error.clear();
    return 0;
  }
  const std::string destVersion = shortVersionLabel(queryCompilerVersion(destExe));
  if (!compilerFilesDiffer(fromExe, destExe)) {
    std::cout << "system install already matches this compiler (" << SERE_VERSION_STRING << ")\n";
    error.clear();
    return 0;
  }
  std::error_code dirError;
  std::filesystem::create_directories(destBin, dirError);
  std::cout << "this compiler:    " << SERE_VERSION_STRING << "  (" << fromDir.string() << ")\n";
  std::cout << "system install:   "
            << (destVersion.empty() ? "(missing or unknown)" : destVersion) << "  ("
            << destBin.string() << ")\n";
  std::cout << "updating " << prefix.string() << "\n";
  if (copyCompilerBin(fromDir, destBin, error) != 0) {
    return 1;
  }
  copyIfExists(stdlibBesideCompiler(fromDir), prefix / "stdlib");
  const std::filesystem::path includeSrc = fromDir.parent_path() / "include";
  if (std::filesystem::exists(includeSrc / "sere" / "api" / "sere_mod.h", existsError)) {
    copyIfExists(includeSrc, prefix / "include");
  }
  error.clear();
  std::cout << "system compiler is now " << SERE_VERSION_STRING << "\n";
  return 0;
}

}  // namespace

int updateSereEnvironment(const std::filesystem::path& start, std::string& error) {
  if (updateGlobalInstall(error) != 0) {
    return 1;
  }
  std::error_code cwdError;
  const std::filesystem::path from =
      start.empty() ? std::filesystem::current_path(cwdError) : start;
  const std::optional<std::filesystem::path> root = findProjectRoot(from);
  const std::filesystem::path sourceDir = compilerDirectory();
  if (!root) {
    return 0;
  }
  ProjectManifest manifest;
  if (!loadProjectManifest(*root, manifest, error)) {
    return 1;
  }
  if (manifest.kind == ProjectKind::Lib) {
    std::error_code existsError;
    if (!std::filesystem::exists(*root / "venv" / "sere.cfg", existsError)) {
      std::cout << "library project has no local compiler environment to update\n";
      return 0;
    }
  }

  const std::string installed = SERE_VERSION_STRING;
  const std::string current = shortVersionLabel(projectEnvironmentVersion(manifest));

  std::cout << "this compiler:    " << installed << "  (" << sourceDir.string() << ")\n";
  std::cout << "project Sere:     " << (current.empty() ? "(unknown)" : current) << "  ("
            << root->string() << ")\n";

  std::error_code preludeError;
  const bool stdlibPresent =
      std::filesystem::exists(*root / "venv" / "stdlib" / "prelude.sere", preludeError);
  if (current == installed && stdlibPresent) {
    std::cout << "project already matches this compiler\n";
    return 0;
  }

  std::error_code dirError;
  std::filesystem::create_directories(*root / "venv" / "bin", dirError);
  std::filesystem::create_directories(*root / "venv" / "stdlib", dirError);
  std::filesystem::create_directories(*root / "bin", dirError);
  copyProjectToolchain(sourceDir, *root);
  std::string refreshError;
  (void)copyCompilerBin(sourceDir, *root / "bin", refreshError);
  (void)copyCompilerBin(sourceDir, *root / "venv" / "bin", refreshError);
  if (!writeText(*root / "venv" / "sere.cfg",
                 "home = " + sourceDir.string() + "\nstdlib = venv/stdlib\nversion = " +
                     installed + "\n")) {
    error = "cannot write venv/sere.cfg";
    return 1;
  }
  (void)patchTomlSereVersion(*root / "sere.toml", installed);
  std::cout << "updated compiler, stdlib, and venv to " << installed
            << " (project source left unchanged)\n";
  return 0;
}

}  // namespace sere
