# stage.ps1
# Copies the current compiler build into releases/<name> for a zip install.

[CmdletBinding()]
param(
  [string]$Name = "pre-0.1.0"
)

$ErrorActionPreference = "Stop"

function Find-RepoRoot {
  $current = $PSScriptRoot
  for ($i = 0; $i -lt 6; $i++) {
    if ((Test-Path (Join-Path $current "CMakeLists.txt")) -and
        (Test-Path (Join-Path $current "stdlib\prelude.sere"))) {
      return $current
    }
    $parent = Split-Path -Parent $current
    if ($parent -eq $current) {
      break
    }
    $current = $parent
  }
  throw "run this script from the Sere repository"
}

function Copy-FileTo([string]$From, [string]$To) {
  if (-not (Test-Path $From)) {
    return $false
  }
  $dir = Split-Path -Parent $To
  if (-not (Test-Path $dir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
  }
  Copy-Item -LiteralPath $From -Destination $To -Force
  return $true
}

function Copy-TreeTo([string]$From, [string]$To) {
  if (-not (Test-Path $From)) {
    return $false
  }
  if (Test-Path $To) {
    Remove-Item -LiteralPath $To -Recurse -Force
  }
  New-Item -ItemType Directory -Force -Path $To | Out-Null
  Copy-Item -Path (Join-Path $From "*") -Destination $To -Recurse -Force
  return $true
}

$repo = Find-RepoRoot
$dest = Join-Path $repo "releases\$Name"
$binCandidates = @(
  (Join-Path $repo "bin\sere.exe"),
  (Join-Path $repo "build\windows-clang-cl-relwithdebinfo\bin\sere.exe")
)
$exe = $binCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
  throw "sere.exe not found. Build the compiler first."
}
$compilerDir = Split-Path -Parent $exe

Write-Host "staging $Name from $compilerDir"

New-Item -ItemType Directory -Force -Path (Join-Path $dest "bin") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dest "include\sere\api") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dest "examples") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dest "docs") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dest "packaging") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dest "editors") | Out-Null

if (-not (Copy-FileTo $exe (Join-Path $dest "bin\sere.exe"))) {
  throw "could not copy sere.exe"
}

$runtime = @(
  (Join-Path $compilerDir "sere_rt.lib"),
  (Join-Path $repo "bin\sere_rt.lib")
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $runtime -or -not (Copy-FileTo $runtime (Join-Path $dest "bin\sere_rt.lib"))) {
  throw "sere_rt.lib not found next to the compiler"
}

foreach ($qtFile in @("sere_qt6.lib", "sere_qt6.dll", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll")) {
  [void](Copy-FileTo (Join-Path $compilerDir $qtFile) (Join-Path $dest "bin\$qtFile"))
}
if (Test-Path (Join-Path $compilerDir "platforms")) {
  [void](Copy-TreeTo (Join-Path $compilerDir "platforms") (Join-Path $dest "bin\platforms"))
}

[void](Copy-FileTo (Join-Path $repo "bin\sere-path.ps1") (Join-Path $dest "bin\sere-path.ps1"))
[void](Copy-FileTo (Join-Path $repo "bin\sere-path.cmd") (Join-Path $dest "bin\sere-path.cmd"))
[void](Copy-FileTo (Join-Path $repo "scripts\sere-path.ps1") (Join-Path $dest "bin\sere-path.ps1"))
[void](Copy-FileTo (Join-Path $repo "scripts\sere-path.cmd") (Join-Path $dest "bin\sere-path.cmd"))

if (-not (Copy-TreeTo (Join-Path $repo "stdlib") (Join-Path $dest "stdlib"))) {
  throw "stdlib/ is missing"
}

[void](Copy-FileTo (Join-Path $repo "include\sere\api\sere_mod.h") (Join-Path $dest "include\sere\api\sere_mod.h"))
[void](Copy-FileTo (Join-Path $repo "include\sere\api\sere_gc.h") (Join-Path $dest "include\sere\api\sere_gc.h"))
[void](Copy-FileTo (Join-Path $repo "LICENSE") (Join-Path $dest "LICENSE"))
[void](Copy-FileTo (Join-Path $repo "docs\language.md") (Join-Path $dest "docs\language.md"))
[void](Copy-FileTo (Join-Path $repo "examples\hello.sere") (Join-Path $dest "examples\hello.sere"))
[void](Copy-FileTo (Join-Path $repo "packaging\ensure-msvc.ps1") (Join-Path $dest "packaging\ensure-msvc.ps1"))
[void](Copy-FileTo (Join-Path $repo "packaging\install-vsix.ps1") (Join-Path $dest "packaging\install-vsix.ps1"))
[void](Copy-FileTo (Join-Path $repo "scripts\bootstrap.ps1") (Join-Path $dest "packaging\bootstrap-llvm.ps1"))

$vsix = Get-ChildItem -Path (Join-Path $repo "editors\vscode") -Filter "*.vsix" -ErrorAction SilentlyContinue |
  Select-Object -First 1
if ($vsix) {
  [void](Copy-FileTo $vsix.FullName (Join-Path $dest "editors\sere.vsix"))
}

$stamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"
$lines = @(
  "Sere $Name",
  "staged: $stamp",
  "compiler: $exe"
)
Set-Content -LiteralPath (Join-Path $dest "MANIFEST.txt") -Value $lines -Encoding utf8

Write-Host "staged $dest"
Write-Host "install with:  .\$Name\install.ps1"
