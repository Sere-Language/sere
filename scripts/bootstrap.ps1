# bootstrap.ps1
# Downloads the official LLVM 22.1.8 Windows MSVC development archive (headers + libs + clang/lld)
# into %LOCALAPPDATA%\sere\toolchains so the toolchain never lives inside OneDrive.

[CmdletBinding()]
param(
  [string]$LlvmVersion = "22.1.8",
  [switch]$SkipDownload
)

$ErrorActionPreference = "Stop"

$LlvmTag = "llvmorg-$LlvmVersion"
$ArchiveName = "clang+llvm-$LlvmVersion-x86_64-pc-windows-msvc.tar.xz"
$Url = "https://github.com/llvm/llvm-project/releases/download/$LlvmTag/$ArchiveName"
$ToolchainRoot = Join-Path $env:LOCALAPPDATA "sere\toolchains"
$ArchivePath = Join-Path $ToolchainRoot $ArchiveName
$InstallDir = Join-Path $ToolchainRoot "llvm-$LlvmVersion"
$ExtractedName = "clang+llvm-$LlvmVersion-x86_64-pc-windows-msvc"

New-Item -ItemType Directory -Force -Path $ToolchainRoot | Out-Null

function Test-LlvmInstall {
  param([string]$Root)
  return (Test-Path (Join-Path $Root "bin\clang.exe")) -and
         (Test-Path (Join-Path $Root "lib\cmake\llvm\LLVMConfig.cmake"))
}

if (Test-LlvmInstall -Root $InstallDir) {
  Write-Host "LLVM $LlvmVersion already installed at $InstallDir"
} else {
  if (-not $SkipDownload) {
    if (-not (Test-Path $ArchivePath) -or (Get-Item $ArchivePath).Length -lt 100MB) {
      Write-Host "Downloading $Url"
      Write-Host "This is the LLVM development archive (~822 MB) with headers and libraries."
      curl.exe -L --fail --retry 3 --retry-delay 5 --progress-bar -o $ArchivePath $Url
      if ($LASTEXITCODE -ne 0) {
        throw "Failed to download LLVM archive (exit $LASTEXITCODE)."
      }
    } else {
      Write-Host "Using existing archive $ArchivePath"
    }
  } elseif (-not (Test-Path $ArchivePath)) {
    throw "Archive not found at $ArchivePath and -SkipDownload was set."
  }

  $staging = Join-Path $ToolchainRoot "tmp-extract-$LlvmVersion"
  if (Test-Path $staging) {
    Remove-Item -Recurse -Force $staging
  }
  New-Item -ItemType Directory -Force -Path $staging | Out-Null
  Write-Host "Extracting $ArchivePath"
  tar -xf $ArchivePath -C $staging
  if ($LASTEXITCODE -ne 0) {
    throw "tar extraction failed (exit $LASTEXITCODE)."
  }

  $inner = Join-Path $staging $ExtractedName
  if (-not (Test-Path $inner)) {
    $found = Get-ChildItem $staging -Directory | Select-Object -First 1
    if (-not $found) {
      throw "Extracted archive did not contain a directory."
    }
    $inner = $found.FullName
  }

  if (Test-Path $InstallDir) {
    Remove-Item -Recurse -Force $InstallDir
  }
  Move-Item $inner $InstallDir
  Remove-Item -Recurse -Force $staging -ErrorAction SilentlyContinue

  if (-not (Test-LlvmInstall -Root $InstallDir)) {
    throw "LLVM extraction succeeded but clang/LLVMConfig.cmake were not found in $InstallDir"
  }
}

$env:SERE_LLVM_DIR = $InstallDir
Write-Host ""
Write-Host "LLVM $LlvmVersion is ready:"
Write-Host "  $InstallDir"
Write-Host ""
Write-Host "Next:"
Write-Host "  . .\scripts\env.ps1"
Write-Host "  cmake --preset windows-clang-cl-relwithdebinfo"
Write-Host "  cmake --build --preset windows-clang-cl-relwithdebinfo"
Write-Host "  ctest --preset windows-clang-cl-relwithdebinfo"
