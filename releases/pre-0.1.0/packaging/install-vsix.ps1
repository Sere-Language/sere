# install-vsix.ps1
# Installs the Sere editor extension into Cursor and/or VS Code.
# Language server is sere --lsp (same binary as the compiler).

[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$Vsix,
  [Parameter(Mandatory = $true)]
  [string]$Compiler
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $Vsix)) {
  Write-Host "Sere VSIX not found at $Vsix"
  exit 0
}

function Find-EditorCli {
  param([string]$Name)
  $cmd = Get-Command $Name -ErrorAction SilentlyContinue
  if ($cmd) {
    return $cmd.Source
  }
  $candidates = @(
    (Join-Path $env:LOCALAPPDATA "Programs\cursor\Cursor.exe"),
    (Join-Path $env:LOCALAPPDATA "Programs\Microsoft VS Code\bin\code.cmd"),
    (Join-Path ${env:ProgramFiles} "Microsoft VS Code\bin\code.cmd"),
    (Join-Path ${env:ProgramFiles} "Cursor\Cursor.exe")
  )
  foreach ($path in $candidates) {
    if ($path -and (Test-Path $path)) {
      return $path
    }
  }
  return $null
}

function Install-Into {
  param([string]$Cli, [string]$Label)
  if (-not $Cli) {
    return $false
  }
  Write-Host "Installing Sere extension into $Label"
  if ($Cli -like "*.exe" -and $Label -eq "Cursor") {
    & $Cli --install-extension $Vsix --force
  } else {
    & $Cli --install-extension $Vsix --force
  }
  return $true
}

$installed = $false
if (Install-Into (Find-EditorCli "cursor") "Cursor") { $installed = $true }
if (Install-Into (Find-EditorCli "code") "VS Code") { $installed = $true }

if (-not $installed) {
  Write-Host "Neither Cursor nor VS Code was found on PATH."
  Write-Host "The VSIX is at $Vsix"
  Write-Host "Install it later with: Extensions -> Install from VSIX"
  Write-Host "Point sere.compilerPath at $Compiler if sere is not on PATH."
  exit 0
}

exit 0
