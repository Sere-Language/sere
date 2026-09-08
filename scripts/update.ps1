[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$CurrentVersion,
  [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Get-ReleaseVersion([string]$Tag) {
  if ($Tag -match '^(?:pre-|v)?(\d+\.\d+\.\d+)(?:-stable)?$') { return [version]$Matches[1] }
  return $null
}

function Test-AssetMatch($State, $Release, $Asset) {
  foreach ($key in @('releaseId', 'assetId', 'updatedAt', 'digest')) {
    if (-not $State.PSObject.Properties[$key]) { return $false }
  }
  return ($State.releaseId -eq $Release.id -and $State.assetId -eq $Asset.id -and
    $State.updatedAt -eq $Asset.updated_at -and $State.digest -eq $Asset.digest)
}

try {
  if (-not [Environment]::Is64BitOperatingSystem) { throw 'Sere releases require Windows x64.' }
  $prefix = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Programs\Sere'))
  $headers = @{ 'User-Agent' = 'Sere-Updater'; Accept = 'application/vnd.github+json' }
  Write-Host 'Checking GitHub releases (including refreshed portable assets)...'
  $candidates = @()
  # Include prereleases: Sere currently publishes pre-x.y.z tags.
  for ($page = 1; ; $page++) {
    $releases = Invoke-RestMethod "https://api.github.com/repos/Sere-Language/sere/releases?per_page=100&page=$page" -Headers $headers -TimeoutSec 60
    foreach ($release in $releases) {
      $version = Get-ReleaseVersion $release.tag_name
      if ($release.draft -or $null -eq $version) { continue }
      $assets = @($release.assets | Where-Object { $_.name -cmatch '^Sere-.+-windows-x64-portable\.zip$' })
      if ($assets.Count -eq 1) {
        $candidates += [pscustomobject]@{ Version=$version; Release=$release; Asset=$assets[0] }
      }
    }
    if ($releases.Count -lt 100) { break }
  }
  $latest = $candidates | Sort-Object Version, @{Expression={$_.Release.published_at}} -Descending | Select-Object -First 1
  if ($null -eq $latest) { throw 'No supported Windows x64 portable release found.' }
  $current = Get-ReleaseVersion $CurrentVersion
  if ($null -eq $current) { throw "Cannot compare compiler version: $CurrentVersion" }
  if ($latest.Version -lt $current) { Write-Host 'This compiler is newer than the published release.'; exit 0 }
  $release = $latest.Release
  $asset = $latest.Asset
  if (-not $asset.PSObject.Properties['digest'] -or $asset.digest -notmatch '^sha256:[a-fA-F0-9]{64}$') {
    throw 'Release asset has no GitHub SHA256 digest; refusing an unverified update.'
  }
  $statePath = Join-Path $prefix 'release-state.json'
  if ((Test-Path -LiteralPath $statePath) -and (Test-Path -LiteralPath "$prefix\bin\sere.exe")) {
    $state = $null
    try { $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json } catch { }
    if ($null -ne $state -and (Test-AssetMatch $state $release $asset)) {
      Write-Host "Already installed: $($release.tag_name), portable asset $($asset.id)."
      exit 0
    }
  }
  Write-Host "Update available: $($release.tag_name), portable asset $($asset.id) ($($asset.updated_at))."
  if ($CheckOnly) { exit 0 }
  $url = [uri]$asset.browser_download_url
  if ($url.Scheme -ne 'https' -or $url.Host -ne 'github.com' -or
      $url.AbsolutePath -notlike '/Sere-Language/sere/releases/download/*') { throw 'Unexpected download URL.' }
  $work = Join-Path ([IO.Path]::GetTempPath()) ('sere-update-' + [guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Path $work | Out-Null
  $archive = Join-Path $work 'portable.zip'
  Write-Host "Downloading $($asset.name)..."
  Invoke-WebRequest $url.AbsoluteUri -Headers $headers -OutFile $archive -UseBasicParsing -TimeoutSec 1800
  $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
  if ("sha256:$hash" -ne $asset.digest) { throw 'Portable archive checksum mismatch.' }
  $payload = Join-Path $work 'payload'
  Expand-Archive -LiteralPath $archive -DestinationPath $payload
  foreach ($file in @('install.ps1','bin\sere.exe','stdlib\prelude.sere','toolchains\llvm-22.1.8\sysroot\lib\libcmt.lib')) {
    if (-not (Test-Path -LiteralPath (Join-Path $payload $file) -PathType Leaf)) { throw "Incomplete portable release: $file" }
  }
  $reported = & "$payload\bin\sere.exe" --version
  if ($LASTEXITCODE -or "$reported" -notmatch [regex]::Escape($release.tag_name)) { throw 'Downloaded compiler version does not match release.' }
  # Keep this updater available even when installing a release predating it.
  Copy-Item -LiteralPath $PSCommandPath -Destination "$payload\bin\update.ps1" -Force
  $backup = "$prefix.backup-$([guid]::NewGuid().ToString('N'))"
  $hadInstall = Test-Path -LiteralPath $prefix
  if ($hadInstall) { Move-Item -LiteralPath $prefix -Destination $backup }
  try {
    & "$payload\install.ps1" -Prefix $prefix
    if (-not $?) { throw 'Portable installer failed.' }
    @{releaseId=$release.id; assetId=$asset.id; updatedAt=$asset.updated_at; digest=$asset.digest} |
      ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
  } catch {
    # Move partial output aside; restore the previous install without deleting user files.
    if (Test-Path -LiteralPath $prefix) { Move-Item -LiteralPath $prefix -Destination "$prefix.failed-$([guid]::NewGuid().ToString('N'))" }
    if ($hadInstall) { Move-Item -LiteralPath $backup -Destination $prefix }
    throw
  }
  Write-Host "Installed $($release.tag_name) to $prefix. Open a new terminal to use it."
  if ($hadInstall) { Write-Host "Previous installation retained at $backup" }
  Write-Host "Download retained at $work"
} catch {
  Write-Error "Sere update failed: $_"
  exit 1
}
