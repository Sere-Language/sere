<#
.SYNOPSIS
  Stages the github-linguist pull request that adds Sere as a language.

.DESCRIPTION
  GitHub only counts `.sere` files as Sere once `Sere` exists in Linguist, so
  this script prepares and (optionally) opens that upstream pull request.

  Without Ruby/Bundler it stages every file it can and prints the two Linguist
  helper commands that have to run inside the checkout. With Ruby installed it
  runs them itself.

.PARAMETER Validate
  Check the payload only: fragment fields, grammar scope, samples, and that no
  other language claims `.sere`. Touches nothing.

.PARAMETER LinguistDir
  Where to clone github-linguist/linguist (default: %TEMP%\sere-linguist).

.PARAMETER Test
  Run Linguist's own test suite (`bundle exec rake test`) after staging.

.PARAMETER Fork
  `owner` or `owner/linguist` to push the branch to, with -Push.

.PARAMETER Push
  Commit, push to -Fork, and open the pull request with `gh`.

.EXAMPLE
  .\scripts\linguist-pr.ps1 -Validate
.EXAMPLE
  .\scripts\linguist-pr.ps1 -Test
.EXAMPLE
  .\scripts\linguist-pr.ps1 -Fork jackw -Push
#>
[CmdletBinding()]
param(
  [string]$LinguistDir = (Join-Path ([System.IO.Path]::GetTempPath()) 'sere-linguist'),
  [string]$GrammarRepo = 'https://github.com/Sere-Language/sere',
  [string]$Fork,
  [switch]$Validate,
  [switch]$Test,
  [switch]$Push,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Payload = Join-Path $RepoRoot '.github\linguist'
$Fragment = Join-Path $Payload 'languages.yml.fragment'
$Samples = Join-Path $Payload 'samples\Sere'
$PrBody = Join-Path $Payload 'pr-body.md'
$Language = 'Sere'
$Extension = '.sere'
$GrammarScope = 'source.sere'
$UpstreamUrl = 'https://github.com/github-linguist/linguist.git'

function Write-Step([string]$Message) { Write-Host "==> $Message" -ForegroundColor Cyan }
function Write-Ok([string]$Message) { Write-Host "    ok   $Message" -ForegroundColor Green }
function Write-Note([string]$Message) { Write-Host "    note $Message" -ForegroundColor Yellow }
function Fail([string]$Message) { Write-Host "error: $Message" -ForegroundColor Red; exit 1 }

function Get-FragmentFields {
  $fields = @{}
  foreach ($line in Get-Content -LiteralPath $Fragment) {
    if ($line -match '^\s*#' -or $line -match '^\s*$') { continue }
    if ($line -match '^([A-Za-z0-9_.\-]+):\s*(.*)$' -and $Matches[1] -ne $Language) {
      $fields[$Matches[1]] = $Matches[2].Trim('"')
    }
  }
  return $fields
}

function Get-LinguistLanguagesYml([string]$Dir) {
  $local = Join-Path $Dir 'lib\linguist\languages.yml'
  if (Test-Path -LiteralPath $local) { return $local }
  $cache = Join-Path ([System.IO.Path]::GetTempPath()) 'sere-linguist-languages.yml'
  if (-not (Test-Path -LiteralPath $cache) -or $Force) {
    Write-Step "Downloading Linguist's languages.yml"
    Invoke-WebRequest -UseBasicParsing -Uri `
      'https://raw.githubusercontent.com/github-linguist/linguist/main/lib/linguist/languages.yml' `
      -OutFile $cache
  }
  return $cache
}

function Test-Payload([string]$LanguagesYml) {
  Write-Step 'Validating the Linguist payload'
  $problems = @()

  $fields = Get-FragmentFields
  foreach ($required in @('type', 'color', 'extensions', 'tm_scope')) {
    if (-not $fields.ContainsKey($required)) { $problems += "languages.yml.fragment is missing '$required'" }
  }
  if ($fields.ContainsKey('language_id')) {
    $problems += 'languages.yml.fragment must not set language_id; Linguist assigns it with script/update-ids'
  }
  if ($fields.ContainsKey('tm_scope') -and $fields['tm_scope'] -ne $GrammarScope) {
    $problems += "tm_scope '$($fields['tm_scope'])' does not match the grammar scope '$GrammarScope'"
  }
  if ($fields.ContainsKey('color') -and $fields['color'] -notmatch '^#[0-9a-fA-F]{6}$') {
    $problems += "color '$($fields['color'])' is not #RRGGBB"
  }
  if (-not (Test-Path -LiteralPath $Fragment)) { $problems += "missing $Fragment" }

  $grammar = Join-Path $RepoRoot 'editors\vscode\syntaxes\sere.tmLanguage.json'
  if (-not (Test-Path -LiteralPath $grammar)) {
    $problems += "missing grammar $grammar"
  } else {
    $text = Get-Content -LiteralPath $grammar -Raw
    if ($text -notmatch '"scopeName"\s*:\s*"' + [regex]::Escape($GrammarScope) + '"') {
      $problems += "grammar scopeName is not '$GrammarScope'"
    }
    # Linguist compiles grammars with PCRE; Oniguruma-only constructs fail there.
    if ($text -match '\\G') { $problems += 'grammar uses \G, which PCRE does not support without a flag' }
    if ($text -match '\(\?<[=!][^)]*[^)]*[^)]*\)') {
      Write-Note 'grammar has lookbehind patterns; Linguist needs fixed-width lookbehind'
    }
  }

  $files = @()
  if (Test-Path -LiteralPath $Samples) {
    $files = @(Get-ChildItem -LiteralPath $Samples -Filter "*$Extension" -File)
  }
  if ($files.Count -lt 2) { $problems += "at least two samples are required in $Samples" }
  foreach ($file in $files) {
    if ($file.Length -lt 500) { $problems += "sample $($file.Name) is too small to be representative" }
  }

  if (Test-Path -LiteralPath $LanguagesYml) {
    if (Select-String -LiteralPath $LanguagesYml -Pattern "^$Language`:" -Quiet) {
      Write-Note "$Language is already in Linguist - the payload is only needed for this repo's history"
    }
    $claimed = Select-String -LiteralPath $LanguagesYml -Pattern "^\s*-\s*`"$([regex]::Escape($Extension))`"\s*$"
    if ($claimed) {
      $problems += "$Extension is already claimed in languages.yml (line $($claimed[0].LineNumber)); add a heuristic"
    }
  } else {
    Write-Note 'no languages.yml available to check extension ownership'
  }

  foreach ($check in @(
      @{ Name = 'repo .gitattributes'; Path = Join-Path $RepoRoot '.gitattributes' }) ) {
    $text = Get-Content -LiteralPath $check.Path -Raw
    if ($text -notmatch [regex]::Escape("linguist-language=$Language")) {
      $problems += "repo .gitattributes is missing linguist-language=$Language"
    }
  }

  if ($problems.Count -gt 0) {
    foreach ($problem in $problems) { Write-Host "    bad  $problem" -ForegroundColor Red }
    Fail 'payload validation failed'
  }
  Write-Ok "language entry: $Language / $Extension / $GrammarScope"
  Write-Ok "samples: $($files.Count)"
  Write-Ok "$Extension is not claimed by another language"
  Write-Ok 'repo .gitattributes declares Sere'
}

function Ensure-LinguistCheckout {
  if (Test-Path -LiteralPath (Join-Path $LinguistDir '.git')) {
    Write-Step "Updating $LinguistDir"
    git -C $LinguistDir fetch --quiet origin
    git -C $LinguistDir checkout --quiet main
    git -C $LinguistDir reset --hard --quiet origin/main
  } else {
    Write-Step "Cloning Linguist into $LinguistDir"
    if (Test-Path -LiteralPath $LinguistDir) { Remove-Item -LiteralPath $LinguistDir -Recurse -Force }
    git clone --quiet --depth 50 $UpstreamUrl $LinguistDir
    if ($LASTEXITCODE -ne 0) { Fail "git clone failed; clone $UpstreamUrl manually and pass -LinguistDir" }
  }
}

function Add-LanguageEntry([string]$LanguagesYml) {
  $lines = [System.Collections.Generic.List[string]]::new()
  $lines.AddRange([string[]](Get-Content -LiteralPath $LanguagesYml))
  if ($lines -contains "$Language`:") {
    Write-Ok "$Language already present in languages.yml"
    return
  }
  $fragmentLines = @(Get-Content -LiteralPath $Fragment |
      Where-Object { $_ -notmatch '^#' } |
      Where-Object { $_.Trim().Length -gt 0 })
  $insertAt = $lines.Count
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^([A-Za-z0-9_.+\-]+):\s*$') {
      if ([System.String]::CompareOrdinal($Matches[1], $Language) -gt 0) { $insertAt = $i; break }
    }
  }
  $block = [System.Collections.Generic.List[string]]::new()
  $block.AddRange([string[]]$fragmentLines)
  $block.Add('')
  $lines.InsertRange($insertAt, $block)
  Set-Content -LiteralPath $LanguagesYml -Value $lines -Encoding UTF8
  Write-Ok "inserted $Language at line $($insertAt + 1)"
}

