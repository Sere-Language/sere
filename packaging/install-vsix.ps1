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

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Vsix)) {
  throw "Sere VSIX not found at $Vsix"
}

function Find-EditorCli {
  param([string]$Name)
  $cmd = Get-Command $Name -ErrorAction SilentlyContinue
  if ($cmd) {
    return $cmd.Source
  }
  $candidates = if ($Name -eq "cursor") {
    @("$env:LOCALAPPDATA\Programs\cursor\resources\app\bin\cursor.cmd",
      "$env:ProgramFiles\Cursor\resources\app\bin\cursor.cmd")
  } else {
    @("$env:LOCALAPPDATA\Programs\Microsoft VS Code\bin\code.cmd",
      "$env:ProgramFiles\Microsoft VS Code\bin\code.cmd")
  }
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
  if ($LASTEXITCODE -ne 0) { throw "$Label extension installation failed (exit $LASTEXITCODE)" }
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
