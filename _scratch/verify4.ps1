Set-Location 'c:\Users\jackw\OneDrive\Desktop\git-projects\sere'
. .\scripts\env.ps1 | Out-Null
$ErrorActionPreference = 'Continue'
$log = '_scratch\eq.log'
Remove-Item $log -ErrorAction SilentlyContinue
$sere = 'build\_dev\bin\sere.exe'
Remove-Item '_scratch\eq.exe' -ErrorAction SilentlyContinue
& $sere '_scratch\eq.sere' -o '_scratch\eq.exe' 2>&1 |
  Where-Object { $_ -notmatch 'override-module|CategoryInfo|FullyQualifiedErrorId|At C:|^\+|^\s*$' } |
  Out-File -Encoding utf8 $log
"COMPILE=$LASTEXITCODE" | Out-File -Append -Encoding utf8 $log
if (Test-Path '_scratch\eq.exe') {
  & '_scratch\eq.exe' 2>&1 | Out-File -Append -Encoding utf8 $log
  "RUN=$LASTEXITCODE" | Out-File -Append -Encoding utf8 $log
}
