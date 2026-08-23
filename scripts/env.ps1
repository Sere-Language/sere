# env.ps1
# Loads MSVC vcvars and the Sere LLVM toolchain into the current PowerShell session.
# Usage: . .\scripts\env.ps1

$ErrorActionPreference = "Stop"

function Get-SereLlvmDir {
  if ($env:SERE_LLVM_DIR -and (Test-Path (Join-Path $env:SERE_LLVM_DIR "bin\clang.exe"))) {
    return $env:SERE_LLVM_DIR
  }
  $defaultDir = Join-Path $env:LOCALAPPDATA "sere\toolchains\llvm-22.1.8"
  if (Test-Path (Join-Path $defaultDir "bin\clang.exe")) {
    return $defaultDir
  }
  throw "LLVM 22.1.8 is not installed. Run scripts/bootstrap.ps1 first."
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
$env:CC = "clang-cl"
$env:CXX = "clang-cl"
if (-not $env:SERE_STDLIB) {
  $env:SERE_STDLIB = (Resolve-Path (Join-Path $PSScriptRoot "..\stdlib")).Path
}

Write-Host "Sere environment ready."
Write-Host "  SERE_LLVM_DIR=$env:SERE_LLVM_DIR"
Write-Host "  clang     = $(& clang --version | Select-Object -First 1)+"
Write-Host "  clang-cl  = $(& clang-cl --version | Select-Object -First 1)"
Write-Host "  cmake     = $(cmake --version | Select-Object -First 1)"
Write-Host "  ninja     = ninja $(ninja --version)"
