# install.ps1
# Copies this release tree into a prefix and registers PATH / env vars.

[CmdletBinding()]
param(
  [string]$Prefix = (Join-Path $env:LOCALAPPDATA "Programs\Sere"),
  [switch]$NoPath,
  [switch]$Associate,
  [switch]$Editor,
  [switch]$Msvc,
  [switch]$DownloadLlvm
)

$ErrorActionPreference = "Stop"
$ReleaseRoot = $PSScriptRoot
$LlvmVersion = "22.1.8"
$ReleaseName = "Sere " + (Split-Path -Leaf $ReleaseRoot)
$ManifestPath = Join-Path $ReleaseRoot "MANIFEST.txt"
if (Test-Path $ManifestPath) {
  $first = (Get-Content -LiteralPath $ManifestPath -TotalCount 1).Trim()
  if ($first -and $first -like "Sere $(Split-Path -Leaf $ReleaseRoot)*") {
    $ReleaseName = $first
  }
}

function Test-Clang([string]$Root) {
  return (Test-Path (Join-Path $Root "bin\clang.exe"))
}

function Copy-Tree([string]$From, [string]$To) {
  if (-not (Test-Path $From)) {
    return
  }
  New-Item -ItemType Directory -Force -Path $To | Out-Null
  Copy-Item -Path (Join-Path $From "*") -Destination $To -Recurse -Force
}

function Copy-File([string]$From, [string]$To) {
  if (-not (Test-Path $From)) {
    return
  }
  $dir = Split-Path -Parent $To
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  Copy-Item -LiteralPath $From -Destination $To -Force
}

function Normalize-PathList([string]$text) {
  if ([string]::IsNullOrWhiteSpace($text)) {
    return @()
  }
  return @($text.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() })
}

