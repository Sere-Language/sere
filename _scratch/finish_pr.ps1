# Creates the fork (no --remote flag: unsupported with a repository argument),
# pushes the Sere branch, and opens the pull request. ASCII log for readability.
$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$cloneDir = Join-Path $here 'linguist-pr\linguist'
$log = Join-Path $here 'finish.log'
$body = Join-Path $here 'pr-body-final.md'

Set-Content -LiteralPath $log -Value '' -Encoding ascii
function Log([string]$m) {
  ($m -split "`r?`n") | Where-Object { $_.Trim().Length -gt 0 } | Out-File -FilePath $log -Encoding ascii -Append
}
function Run([string]$label, [scriptblock]$action) {
  Log "== $label"
  $output = & $action 2>&1 | Out-String
  Log $output.Trim()
  Log "   exit=$LASTEXITCODE"
}

Log "clone present: $(Test-Path (Join-Path $cloneDir '.git'))"
Run 'fork-create' { gh repo fork github-linguist/linguist --clone=false --default-branch-only }
Run 'fork-view' { gh repo view youthx/linguist --json nameWithOwner,isFork,defaultBranchRef --jq '.nameWithOwner + " isFork=" + (.isFork | tostring) + " default=" + .defaultBranchRef.name' }
Run 'push' { git -C $cloneDir push --force https://github.com/youthx/linguist.git add-sere-language:add-sere-language }
Run 'pr' { gh pr create --repo github-linguist/linguist --head youthx:add-sere-language --title 'Add Sere programming language' --body-file $body }
Log 'done'
