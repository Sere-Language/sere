# Verifies the opened Linguist PR and removes the local clone.
$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$log = Join-Path $here 'pr_check.log'
Set-Content -LiteralPath $log -Value '' -Encoding ascii
function Log([string]$m) { ($m -split "`r?`n") | Out-File -FilePath $log -Encoding ascii -Append }

Log '== pr'
Log ((gh pr view 8209 --repo github-linguist/linguist --json url,title,state,changedFiles,additions,deletions 2>&1 | Out-String).Trim())
Log '== files'
Log ((gh pr view 8209 --repo github-linguist/linguist --json files --jq '.files[] | .path + "  +" + (.additions | tostring)' 2>&1 | Out-String).Trim())
Log '== languages.yml diff'
Log ((gh pr diff 8209 --repo github-linguist/linguist 2>&1 | Select-String -Pattern 'Sere' -Context 4,4 | Out-String).Trim())

Log '== cleanup'
Remove-Item (Join-Path $here 'linguist-pr') -Recurse -Force -ErrorAction SilentlyContinue
Log "clone removed: $(-not (Test-Path (Join-Path $here 'linguist-pr')))"
Log 'done'
