# Copies repo stdlib/ into ./bin/stdlib (and any build/*/bin/stdlib that exists).
# Usage: .\scripts\refresh-stdlib.ps1

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

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
  Write-Host "updated $to"
}