function Add-UserPath([string]$dir) {
  $parts = [System.Collections.Generic.List[string]](Normalize-PathList (
      [Environment]::GetEnvironmentVariable("Path", "User")))
  $normalized = $dir.TrimEnd('\')
  $keep = @($parts | Where-Object { $_ -and ($_.TrimEnd('\') -ne $normalized) })
  $parts.Clear()
  $parts.Add($normalized)
  foreach ($item in $keep) {
    $parts.Add($item)
  }
  [Environment]::SetEnvironmentVariable("Path", ($parts -join ';'), "User")
  $session = [System.Collections.Generic.List[string]](Normalize-PathList $env:PATH)
  $sessionKeep = @($session | Where-Object { $_ -and ($_.TrimEnd('\') -ne $normalized) })
  $env:PATH = (@($normalized) + $sessionKeep) -join ';'
}

function Find-ExistingLlvm {
  $candidates = @(
    (Join-Path $ReleaseRoot "toolchains\llvm-$LlvmVersion"),
    (Join-Path $env:LOCALAPPDATA "sere\toolchains\llvm-$LlvmVersion"),
    (Join-Path $env:LOCALAPPDATA "Programs\Sere\toolchains\llvm-$LlvmVersion")
  )
  if ($env:SERE_LLVM_DIR) {
    $candidates = @($env:SERE_LLVM_DIR) + $candidates
  }
  foreach ($root in $candidates) {
    if ($root -and (Test-Clang $root)) {
      return $root
    }
  }
  return $null
}

function Install-Llvm([string]$DestRoot) {
  if (-not $DownloadLlvm -and (Test-Clang $DestRoot)) {
    Write-Host "LLVM $LlvmVersion already at $DestRoot"
    return $DestRoot
  }
  if (-not $DownloadLlvm) {
    $existing = Find-ExistingLlvm
    if ($existing) {
      if ($existing -eq $DestRoot) {
        Write-Host "LLVM $LlvmVersion already at $DestRoot"
        return $DestRoot
      }
      Write-Host "reusing LLVM $LlvmVersion from $existing"
      Copy-Tree $existing $DestRoot
      return $DestRoot
    }
  }
  $bootstrap = Join-Path $ReleaseRoot "packaging\bootstrap-llvm.ps1"
  if (-not (Test-Path $bootstrap)) {
    throw "LLVM $LlvmVersion not found and packaging\bootstrap-llvm.ps1 is missing. Pass -DownloadLlvm after staging, or install clang+llvm $LlvmVersion first."
  }
  Write-Host "downloading LLVM $LlvmVersion (this can take several minutes)"
  & $bootstrap -LlvmVersion $LlvmVersion
  if ($LASTEXITCODE -ne 0) {
    throw "LLVM download failed"
  }
  $downloaded = Join-Path $env:LOCALAPPDATA "sere\toolchains\llvm-$LlvmVersion"
  if (-not (Test-Clang $downloaded)) {
    throw "LLVM download finished but clang.exe is missing under $downloaded"
  }
  Copy-Tree $downloaded $DestRoot
  return $DestRoot
}

if (-not (Test-Path (Join-Path $ReleaseRoot "bin\sere.exe"))) {
  throw "bin\sere.exe is missing. Run ..\stage.ps1 from the Sere repo, or unzip a complete release."
}

Write-Host "installing $ReleaseName -> $Prefix"
New-Item -ItemType Directory -Force -Path $Prefix | Out-Null

Copy-Tree (Join-Path $ReleaseRoot "bin") (Join-Path $Prefix "bin")
Copy-Tree (Join-Path $ReleaseRoot "stdlib") (Join-Path $Prefix "stdlib")
Copy-Tree (Join-Path $ReleaseRoot "include") (Join-Path $Prefix "include")
Copy-Tree (Join-Path $ReleaseRoot "examples") (Join-Path $Prefix "examples")
Copy-Tree (Join-Path $ReleaseRoot "docs") (Join-Path $Prefix "docs")
Copy-Tree (Join-Path $ReleaseRoot "packaging") (Join-Path $Prefix "packaging")
Copy-Tree (Join-Path $ReleaseRoot "editors") (Join-Path $Prefix "editors")
Copy-File (Join-Path $ReleaseRoot "LICENSE") (Join-Path $Prefix "LICENSE")
Copy-File (Join-Path $ReleaseRoot "README.md") (Join-Path $Prefix "README.md")
Copy-File (Join-Path $ReleaseRoot "MANIFEST.txt") (Join-Path $Prefix "MANIFEST.txt")

$llvmDest = Join-Path $Prefix "toolchains\llvm-$LlvmVersion"
$llvmRoot = Install-Llvm $llvmDest

$bin = Join-Path $Prefix "bin"
$stdlib = Join-Path $Prefix "stdlib"
[Environment]::SetEnvironmentVariable("SERE_STDLIB", $stdlib, "User")
[Environment]::SetEnvironmentVariable("SERE_LLVM_DIR", $llvmRoot, "User")
$env:SERE_STDLIB = $stdlib
$env:SERE_LLVM_DIR = $llvmRoot

if (-not $NoPath) {
  Add-UserPath $bin
  Write-Host "user PATH starts with $bin"
}

if ($Associate) {
  $exe = Join-Path $bin "sere.exe"
  New-Item -Path "HKCU:\Software\Classes\.sere" -Force | Out-Null
  Set-ItemProperty -Path "HKCU:\Software\Classes\.sere" -Name "(default)" -Value "SereSourceFile"
  New-Item -Path "HKCU:\Software\Classes\SereSourceFile" -Force | Out-Null
  Set-ItemProperty -Path "HKCU:\Software\Classes\SereSourceFile" -Name "(default)" -Value "Sere Source File"
  New-Item -Path "HKCU:\Software\Classes\SereSourceFile\DefaultIcon" -Force | Out-Null
  Set-ItemProperty -Path "HKCU:\Software\Classes\SereSourceFile\DefaultIcon" -Name "(default)" -Value "$exe,0"
  New-Item -Path "HKCU:\Software\Classes\SereSourceFile\shell\open\command" -Force | Out-Null
  Set-ItemProperty -Path "HKCU:\Software\Classes\SereSourceFile\shell\open\command" -Name "(default)" -Value "`"$exe`" `"%1`""
  Write-Host "associated .sere with sere.exe"
}

if ($Editor) {
  $vsix = Join-Path $Prefix "editors\sere.vsix"
  $script = Join-Path $Prefix "packaging\install-vsix.ps1"
  if ((Test-Path $vsix) -and (Test-Path $script)) {
    & $script -Vsix $vsix -Compiler (Join-Path $bin "sere.exe")
  } else {
    Write-Host "editor VSIX not in this package; skip -Editor or run scripts\package-vsix.ps1 before staging"
  }
}

if ($Msvc) {
  $ensure = Join-Path $Prefix "packaging\ensure-msvc.ps1"
  if (Test-Path $ensure) {
    & $ensure
  }
}

Write-Host ""
Write-Host "$ReleaseName is installed."
Write-Host "  compiler  $(Join-Path $bin 'sere.exe')"
Write-Host "  stdlib    $stdlib"
Write-Host "  llvm      $llvmRoot"
Write-Host "Open a new terminal, then:  sere --version"
