# Puts the Sere compiler on PATH.
#   .\sere-path.ps1              this session
#   .\sere-path.ps1 -Persistent  this session + your user PATH (new terminals)

[CmdletBinding()]
param(
  [switch]$Persistent,
  [switch]$Remove
)

function Add-HomeFromCfg([string]$cfg, [System.Collections.Generic.List[string]]$dirs) {
  if (-not (Test-Path $cfg)) {
    return
  }
  foreach ($line in Get-Content $cfg) {
    if ($line -match '^\s*home\s*=\s*(.+)$') {
      $home = $Matches[1].Trim().Trim('"')
      if ($home) {
        $dirs.Add($home)
      }
    }
  }
}

function Find-SereBin {
  $here = $PSScriptRoot
  $dirs = [System.Collections.Generic.List[string]]@(
    $here,
    (Join-Path $here "bin"),
    (Join-Path $here "..\bin"),
    (Join-Path $here "..\venv\bin"),
    (Join-Path $here "..\build\windows-clang-cl-relwithdebinfo\bin")
  )
  Add-HomeFromCfg (Join-Path $here "sere.cfg") $dirs
  Add-HomeFromCfg (Join-Path $here "..\venv\sere.cfg") $dirs
  Add-HomeFromCfg (Join-Path $here "..\sere.cfg") $dirs
  foreach ($dir in $dirs) {
    $resolved = $null
    try {
      $resolved = (Resolve-Path $dir -ErrorAction Stop).Path
    } catch {
      continue
    }
    if ((Test-Path (Join-Path $resolved "sere.exe")) -or (Test-Path (Join-Path $resolved "sere"))) {
      return $resolved
    }
  }
  $cmd = Get-Command sere -ErrorAction SilentlyContinue
  if ($cmd) {
    return (Split-Path -Parent $cmd.Source)
  }
  return $null
}

function Normalize-PathList([string]$text) {
  if ([string]::IsNullOrWhiteSpace($text)) {
    return @()
  }
  return @($text.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() })
}

$bin = Find-SereBin
if (-not $bin) {
  Write-Error "sere.exe not found. Build the compiler or run this script from the folder that contains sere.exe."
  return
}

$sessionParts = [System.Collections.Generic.List[string]](Normalize-PathList $env:PATH)
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$userParts = [System.Collections.Generic.List[string]](Normalize-PathList $userPath)

function Remove-Dir([System.Collections.Generic.List[string]]$parts, [string]$dir) {
  $keep = @($parts | Where-Object { $_ -and ($_.TrimEnd('\') -ne $dir.TrimEnd('\')) })
  $parts.Clear()
  foreach ($item in $keep) {
    $parts.Add($item)
  }
}

if ($Remove) {
  Remove-Dir $sessionParts $bin
  $env:PATH = ($sessionParts -join ';')
  if ($Persistent) {
    Remove-Dir $userParts $bin
    [Environment]::SetEnvironmentVariable("Path", ($userParts -join ';'), "User")
    Write-Host "Removed from user PATH: $bin"
  }
  Write-Host "Removed from this session: $bin"
  return
}

Remove-Dir $sessionParts $bin
$sessionParts.Insert(0, $bin)
$env:PATH = ($sessionParts -join ';')
Write-Host "This session PATH starts with:"
Write-Host "  $bin"

if ($Persistent) {
  Remove-Dir $userParts $bin
  $userParts.Insert(0, $bin)
  [Environment]::SetEnvironmentVariable("Path", ($userParts -join ';'), "User")
  Write-Host "Saved on your user PATH (new terminals will see it)."
}

$sere = Join-Path $bin "sere.exe"
if (-not (Test-Path $sere)) {
  $sere = Join-Path $bin "sere"
}
Write-Host "sere -> $sere"
Write-Host "Try:  sere --help"
