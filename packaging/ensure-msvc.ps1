# ensure-msvc.ps1
# Installs Visual Studio Build Tools (C++ workload + Windows SDK) when link
# libraries required by sere are missing.

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

function Test-SereLinkLibs {
  $roots = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise",
    "${env:ProgramFiles}\Microsoft Visual Studio\2026\BuildTools"
  )
  $hasMsvc = $false
  foreach ($root in $roots) {
    if (-not (Test-Path $root)) { continue }
    $msvc = Get-ChildItem -Path (Join-Path $root "VC\Tools\MSVC") -Directory -ErrorAction SilentlyContinue |
      Where-Object { Test-Path (Join-Path $_.FullName "lib\x64\msvcprt.lib") } |
      Select-Object -First 1
    if ($msvc) { $hasMsvc = $true; break }
  }
  $sdkLib = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\Lib" -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName "um\x64\user32.lib") } |
    Select-Object -First 1
  return ($hasMsvc -and $sdkLib)
}

if (Test-SereLinkLibs) {
  Write-Host "C++ build tools and Windows SDK are already installed."
  exit 0
}

Write-Host "C++ build tools / Windows SDK not found. Installing Visual Studio Build Tools..."
$bootstrap = Join-Path $env:TEMP "sere-vs-BuildTools.exe"
$uri = "https://aka.ms/vs/17/release/vs_BuildTools.exe"
curl.exe -L --fail --retry 3 --retry-delay 5 --progress-bar -o $bootstrap $uri
if ($LASTEXITCODE -ne 0) {
  Write-Host "Failed to download Visual Studio Build Tools bootstrapper."
  exit 1
}

& $bootstrap --wait --passive --norestart `
  --add Microsoft.VisualStudio.Workload.VCTools `
  --includeRecommended
$code = $LASTEXITCODE
if ($code -ne 0 -and $code -ne 3010) {
  Write-Host "Visual Studio Build Tools installer exited $code"
  exit $code
}

if (-not (Test-SereLinkLibs)) {
  Write-Host "Build tools installed but link libraries were not found. A reboot may be required."
  exit 0
}

Write-Host "C++ build tools are ready."
exit 0
