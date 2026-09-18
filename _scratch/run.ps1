Set-Location 'c:\Users\jackw\OneDrive\Desktop\git-projects\sere'
. .\scripts\env.ps1 | Out-Null
$out = '_scratch\result.txt'
Remove-Item $out -ErrorAction SilentlyContinue
cmake --build build\_dev --target sere *> build\_dev\build.log
"BUILD EXIT=$LASTEXITCODE" | Out-File -Encoding utf8 $out
Get-Content build\_dev\build.log -Tail 6 | Out-File -Append -Encoding utf8 $out
$sere = 'build\_dev\bin\sere.exe'
if (-not (Test-Path $sere)) { "NO COMPILER" | Out-File -Append -Encoding utf8 $out; exit 1 }
foreach ($name in @('fmt_repro', 'argcount_repro')) {
  "===== $name =====" | Out-File -Append -Encoding utf8 $out
  $exe = "_scratch\$name.exe"
  Remove-Item $exe -ErrorAction SilentlyContinue
  & $sere "_scratch\$name.sere" -o $exe *>> $out
  "COMPILE EXIT=$LASTEXITCODE" | Out-File -Append -Encoding utf8 $out
  if (Test-Path $exe) {
    & $exe *>> $out
    "RUN EXIT=$LASTEXITCODE" | Out-File -Append -Encoding utf8 $out
  }
}