function Copy-Samples {
  $target = Join-Path $LinguistDir 'samples\Sere'
  if (-not (Test-Path -LiteralPath $target)) { New-Item -ItemType Directory -Path $target | Out-Null }
  foreach ($file in Get-ChildItem -LiteralPath $Samples -Filter "*$Extension" -File) {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $target $file.Name) -Force
  }
  Write-Ok "copied $(@(Get-ChildItem -LiteralPath $target -File).Count) samples to samples\$Language"
}

function Test-Ruby {
  return [bool](Get-Command ruby -ErrorAction SilentlyContinue) -and
         [bool](Get-Command bundle -ErrorAction SilentlyContinue)
}

function Invoke-LinguistTooling {
  if (-not (Test-Ruby)) {
    Write-Note 'Ruby + Bundler are not installed, so Linguist cannot vendor the grammar or assign an id here.'
    Write-Host ''
    Write-Host '  Run these inside the checkout to finish the branch:' -ForegroundColor Yellow
    Write-Host "    cd $LinguistDir"
    Write-Host '    bundle install'
    Write-Host "    script/add-grammar $GrammarRepo"
    Write-Host '    script/update-ids'
    Write-Host '    bundle exec rake test'
    Write-Host ''
    return $false
  }
  Push-Location $LinguistDir
  try {
    Write-Step 'script/add-grammar'
    & script/add-grammar $GrammarRepo
    if ($LASTEXITCODE -ne 0) { Fail 'script/add-grammar failed; fix the grammar and re-run' }
    Write-Ok 'grammar vendored and recorded in grammars.yml'
    Write-Step 'script/update-ids'
    & script/update-ids
    if ($LASTEXITCODE -ne 0) { Fail 'script/update-ids failed' }
    Write-Ok 'language_id assigned'
    if ($Test) {
      Write-Step 'bundle exec rake test'
      & bundle exec rake test
      if ($LASTEXITCODE -ne 0) { Fail 'Linguist tests failed' }
      Write-Ok 'Linguist tests passed'
    }
  } finally {
    Pop-Location
  }
  return $true
}

