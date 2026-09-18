$ErrorActionPreference = 'Continue'
. "$PSScriptRoot\..\scripts\env.ps1" | Out-Null
$sere = Join-Path $PSScriptRoot '..\build\_dev\bin\sere.exe'
if (-not (Test-Path $sere)) { Write-Host "MISSING $sere"; exit 1 }
$log = Join-Path $PSScriptRoot 'docs_check.log'
Set-Content -Path $log -Value "sere: $sere" -Encoding utf8
$dir = Join-Path $PSScriptRoot 'docs'
foreach ($f in (Get-ChildItem $dir -Filter 'd*.sere' | Sort-Object Name)) {
  $exe = Join-Path $dir ($f.BaseName + '.exe')
  $compile = & $sere $f.FullName -o $exe 2>&1 | Out-String
  Add-Content -Path $log -Value ("`n=== " + $f.Name + " compile exit=$LASTEXITCODE")
  if ($compile.Trim().Length -gt 0) { Add-Content -Path $log -Value $compile.Trim() }
  if (Test-Path $exe) {
    $run = & $exe 2>&1 | Out-String
    Add-Content -Path $log -Value ("--- run exit=$LASTEXITCODE")
    Add-Content -Path $log -Value $run.Trim()
  }
}
Add-Content -Path $log -Value "`nDONE"
Get-Content $log
