Set-Location 'c:\Users\jackw\OneDrive\Desktop\git-projects\sere'
. .\scripts\env.ps1 | Out-Null
$ErrorActionPreference = 'Continue'
$log = '_scratch\y.log'
Remove-Item $log -ErrorAction SilentlyContinue
$sere = 'build\_dev\bin\sere.exe'
foreach ($n in @('v9', 'v10')) {
  "===== $n =====" | Out-File -Append -Encoding utf8 $log
  & $sere "_scratch\$n.sere" -o "_scratch\$n.exe" 2>&1 | Out-File -Append -Encoding utf8 $log
  "EXIT=$LASTEXITCODE" | Out-File -Append -Encoding utf8 $log
  if (Test-Path "_scratch\$n.exe") {
    & "_scratch\$n.exe" 2>&1 | Out-File -Append -Encoding utf8 $log
    "RUN=$LASTEXITCODE" | Out-File -Append -Encoding utf8 $log
  }
}
"DONE" | Out-File -Append -Encoding utf8 $log
