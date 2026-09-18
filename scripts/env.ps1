# env.ps1
# Loads MSVC vcvars and the Sere LLVM toolchain into the current PowerShell session.
# Usage: . .\scripts\env.ps1

$ErrorActionPreference = "Stop"

function Test-SereLlvmDir([string]$dir) {
  if (-not $dir) { return $false }
  # A directory is only usable for building when it has the clang-cl driver and
  # the LLVM CMake package. Installed layouts also ship a trimmed runtime-only
  # toolchain (clang.exe, no clang-cl.exe) that must not be selected here.
  return (Test-Path (Join-Path $dir "bin\clang-cl.exe")) -and
         (Test-Path (Join-Path $dir "lib\cmake\llvm"))
}

function Get-SereLlvmDir {
  if (Test-SereLlvmDir $env:SERE_LLVM_DIR) {
    return $env:SERE_LLVM_DIR
  }
  $candidates = @(
    (Join-Path $env:LOCALAPPDATA "sere\toolchains\llvm-22.1.8"),
    (Join-Path $env:LOCALAPPDATA "Programs\Sere\toolchains\llvm-22.1.8")
  )
  foreach ($candidate in $candidates) {
    if (Test-SereLlvmDir $candidate) {
      return $candidate
    }
  }
  $found = $env:SERE_LLVM_DIR
  if ($found) {
    throw "LLVM 22.1.8 at '$found' cannot build Sere (clang-cl.exe or the LLVM CMake package is missing). Run scripts\bootstrap.ps1."
  }
  throw "LLVM 22.1.8 is not installed. Run scripts\bootstrap.ps1 first."
}

function Import-VcVars64 {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Build Tools were not found (vswhere.exe missing)."
  }
  $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $vsPath) {
    throw "MSVC x64 toolset is not installed. Install the C++ workload in Visual Studio Build Tools."
  }
  $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
  if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found at $vcvars"
  }
  cmd.exe /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
      $name = $matches[1]
      $value = $matches[2]
      [System.Environment]::SetEnvironmentVariable($name, $value, "Process")
    }
  }
}

Import-VcVars64
$llvmDir = Get-SereLlvmDir
$env:SERE_LLVM_DIR = ($llvmDir -replace '\\', '/')
$env:PATH = "$(Join-Path $llvmDir 'bin');$env:PATH"
# Point CC/CXX at the full path. A bare name is resolved against PATH and then
# cached by CMake, which breaks as soon as another clang-cl appears on PATH.
$env:CC = (Join-Path $llvmDir 'bin\clang-cl.exe')
$env:CXX = $env:CC
if (-not $env:SERE_STDLIB) {
  $env:SERE_STDLIB = (Resolve-Path (Join-Path $PSScriptRoot "..\stdlib")).Path
}

Write-Host "Sere environment ready."
Write-Host "  SERE_LLVM_DIR=$env:SERE_LLVM_DIR"
Write-Host "  clang     = $(& clang --version | Select-Object -First 1)+"
Write-Host "  clang-cl  = $(& $env:CC --version | Select-Object -First 1)"
Write-Host "  cmake     = $(cmake --version | Select-Object -First 1)"
Write-Host "  ninja     = ninja $(ninja --version)"
