# Adds Sere to github-linguist and opens the pull request.
# Everything (clone, logs) stays under _scratch\linguist-pr so it can be inspected.
$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$work = Join-Path $here 'linguist-pr'
$log = Join-Path $work 'run.log'
$repoRoot = Split-Path -Parent $here

function Log([string]$m) { Add-Content -LiteralPath $log -Value $m }
function Die([string]$m) {
  Log "FAIL: $m"
  Set-Content -LiteralPath (Join-Path $here 'status.txt') -Value "failed: $m"
  exit 1
}

New-Item -ItemType Directory -Force -Path $work | Out-Null
Set-Content -LiteralPath $log -Value "start $(Get-Date -Format s)"

Add-Content -LiteralPath $log -Value 'step: clone'
$cloneDir = Join-Path $work 'linguist'
if (Test-Path $cloneDir) { Remove-Item $cloneDir -Recurse -Force }
git clone --depth 1 https://github.com/github-linguist/linguist.git $cloneDir *>> $log
if (-not (Test-Path (Join-Path $cloneDir 'lib\linguist\languages.yml'))) { Die 'clone produced no languages.yml' }
Add-Content -LiteralPath $log -Value 'ok: clone'

Add-Content -LiteralPath $log -Value 'step: branch'
git -C $cloneDir switch -c add-sere-language *>> $log
if ($LASTEXITCODE -ne 0) { Die 'could not create branch' }
Add-Content -LiteralPath $log -Value 'ok: branch'

Add-Content -LiteralPath $log -Value 'step: languages.yml'
$langPath = Join-Path $cloneDir 'lib\linguist\languages.yml'
$lines = [System.IO.File]::ReadAllText($langPath) -split "`n"
$entry = @(
  'Sere:'
  '  type: programming'
  '  color: "#6f42c1"'
  '  extensions:'
  '    - ".sere"'
  '  tm_scope: "source.sere"'
  '  ace_mode: "text"'
  ''
)
$insertAt = $lines.Count
for ($i = 0; $i -lt $lines.Count; $i++) {
  $line = $lines[$i].TrimEnd("`r")
  if ($line -match '^([A-Za-z0-9_.+\-]+):\s*$') {
    if ([System.String]::CompareOrdinal($Matches[1], 'Sere') -gt 0) { $insertAt = $i; break }
  }
}
$next = New-Object System.Collections.Generic.List[string]
if ($insertAt -gt 0) { $next.AddRange([string[]]$lines[0..($insertAt - 1)]) }
$next.AddRange([string[]]$entry)
if ($insertAt -lt $lines.Count) { $next.AddRange([string[]]$lines[$insertAt..($lines.Count - 1)]) }
[System.IO.File]::WriteAllText($langPath, ($next -join "`n"), (New-Object System.Text.UTF8Encoding($false)))
Add-Content -LiteralPath $log -Value "ok: inserted Sere before line $($insertAt + 1) [$($lines[$insertAt].TrimEnd())]"

Add-Content -LiteralPath $log -Value 'step: samples'
$sampleTarget = Join-Path $cloneDir 'samples\Sere'
New-Item -ItemType Directory -Force -Path $sampleTarget | Out-Null
Copy-Item -Path (Join-Path $repoRoot '.github\linguist\samples\Sere\*.sere') -Destination $sampleTarget -Force
Add-Content -LiteralPath $log -Value "ok: samples $((Get-ChildItem $sampleTarget -File).Name -join ', ')"

Add-Content -LiteralPath $log -Value 'step: commit'
git -C $cloneDir add lib/linguist/languages.yml samples *>> $log
git -C $cloneDir -c user.name='Sere Language' -c user.email='noreply@sere-lang.com' commit -q -m 'Add Sere language' *>> $log
if ($LASTEXITCODE -ne 0) { Die 'commit failed' }
Add-Content -LiteralPath $log -Value 'ok: commit'
git -C $cloneDir show --stat --oneline HEAD *>> $log

Add-Content -LiteralPath $log -Value 'step: fork'
gh repo fork github-linguist/linguist --clone=false --remote=false *>> $log
Add-Content -LiteralPath $log -Value 'ok: fork'

Add-Content -LiteralPath $log -Value 'step: push'
git -C $cloneDir remote remove fork 2>$null *>> $log
git -C $cloneDir remote add fork https://github.com/youthx/linguist.git *>> $log
git -C $cloneDir push --force fork add-sere-language:add-sere-language *>> $log
if ($LASTEXITCODE -ne 0) { Die 'push failed' }
Add-Content -LiteralPath $log -Value 'ok: push'

Add-Content -LiteralPath $log -Value 'step: pr'
$body = Join-Path $here 'pr-body-final.md'
gh pr create --repo github-linguist/linguist --head youthx:add-sere-language `
  --title 'Add Sere programming language' --body-file $body *>> $log
if ($LASTEXITCODE -ne 0) {
  Add-Content -LiteralPath $log -Value 'retry: pr create without --head'
  gh pr create --repo github-linguist/linguist `
    --title 'Add Sere programming language' --body-file $body *>> $log
}
Add-Content -LiteralPath $log -Value "done $(Get-Date -Format s)"
Set-Content -LiteralPath (Join-Path $here 'status.txt') -Value 'ok'
