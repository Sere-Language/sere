param([string]$Sere, [string]$Repo, [string]$Out)
$env:SERE_STDLIB = Join-Path $Repo "stdlib"
$env:Path = (Join-Path $Repo "build\windows-clang-cl-relwithdebinfo\bin") + ";" + $env:Path
foreach ($f in "gen", "async", "exc", "float") {
  foreach ($b in "llvm", "serem") {
    $src = Join-Path $Repo "_scratch\repro_$f.sere"
    $exe = Join-Path $Out "$f-$b.exe"
    $so = Join-Path $Out "$f-$b.out.txt"
    $se = Join-Path $Out "$f-$b.err.txt"
    Write-Output "=== $f / $b ==="
    $p = Start-Process -FilePath $Sere -ArgumentList @($src, "--backend=$b", "-o", $exe) -NoNewWindow -Wait -PassThru -RedirectStandardOutput $so -RedirectStandardError $se
    Write-Output "  compile exit=$($p.ExitCode)"
    if (Test-Path $se) { Get-Content $se | ForEach-Object { Write-Output "  C: $_" } }
    if (Test-Path $exe) {
      $ro = Join-Path $Out "$f-$b.run.txt"
      $re = Join-Path $Out "$f-$b.runerr.txt"
      $r = Start-Process -FilePath $exe -NoNewWindow -Wait -PassThru -RedirectStandardOutput $ro -RedirectStandardError $re
      Write-Output "  run exit=$($r.ExitCode)"
      if (Test-Path $ro) { Get-Content $ro | ForEach-Object { Write-Output "  OUT: $_" } }
      if (Test-Path $re) { Get-Content $re | ForEach-Object { Write-Output "  ERR: $_" } }
    }
  }
}
