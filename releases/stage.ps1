# stage.ps1
# Builds a shippable tree under releases/<name> and zips it.

[CmdletBinding()]
param(
  [string]$Name = "pre-0.1.1"
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
$dist = Join-Path $repo "dist"
$dest = Join-Path $repo "releases\$Name"
$zip = Join-Path $repo "releases\Sere-$Name-windows-x64.zip"
$payloadScripts = Join-Path $repo "releases\pre-0.1.0"

$binCandidates = @(
  (Join-Path $repo "bin\sere.exe"),
  (Join-Path $repo "build\windows-clang-cl-relwithdebinfo\bin\sere.exe")
)
$exe = $binCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
  throw "sere.exe not found. Build the compiler first."
}
$compilerDir = Split-Path -Parent $exe

Write-Host "staging $Name from $compilerDir -> $dest"

if (Test-Path $dest) {
  Remove-Item -LiteralPath $dest -Recurse -Force
}

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

foreach ($extra in @("icon.ico", "sere_icon.res")) {
  $found = @(
    (Join-Path $compilerDir $extra),
    (Join-Path $repo "bin\$extra"),
    (Join-Path $repo $extra)
  ) | Where-Object { Test-Path $_ } | Select-Object -First 1
  if ($found) {
    [void](Copy-FileTo $found (Join-Path $dest "bin\$extra"))
  }
}

foreach ($qtFile in @("sere_qt6.lib", "sere_qt6.dll", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll")) {
  [void](Copy-FileTo (Join-Path $compilerDir $qtFile) (Join-Path $dest "bin\$qtFile"))
}
if (Test-Path (Join-Path $compilerDir "platforms")) {
  [void](Copy-TreeTo (Join-Path $compilerDir "platforms") (Join-Path $dest "bin\platforms"))
}

[void](Copy-FileTo (Join-Path $repo "scripts\sere-path.ps1") (Join-Path $dest "bin\sere-path.ps1"))
[void](Copy-FileTo (Join-Path $repo "scripts\sere-path.cmd") (Join-Path $dest "bin\sere-path.cmd"))
[void](Copy-FileTo (Join-Path $repo "scripts\sere-path.sh") (Join-Path $dest "bin\sere-path.sh"))

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
[void](Copy-FileTo (Join-Path $payloadScripts "install.ps1") (Join-Path $dest "install.ps1"))
[void](Copy-FileTo (Join-Path $payloadScripts "install.cmd") (Join-Path $dest "install.cmd"))
[void](Copy-FileTo (Join-Path $payloadScripts "uninstall.ps1") (Join-Path $dest "uninstall.ps1"))
[void](Copy-FileTo (Join-Path $payloadScripts "uninstall.cmd") (Join-Path $dest "uninstall.cmd"))

$vsix = Get-ChildItem -Path $dist -Filter "sere-*.vsix" -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
if ($vsix) {
  [void](Copy-FileTo $vsix.FullName (Join-Path $dest "editors\sere.vsix"))
  [void](Copy-FileTo $vsix.FullName (Join-Path $repo "releases\$($vsix.Name)"))
}

$stamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"
$lines = @(
  "Sere $Name",
  "staged: $stamp",
  "compiler: $exe"
)
if ($vsix) {
  $lines += "vsix: $($vsix.Name)"
}
Set-Content -LiteralPath (Join-Path $dest "MANIFEST.txt") -Value $lines -Encoding utf8

$readme = @"
# Sere $Name

Windows x64 package. Unzip this folder, then run ``install.ps1`` (or
``install.cmd``) to copy files to ``%LOCALAPPDATA%\Programs\Sere``, add ``bin``
to the user PATH, and set ``SERE_STDLIB`` / ``SERE_LLVM_DIR``.

``````powershell
.\install.ps1
sere --version
``````

The editor VSIX is ``editors\sere.vsix``. Pass ``-Editor`` to install it, or
use **Extensions → Install from VSIX…** in Cursor / VS Code.
"@
Set-Content -LiteralPath (Join-Path $dest "README.md") -Value $readme.TrimStart() -Encoding utf8

if (Test-Path $zip) {
  Remove-Item -LiteralPath $zip -Force
}
$zipStaging = Join-Path ([System.IO.Path]::GetTempPath()) ("sere-zip-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $zipStaging | Out-Null
try {
  Copy-Item -LiteralPath $dest -Destination (Join-Path $zipStaging $Name) -Recurse -Force
  Compress-Archive -Path (Join-Path $zipStaging $Name) -DestinationPath $zip -Force
} finally {
  Remove-Item -LiteralPath $zipStaging -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "staged $dest"
Write-Host "zip     $zip"
if ($vsix) {
  Write-Host "vsix    $($vsix.FullName)"
}
Write-Host "install with:  $dest\install.ps1"