function Publish-PullRequest {
  if ([string]::IsNullOrWhiteSpace($Fork)) { Fail '-Push needs -Fork <owner> (or owner/linguist)' }
  $owner = $Fork.Split('/')[0]
  $branch = 'add-sere-language'
  Push-Location $LinguistDir
  try {
    git checkout --quiet -B $branch
    git add lib/linguist/languages.yml grammars.yml vendor/grammars samples
    git -c user.name='Sere' -c user.email='noreply@sere-lang.com' commit --quiet -m 'Add Sere language'
    Write-Ok "committed on $branch"
    git push --quiet --force "$owner" "${branch}:${branch}"
    if ($LASTEXITCODE -ne 0) { Fail "push to $owner failed; is the fork reachable?" }
    Write-Ok "pushed to $owner/$branch"
    if (Get-Command gh -ErrorAction SilentlyContinue) {
      gh pr create --repo github-linguist/linguist --head "${owner}:${branch}" `
        --title 'Add Sere language' --body-file $PrBody
      Write-Ok 'pull request opened'
    } else {
      Write-Note 'gh is not installed; open the pull request with the body in .github/linguist/pr-body.md'
      Write-Host "    https://github.com/github-linguist/linguist/compare/main...${owner}:${branch}?expand=1"
    }
  } finally {
    Pop-Location
  }
}

# ---------------------------------------------------------------------------

if (-not (Test-Path -LiteralPath $Fragment)) { Fail "missing $Fragment" }

if ($Validate) {
  Test-Payload (Get-LinguistLanguagesYml '')
  Write-Host ''
  Write-Host 'Payload is ready for github-linguist/linguist.' -ForegroundColor Green
  exit 0
}

Test-Payload (Get-LinguistLanguagesYml $LinguistDir)
Ensure-LinguistCheckout
Add-LanguageEntry (Join-Path $LinguistDir 'lib\linguist\languages.yml')
Copy-Samples
$finished = Invoke-LinguistTooling

if ($Push) {
  if (-not $finished) { Fail '-Push needs Ruby installed so the grammar and language id are in the branch' }
  Publish-PullRequest
  exit 0
}

Write-Host ''
if ($finished) {
  Write-Host "Branch ready in $LinguistDir." -ForegroundColor Green
  Write-Host 'Review `git -C '"$LinguistDir"' status`, then re-run with -Fork <owner> -Push.' -ForegroundColor Green
} else {
  Write-Host "Staged in $LinguistDir; finish the four commands above, then re-run with -Fork <owner> -Push." -ForegroundColor Yellow
}
Write-Host 'Pull request body: .github/linguist/pr-body.md' -ForegroundColor Green
