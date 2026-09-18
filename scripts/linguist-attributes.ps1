<#
.SYNOPSIS
  Adds the Sere language-detection block to the .gitattributes of Sere repos.

.DESCRIPTION
  GitHub reads `.gitattributes` from every repository, so the directive has to
  exist in each project - not only in the compiler repo. New projects get it
  from `sere init`; this script backfills existing ones.

  The block is delimited by markers, so re-running only rewrites that block and
  never touches hand-written attributes.

.PARAMETER Path
  A project directory, or a directory containing several Sere projects.

.PARAMETER Recurse
  Walk subdirectories looking for projects.

.PARAMETER DryRun
  Report what would change without writing.

.EXAMPLE
  .\scripts\linguist-attributes.ps1 -Path . -DryRun
.EXAMPLE
  .\scripts\linguist-attributes.ps1 -Path C:\Users\me\sere-projects -Recurse
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string]$Path,
  [switch]$Recurse,
  [switch]$DryRun,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Begin = '# >>> sere linguist >>>'
$End = '# <<< sere linguist <<<'
$Block = @(
  $Begin
  '# GitHub language detection: https://github.com/Sere-Language/sere/blob/main/docs/linguist.md'
  '*.sere linguist-language=Sere linguist-detectable text eol=lf'
  '*.slib binary'
  '*.lib binary'
  '*.dll binary'
  '*.exe binary'
  $End
)

function Test-SereProject([string]$Dir) {
  if (Test-Path -LiteralPath (Join-Path $Dir 'sere.toml')) { return $true }
  return [bool](Get-ChildItem -LiteralPath $Dir -Filter '*.sere' -File -ErrorAction SilentlyContinue)
}

function Get-ProjectDirectories([string]$Root, [bool]$Deep) {
  $found = [System.Collections.Generic.List[string]]::new()
  $skip = @('.git', 'venv', 'bin', 'build', 'dist', 'node_modules', '.sere-lib')
  $queue = [System.Collections.Generic.Queue[string]]::new()
  $queue.Enqueue((Resolve-Path -LiteralPath $Root).Path)
  while ($queue.Count -gt 0) {
    $dir = $queue.Dequeue()
    if (Test-SereProject $dir) { $found.Add($dir) }
    if (-not $Deep) { continue }
    foreach ($child in Get-ChildItem -LiteralPath $dir -Directory -ErrorAction SilentlyContinue) {
      if ($skip -contains $child.Name) { continue }
      if ($child.Attributes -band [System.IO.FileAttributes]::ReparsePoint) { continue }
      $queue.Enqueue($child.FullName)
    }
  }
  return $found
}

function Update-Attributes([string]$Dir) {
  $file = Join-Path $Dir '.gitattributes'
  $existing = if (Test-Path -LiteralPath $file) { Get-Content -LiteralPath $file } else { @() }
  $kept = [System.Collections.Generic.List[string]]::new()
  $inside = $false
  foreach ($line in $existing) {
    if ($line.Trim() -eq $Begin) { $inside = $true; continue }
    if ($line.Trim() -eq $End) { $inside = $false; continue }
    if (-not $inside) { $kept.Add($line) }
  }
  while ($kept.Count -gt 0 -and $kept[$kept.Count - 1].Trim().Length -eq 0) {
    $kept.RemoveAt($kept.Count - 1)
  }
  $next = [System.Collections.Generic.List[string]]::new()
  $next.AddRange([string[]]$kept)
  if ($next.Count -gt 0) { $next.Add('') }
  $next.AddRange([string[]]$Block)

  $current = @(Get-Content -LiteralPath $file -ErrorAction SilentlyContinue)
  $changed = -not (Test-Path -LiteralPath $file) -or
             ($kept.Count -eq 0 -and $current.Count -eq 0) -or
             (($current -join "`n") -ne ($next -join "`n"))
  if (-not $changed) { return 'unchanged' }
  $verb = if (Test-Path -LiteralPath $file) { 'updated' } else { 'created' }
  if (-not $DryRun) { Set-Content -LiteralPath $file -Value $next -Encoding UTF8 }
  return $verb
}

$projects = Get-ProjectDirectories $Path $Recurse.IsPresent
if ($projects.Count -eq 0) {
  Write-Host "no Sere project found under '$Path' (looked for sere.toml or *.sere)" -ForegroundColor Yellow
  exit 1
}

$summary = @()
foreach ($dir in $projects) {
  $result = Update-Attributes $dir
  $summary += [pscustomobject]@{ Status = $result; Project = $dir }
}
$summary | Format-Table -AutoSize | Out-String -Width 120 | Write-Host
if ($DryRun) { Write-Host 'dry run: nothing was written' -ForegroundColor Yellow }
Write-Host ("{0} project(s): {1} updated, {2} unchanged" -f $projects.Count,
    @($summary | Where-Object Status -ne 'unchanged').Count,
    @($summary | Where-Object Status -eq 'unchanged').Count) -ForegroundColor Green
