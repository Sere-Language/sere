# bootstrap-innosetup.ps1
# Installs Inno Setup 6 so `sere --build-installer` can compile the Windows setup exe.

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

function Test-Iscc {
  $candidates = @(
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
  )
  foreach ($path in $candidates) {
    if (Test-Path $path) { return $path }
  }
  $cmd = Get-Command ISCC -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  return $null
}

$existing = Test-Iscc
if ($existing) {
  Write-Host "Inno Setup already installed: $existing"
  exit 0
}

Write-Host "Installing Inno Setup 6 via winget..."
winget install --id JRSoftware.InnoSetup -e --accept-package-agreements --accept-source-agreements
if ($LASTEXITCODE -ne 0) {
  throw "winget failed to install Inno Setup (exit $LASTEXITCODE). Download it from https://jrsoftware.org/isinfo.php"
}

$found = Test-Iscc
if (-not $found) {
  throw "Inno Setup installed but ISCC.exe was not found. Re-open the terminal and retry."
}
Write-Host "ISCC: $found"
