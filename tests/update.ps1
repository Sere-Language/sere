$ErrorActionPreference = 'Stop'
$tokens = $null
$errors = $null
$script = Join-Path $PSScriptRoot '..\scripts\update.ps1'
$ast = [Management.Automation.Language.Parser]::ParseFile($script, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
foreach ($name in @('Get-ReleaseVersion', 'Test-AssetMatch')) {
  $function = $ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name}, $true)
  Invoke-Expression $function.Extent.Text
}
if ((Get-ReleaseVersion 'pre-0.1.10') -le (Get-ReleaseVersion 'pre-0.1.9')) { throw 'Versions must compare numerically.' }
if ($null -ne (Get-ReleaseVersion 'unrecognized')) { throw 'Invalid version accepted.' }
$release = [pscustomobject]@{id=10}
$asset = [pscustomobject]@{id=20; updated_at='2026-09-08T00:00:00Z'; digest='sha256:abc'}
$state = [pscustomobject]@{releaseId=10; assetId=20; updatedAt=$asset.updated_at; digest=$asset.digest}
if (-not (Test-AssetMatch $state $release $asset)) { throw 'Unchanged asset should match.' }
foreach ($property in @('id', 'updated_at', 'digest')) {
  $changed = $asset.PSObject.Copy()
  $changed.$property = 'changed'
  if (Test-AssetMatch $state $release $changed) { throw "Same-release change missed: $property" }
}
if (Test-AssetMatch ([pscustomobject]@{}) $release $asset) { throw 'Missing installation state must trigger an update.' }
Write-Host 'Updater version and same-release asset tests passed.'
