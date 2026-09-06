# Activate this Sere project in the current PowerShell:
#   . .\scripts\activate.ps1
# Leave with: deactivate

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Find-SereCompiler([string]$ProjectRoot) {
  $candidates = @(
    (Join-Path $ProjectRoot 'venv\bin\sere.exe'),
    (Join-Path $ProjectRoot 'venv\bin\sere')
  )
  $cfg = Join-Path $ProjectRoot 'venv\sere.cfg'
  if (Test-Path $cfg) {
    foreach ($line in Get-Content $cfg) {
      if ($line -match '^\s*home\s*=\s*(.+)$') {
        $home = $Matches[1].Trim().Trim('"')
        $candidates += (Join-Path $home 'sere.exe')
        $candidates += (Join-Path $home 'sere')
      }
    }
  }
  $cmd = Get-Command sere -ErrorAction SilentlyContinue
  if ($cmd) { $candidates += $cmd.Source }
  foreach ($path in $candidates) {
    if ($path -and (Test-Path $path)) { return $path }
  }
  return $null
}

$Sourced = $MyInvocation.InvocationName -eq '.'
if (-not $Sourced) {
  Write-Host "Activate in this shell with:"
  Write-Host "  . .\scripts\activate.ps1"
  $sere = Find-SereCompiler $Root
  if ($sere) {
    Write-Host ""
    Write-Host "Starting a clean Sere shell..."
    Set-Location $Root
    & $sere shell --host powershell
  }
  return
}

if ($env:SERE_ACTIVE) {
  Write-Host "Already in $($env:SERE_PROJECT_NAME)."
  return
}

$Name = Split-Path $Root -Leaf
$toml = Join-Path $Root 'sere.toml'
if (Test-Path $toml) {
  $Section = ''
  foreach ($line in Get-Content $toml) {
    if ($line -match '^\s*\[([^]]+)\]') { $Section = $Matches[1]; continue }
    if ($Section -in @('', 'project') -and $line -match '^\s*name\s*=\s*"?([^"#]+)"?') { $Name = $Matches[1].Trim() }
  }
}

$Sere = Find-SereCompiler $Root
$SereBin = if ($Sere) { Split-Path $Sere -Parent } else { Join-Path $Root 'venv\bin' }
$VenvBin = Join-Path $Root 'venv\bin'
$Stdlib = Join-Path $Root 'venv\stdlib'
if (-not (Test-Path (Join-Path $Stdlib 'prelude.sere'))) {
  $near = Join-Path $SereBin 'stdlib'
  if (Test-Path (Join-Path $near 'prelude.sere')) { $Stdlib = $near }
}

$global:_SerePrev = @{
  PATH = $env:PATH
  SERE_ACTIVE = $env:SERE_ACTIVE
  SERE_PROJECT_ROOT = $env:SERE_PROJECT_ROOT
  SERE_PROJECT_NAME = $env:SERE_PROJECT_NAME
  SERE_VENV_BIN = $env:SERE_VENV_BIN
  SERE_STDLIB = $env:SERE_STDLIB
  SERE_HOME = $env:SERE_HOME
  Prompt = $function:prompt
}

$env:SERE_ACTIVE = '1'
$env:SERE_PROJECT_ROOT = $Root
$env:SERE_PROJECT_NAME = $Name
$env:SERE_VENV_BIN = $VenvBin
$env:SERE_STDLIB = $Stdlib
$env:SERE_HOME = $SereBin
$env:PATH = "$SereBin;$VenvBin;$env:PATH"
Set-Location $Root

function global:prompt {
  "(sere:$env:SERE_PROJECT_NAME) $($executionContext.SessionState.Path.CurrentLocation.ProviderPath)> "
}

function global:deactivate {
  if (-not $global:_SerePrev) { return }
  $env:PATH = $global:_SerePrev.PATH
  $env:SERE_ACTIVE = $global:_SerePrev.SERE_ACTIVE
  $env:SERE_PROJECT_ROOT = $global:_SerePrev.SERE_PROJECT_ROOT
  $env:SERE_PROJECT_NAME = $global:_SerePrev.SERE_PROJECT_NAME
  $env:SERE_VENV_BIN = $global:_SerePrev.SERE_VENV_BIN
  $env:SERE_STDLIB = $global:_SerePrev.SERE_STDLIB
  $env:SERE_HOME = $global:_SerePrev.SERE_HOME
  if ($global:_SerePrev.Prompt) {
    Set-Item function:global:prompt $global:_SerePrev.Prompt
  }
  Remove-Item function:global:deactivate -ErrorAction SilentlyContinue
  Remove-Variable _SerePrev -Scope Global -ErrorAction SilentlyContinue
  Write-Host "Sere project deactivated."
}

Remove-Item function:Find-SereCompiler -ErrorAction SilentlyContinue
Write-Host "Sere project: $Name"
if ($Sere) { Write-Host "  compiler  $Sere" } else { Write-Host "  compiler  not found (run .\bin\sere-path.ps1)" }
Write-Host "  stdlib    $env:SERE_STDLIB"
Write-Host "  commands  sere build | sere run | sere clean | deactivate"
