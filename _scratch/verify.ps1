Set-Location 'c:\Users\jackw\OneDrive\Desktop\git-projects\sere'
. .\scripts\env.ps1 | Out-Null
$ErrorActionPreference = 'Continue'
$log = '_scratch\verify.log'
Remove-Item $log -ErrorAction SilentlyContinue
cmake --build build\_dev --target sere *> build\_dev\build.log
"BUILD EXIT=$LASTEXITCODE" | Out-File -Encoding utf8 $log
if ($LASTEXITCODE -ne 0) {
  Select-String -Path build\_dev\build.log -Pattern 'error' | Select-Object -First 12 |
    Out-String -Width 220 | Out-File -Append -Encoding utf8 $log
  exit 1
}
$sere = 'build\_dev\bin\sere.exe'
foreach ($n in @('v9', 'v4', 'v2', 'v3', 'print_repro')) {
  "===== $n =====" | Out-File -Append -Encoding utf8 $log
  Remove-Item "_scratch\$n.exe" -ErrorAction SilentlyContinue
  & $sere "_scratch\$n.sere" -o "_scratch\$n.exe" 2>&1 |
    Where-Object { $_ -notmatch 'override-module|CategoryInfo|FullyQualifiedErrorId|At C:|^\+|^\s*$' } |
    Out-File -Append -Encoding utf8 $log
  "COMPILE=$LASTEXITCODE" | Out-File -Append -Encoding utf8 $log
  if (Test-Path "_scratch\$n.exe") {
    & "_scratch\$n.exe" 2>&1 | Out-File -Append -Encoding utf8 $log
    "RUN=$LASTEXITCODE" | Out-File -Append -Encoding utf8 $log
  }
}
"DONE" | Out-File -Append -Encoding utf8 $log
