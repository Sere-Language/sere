# Copies repo stdlib/ into ./bin/stdlib (and any build/*/bin/stdlib that exists).
# Windows-only modules are left out unless the host is Windows, matching the
# staging performed by scripts/copy_compiler_outputs.cmake.
# Usage: .\scripts\refresh-stdlib.ps1

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

# Standard-library modules that only compile against Windows APIs.
$WindowsOnlyStdlib = @("windows.sere")

function Test-HostIsWindows {
  # $IsWindows is only defined on PowerShell 6+; Windows PowerShell 5.1 is Windows.
  if (Get-Variable -Name IsWindows -ErrorAction SilentlyContinue) {
    return [bool]$IsWindows
  }
  return $true
}

function Remove-WindowsOnlyStdlib([string]$To) {
  if (Test-HostIsWindows) {
    return
  }
  foreach ($module in $WindowsOnlyStdlib) {
    $path = Join-Path $To $module
    if (Test-Path -LiteralPath $path) {
      Remove-Item -LiteralPath $path -Force
    }
  }
}

function Find-RepoRoot {
  $current = $PSScriptRoot
  for ($i = 0; $i -lt 6; $i++) {
    if ((Test-Path (Join-Path $current "CMakeLists.txt")) -and
        (Test-Path (Join-Path $current "stdlib\prelude.sere"))) {
      return (Resolve-Path $current).Path
    }
    $parent = Split-Path -Parent $current
    if ($parent -eq $current) {
      break
    }
    $current = $parent
  }
  throw "run this script from the Sere repository"
}

function Copy-StdlibTree([string]$From, [string]$To) {
  if (Test-Path $To) {
    Remove-Item -LiteralPath $To -Recurse -Force
  }
  New-Item -ItemType Directory -Force -Path $To | Out-Null
  Copy-Item -Path (Join-Path $From "*") -Destination $To -Recurse -Force
}

$repo = Find-RepoRoot
$from = Join-Path $repo "stdlib"
if (-not (Test-Path (Join-Path $from "prelude.sere"))) {
  throw "stdlib/prelude.sere is missing"
}

$dests = [System.Collections.Generic.List[string]]@((Join-Path $repo "bin\stdlib"))
$buildRoot = Join-Path $repo "build"
if (Test-Path $buildRoot) {
  Get-ChildItem -LiteralPath $buildRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
    $binDir = Join-Path $_.FullName "bin"
    if (Test-Path $binDir) {
      $dests.Add((Join-Path $binDir "stdlib"))
    }
  }
}

foreach ($to in $dests) {
  Copy-StdlibTree $from $to
  Remove-WindowsOnlyStdlib $to
  Write-Host "updated $to"
}
