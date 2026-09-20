#requires -Version 5.1
<#
.SYNOPSIS
Compile and execute every categorized Sere test with both native backends.
.EXAMPLE
.\tests\run-tests.ps1
.EXAMPLE
.\tests\run-tests.ps1 -Category features -Filter '*classes*' -ShowOutput
#>
[CmdletBinding()]
param(
  [string]$Compiler,
  [string]$TestRoot,
  [string[]]$Category = @('*'),
  [string]$Filter = '*',
  [ValidateSet('O0', 'O1', 'O2', 'O3', 'Os', 'Oz')]
  [string]$Optimization = 'O0',
  [ValidateRange(1, 3600)][int]$CompileTimeoutSeconds = 90,
  [ValidateRange(1, 3600)][int]$RunTimeoutSeconds = 30,
  [string]$OutputDirectory,
  [switch]$ShowOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $TestRoot) { $TestRoot = $PSScriptRoot }
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Write-TextFile([string]$Path, [string]$Text) {
  [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Quote-NativeArgument([string]$Value) {
  # Windows CommandLineToArgvW quoting, including trailing backslashes.
  return '"' + [regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}

function Invoke-TestProcess {
  param([string]$File, [string[]]$Arguments, [string]$WorkDirectory,
        [string]$LogPrefix, [int]$TimeoutSeconds, [string]$InputFile)
  $watch = [System.Diagnostics.Stopwatch]::StartNew()
  $process = New-Object System.Diagnostics.Process
  $info = $process.StartInfo
  $info.FileName = $File
  $info.Arguments = ($Arguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
  $info.WorkingDirectory = $WorkDirectory
  $info.UseShellExecute = $false
  $info.CreateNoWindow = $true
  $info.RedirectStandardOutput = $true
  $info.RedirectStandardError = $true
  $info.RedirectStandardInput = $true
  $info.StandardOutputEncoding = $utf8
  $info.StandardErrorEncoding = $utf8
  $info.EnvironmentVariables['SERE_STDLIB'] = Join-Path $repo 'stdlib'
  $stdout = ''
  $stderr = ''
  $exitCode = $null
  $timedOut = $false
  $startError = $null
  $started = $false
  try {
    if (-not $process.Start()) { throw 'Process did not start.' }
    $started = $true
    # Drain both pipes concurrently so verbose diagnostics cannot deadlock.
    $outTask = $process.StandardOutput.ReadToEndAsync()
    $errTask = $process.StandardError.ReadToEndAsync()
    if ($InputFile -and (Test-Path -LiteralPath $InputFile -PathType Leaf)) {
      $inputBytes = [System.IO.File]::ReadAllBytes($InputFile)
      $inputTask = $process.StandardInput.BaseStream.WriteAsync($inputBytes, 0, $inputBytes.Length)
    } else {
      $inputTask = $null
      $process.StandardInput.Close()
    }
    while (-not $process.WaitForExit(50)) {
      if ($null -ne $inputTask -and $inputTask.IsCompleted) {
        $process.StandardInput.Close()
        $inputTask = $null
      }
      if ($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
        $timedOut = $true
        break
      }
    }
    if ($timedOut) {
      # Kill the compiler/linker or test child tree, without opening a window.
      $killer = New-Object System.Diagnostics.Process
      $killer.StartInfo.FileName = Join-Path $env:SystemRoot 'System32/taskkill.exe'
      $killer.StartInfo.Arguments = "/PID $($process.Id) /T /F"
      $killer.StartInfo.UseShellExecute = $false
      $killer.StartInfo.CreateNoWindow = $true
      $killer.StartInfo.RedirectStandardOutput = $true
      $killer.StartInfo.RedirectStandardError = $true
      try {
        [void]$killer.Start()
        [void]$killer.StandardOutput.ReadToEndAsync()
        [void]$killer.StandardError.ReadToEndAsync()
        [void]$killer.WaitForExit(5000)
        if (-not $process.HasExited) { $process.Kill() }
        [void]$process.WaitForExit(5000)
      } finally { $killer.Dispose() }
    }
    if ($process.HasExited) { $exitCode = $process.ExitCode }
    if ($outTask.Wait(5000)) { $stdout = $outTask.Result }
    else { $stderr += "Output capture did not finish; a child may still hold the pipe open.`n" }
    if ($errTask.Wait(5000)) { $stderr += $errTask.Result }
  } catch {
    $startError = $_.Exception.Message
    $stderr += $startError
    if ($started -and -not $process.HasExited) { $process.Kill() }
  } finally {
    $process.Dispose()
    $watch.Stop()
  }
  Write-TextFile "$LogPrefix.stdout.log" $stdout
  Write-TextFile "$LogPrefix.stderr.log" $stderr
  return [pscustomobject]@{
    Command = $File; Arguments = @($Arguments); WorkingDirectory = $WorkDirectory
    ExitCode = $exitCode; TimedOut = $timedOut; Error = $startError
    Seconds = [math]::Round($watch.Elapsed.TotalSeconds, 3)
    Stdout = $stdout; Stderr = $stderr
    StdoutLog = "$LogPrefix.stdout.log"; StderrLog = "$LogPrefix.stderr.log"
  }
}

function Normalize-Output([string]$Text) { return $Text.Replace("`r`n", "`n") }

function Describe-ExitCode($Code) {
  if ($null -eq $Code) { return 'Process could not start' }
  if ($Code -lt 0) {
    $hex = [System.BitConverter]::ToUInt32([System.BitConverter]::GetBytes([int]$Code), 0).ToString('X8')
    $hint = switch ($hex) {
      'C0000005' { ' (access violation)' }
      'C00000FD' { ' (stack overflow)' }
      'C000001D' { ' (illegal instruction)' }
      'C0000135' { ' (missing DLL)' }
      default { ' (process crash)' }
    }
    return "Exit $Code / 0x$hex$hint"
  }
  return "Exit $Code"
}

try {
  $root = (Resolve-Path -LiteralPath $TestRoot).Path
  if (-not $Compiler) { $Compiler = Join-Path $repo 'bin/sere.exe' }
  $compilerCommand = Get-Command $Compiler -CommandType Application -ErrorAction Stop
  $Compiler = $compilerCommand.Source
  if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'build/test-results' }
  $runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
  $runDirectory = [System.IO.Path]::GetFullPath((Join-Path $OutputDirectory $runId))
  $tests = @(
    foreach ($folder in Get-ChildItem -LiteralPath $root -Directory | Sort-Object Name) {
      $matchesCategory = @($Category | Where-Object { $folder.Name -like $_ }).Count -gt 0
      if (-not $matchesCategory) { continue }
      foreach ($file in Get-ChildItem -LiteralPath $folder.FullName -Filter '*.sere' -File -Recurse | Sort-Object FullName) {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
        if ($relative -like $Filter) {
          [pscustomobject]@{ Category = $folder.Name; Name = $relative; Source = $file.FullName }
        }
      }
    }
  )
  if ($tests.Count -eq 0) { throw "No .sere tests matched under '$root' (category: $Category; filter: $Filter)." }
  [void][System.IO.Directory]::CreateDirectory($runDirectory)
} catch {
  Write-Host "Test setup failed: $($_.Exception.Message)" -ForegroundColor Red
  exit 2
}

$results = New-Object 'System.Collections.Generic.List[object]'
$suiteWatch = [System.Diagnostics.Stopwatch]::StartNew()
Write-Host "`nSERE TESTS" -ForegroundColor Cyan
Write-Host "Compiler: $Compiler"
Write-Host "Tests: $($tests.Count) | Backends: serem, llvm | Optimization: -$Optimization"
Write-Host "Timeouts: compile ${CompileTimeoutSeconds}s, run ${RunTimeoutSeconds}s"
Write-Host "Results: $runDirectory"
Write-Host 'PASS means compilation and execution exited 0, plus any .stdout expectation matched.'
$currentCategory = ''
$index = 0
try {
  foreach ($test in $tests) {
    $index++
    if ($currentCategory -ne $test.Category) {
      $currentCategory = $test.Category
      Write-Host "`n[$currentCategory]" -ForegroundColor Cyan
    }
    foreach ($backend in @('serem', 'llvm')) {
      $caseDirectory = Join-Path $runDirectory "$($test.Name)/$backend"
      [void][System.IO.Directory]::CreateDirectory($caseDirectory)
      $exe = Join-Path $caseDirectory 'test.exe'
      $compileArgs = @($test.Source, "--backend=$backend", "-$Optimization", '--no-color', '-o', $exe)
      Write-Host ("  [{0}/{1}] {2} ({3}) ... " -f $index, $tests.Count, $test.Name, $backend) -NoNewline
      $compile = Invoke-TestProcess -File $Compiler -Arguments $compileArgs -WorkDirectory $caseDirectory -LogPrefix (Join-Path $caseDirectory 'compile') -TimeoutSeconds $CompileTimeoutSeconds
      $run = $null
      $status = 'PASS'
      $reason = ''
      if ($compile.TimedOut) { $status = 'COMPILE TIMEOUT'; $reason = "Exceeded ${CompileTimeoutSeconds}s" }
      elseif ($compile.Error -or $compile.ExitCode -ne 0) { $status = 'COMPILE FAIL'; $reason = "$(Describe-ExitCode $compile.ExitCode) $($compile.Error)" }
      elseif (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { $status = 'COMPILE FAIL'; $reason = 'Compiler returned 0 but produced no executable.' }
      else {
        $run = Invoke-TestProcess -File $exe -Arguments @() -WorkDirectory $caseDirectory -LogPrefix (Join-Path $caseDirectory 'run') -TimeoutSeconds $RunTimeoutSeconds -InputFile ([System.IO.Path]::ChangeExtension($test.Source, '.stdin'))
        if ($run.TimedOut) { $status = 'RUN TIMEOUT'; $reason = "Exceeded ${RunTimeoutSeconds}s" }
        elseif ($run.Error -or $run.ExitCode -ne 0) { $status = 'RUN FAIL'; $reason = "$(Describe-ExitCode $run.ExitCode) $($run.Error)" }
        else {
          $expectedPath = [System.IO.Path]::ChangeExtension($test.Source, '.stdout')
          if (Test-Path -LiteralPath $expectedPath -PathType Leaf) {
            $expected = [System.IO.File]::ReadAllText($expectedPath)
            if ((Normalize-Output $run.Stdout) -cne (Normalize-Output $expected)) {
              $status = 'OUTPUT FAIL'
              $reason = "Standard output differs from $expectedPath (only CRLF/LF is normalized)."
            }
          }
        }
      }
      $runSeconds = 0
      if ($null -ne $run) { $runSeconds = $run.Seconds }
      $result = [pscustomobject]@{
        Test = $test.Name; Category = $test.Category; Source = $test.Source
        Backend = $backend; Status = $status; Reason = $reason
        Seconds = [math]::Round(($compile.Seconds + $runSeconds), 3)
        Compile = $compile; Run = $run; Artifacts = $caseDirectory
      }
      $results.Add($result)
      $color = if ($status -eq 'PASS') { 'Green' } else { 'Red' }
      Write-Host ("{0}  (compile {1:N2}s, run {2:N2}s)" -f $status, $compile.Seconds, $runSeconds) -ForegroundColor $color
      if ($status -ne 'PASS') {
        Write-Host "    $reason" -ForegroundColor Red
        $failedProcess = if ($null -ne $run) { $run } else { $compile }
        $detail = ($failedProcess.Stderr + "`n" + $failedProcess.Stdout).Trim()
        if ($detail) {
          $detail -split '\r?\n' | Select-Object -First 14 | ForEach-Object { Write-Host "    $_" }
        }
        Write-Host "    Full logs: $caseDirectory" -ForegroundColor DarkGray
      } elseif ($ShowOutput -and $null -ne $run) {
        if ($run.Stdout) { Write-Host $run.Stdout.TrimEnd() }
        if ($run.Stderr) { Write-Host $run.Stderr.TrimEnd() -ForegroundColor Yellow }
      }
    }
  }
} catch {
  Write-Host "`nRunner error: $($_.Exception.Message)" -ForegroundColor Red
  $runnerError = $_.Exception.Message
} finally {
  $suiteWatch.Stop()
  if (-not (Get-Variable runnerError -ErrorAction SilentlyContinue)) { $runnerError = $null }
  $passed = @($results | Where-Object Status -eq 'PASS').Count
  $failed = $results.Count - $passed
  $report = [pscustomobject]@{
    Timestamp = (Get-Date).ToString('o'); Compiler = $Compiler; TestRoot = $root
    Optimization = $Optimization; CompileTimeoutSeconds = $CompileTimeoutSeconds
    RunTimeoutSeconds = $RunTimeoutSeconds; ExpectedRuns = $tests.Count * 2
    CompletedRuns = $results.Count; Passed = $passed; Failed = $failed
    Seconds = [math]::Round($suiteWatch.Elapsed.TotalSeconds, 3)
    RunnerError = $runnerError; Results = @($results.ToArray())
  }
  Write-TextFile (Join-Path $runDirectory 'results.json') ($report | ConvertTo-Json -Depth 8)
  $lines = New-Object 'System.Collections.Generic.List[string]'
  $lines.Add('# Sere test results')
  $lines.Add('')
  $lines.Add("Compiler: ``$Compiler``; optimization: ``-$Optimization``.")
  $lines.Add("Completed $($results.Count)/$($tests.Count * 2) backend runs: $passed passed, $failed failed in $($report.Seconds)s.")
  if ($runnerError) { $lines.Add("Runner error: $runnerError") }
  $lines.Add('')
  $lines.Add('| Test | serem | llvm | Comparison |')
  $lines.Add('| --- | --- | --- | --- |')
  Write-Host "`nBACKEND COMPARISON" -ForegroundColor Cyan
  foreach ($test in $tests) {
    $pair = @($results | Where-Object Test -eq $test.Name)
    $serem = $pair | Where-Object Backend -eq 'serem' | Select-Object -First 1
    $llvm = $pair | Where-Object Backend -eq 'llvm' | Select-Object -First 1
    $left = if ($null -ne $serem) { $serem.Status } else { 'NOT RUN' }
    $right = if ($null -ne $llvm) { $llvm.Status } else { 'NOT RUN' }
    $comparison = ''
    if (($left -eq 'PASS') -ne ($right -eq 'PASS')) { $comparison = 'BACKEND DIFFERENCE' }
    elseif ($left -eq 'PASS' -and $right -eq 'PASS' -and
            (Normalize-Output $serem.Run.Stdout) -cne (Normalize-Output $llvm.Run.Stdout)) {
      $comparison = 'stdout differs (informational)'
    }
    Write-Host ("  {0,-42} serem: {1,-16} llvm: {2,-16} {3}" -f $test.Name, $left, $right, $comparison)
    $lines.Add("| $($test.Name.Replace('|', '\|')) | $left | $right | $comparison |")
  }
  Write-Host "`nSUMMARY" -ForegroundColor Cyan
  foreach ($group in $results | Group-Object Category, Backend) {
    $ok = @($group.Group | Where-Object Status -eq 'PASS').Count
    $summary = "{0}: {1}/{2} passed, {3} failed" -f $group.Name, $ok, $group.Count, ($group.Count - $ok)
    Write-Host "  $summary"
    $lines.Add('')
    $lines.Add($summary)
  }
  Write-Host ("  Total: {0} passed, {1} failed, {2} not run | {3:N2}s" -f $passed, $failed, ($tests.Count * 2 - $results.Count), $report.Seconds)
  Write-Host '  Slowest backend runs:'
  foreach ($slow in $results | Sort-Object Seconds -Descending | Select-Object -First 5) {
    Write-Host ("    {0:N2}s  {1} ({2})" -f $slow.Seconds, $slow.Test, $slow.Backend)
  }
  $lines.Add('')
  $lines.Add('## Failures')
  foreach ($failure in $results | Where-Object Status -ne 'PASS') {
    $lines.Add('')
    $lines.Add("- $($failure.Test) ($($failure.Backend)): $($failure.Status). $($failure.Reason)")
    $lines.Add("  Logs: ``$($failure.Artifacts)``")
  }
  Write-TextFile (Join-Path $runDirectory 'summary.md') ($lines -join "`n")
  Write-Host "`nSaved summary.md, results.json, and full process logs to:`n  $runDirectory"
}
if ($runnerError -or $results.Count -ne $tests.Count * 2) { exit 2 }
if ($failed -gt 0) { exit 1 }
exit 0
