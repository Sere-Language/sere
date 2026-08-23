# uninstall.ps1
# Removes PATH / env entries and the prefix copy created by install.ps1.

[CmdletBinding()]
param(
  [string]$Prefix = (Join-Path $env:LOCALAPPDATA "Programs\Sere"),
  [switch]$KeepFiles
)

$ErrorActionPreference = "Stop"

function Normalize-PathList([string]$text) {
  if ([string]::IsNullOrWhiteSpace($text)) {
    return @()
  }
  return @($text.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() })
}

function Remove-UserPath([string]$dir) {
  $normalized = $dir.TrimEnd('\')
  $parts = [System.Collections.Generic.List[string]](Normalize-PathList (
      [Environment]::GetEnvironmentVariable("Path", "User")))
  $keep = @($parts | Where-Object { $_ -and ($_.TrimEnd('\') -ne $normalized) })
  [Environment]::SetEnvironmentVariable("Path", ($keep -join ';'), "User")
  $session = Normalize-PathList $env:PATH
  $env:PATH = (@($session | Where-Object { $_ -and ($_.TrimEnd('\') -ne $normalized) })) -join ';'
}

$bin = Join-Path $Prefix "bin"
Remove-UserPath $bin

$stdlib = [Environment]::GetEnvironmentVariable("SERE_STDLIB", "User")
$llvm = [Environment]::GetEnvironmentVariable("SERE_LLVM_DIR", "User")
if ($stdlib -and $stdlib.StartsWith($Prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
  [Environment]::SetEnvironmentVariable("SERE_STDLIB", $null, "User")
}
if ($llvm -and $llvm.StartsWith($Prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
  [Environment]::SetEnvironmentVariable("SERE_LLVM_DIR", $null, "User")
}

foreach ($key in @(
    "HKCU:\Software\Classes\.sere",
    "HKCU:\Software\Classes\SereSourceFile")) {
  if (Test-Path $key) {
    Remove-Item -LiteralPath $key -Recurse -Force
  }
}

if (-not $KeepFiles -and (Test-Path $Prefix)) {
  Remove-Item -LiteralPath $Prefix -Recurse -Force
  Write-Host "removed $Prefix"
} else {
  Write-Host "left files at $Prefix"
}

Write-Host "uninstalled Sere PATH and environment entries"
