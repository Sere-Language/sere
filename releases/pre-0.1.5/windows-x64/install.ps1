[CmdletBinding()]
param([string]$Prefix = (Join-Path $env:LOCALAPPDATA 'Programs\Sere'), [switch]$Editor, [switch]$NoPath)
$ErrorActionPreference = 'Stop'
$Prefix = [IO.Path]::GetFullPath($Prefix)
if (-not (Test-Path "$PSScriptRoot\toolchains\llvm-22.1.8\sysroot\lib\libcmt.lib")) { throw 'Incomplete Sere package. Extract the complete portable archive first.' }
if ($Prefix.StartsWith($PSScriptRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
  throw 'Install prefix must not be inside the source package.'
}
if ($Prefix.TrimEnd('\') -ne $PSScriptRoot.TrimEnd('\')) {
  New-Item -ItemType Directory -Force -Path $Prefix | Out-Null
  Get-ChildItem -LiteralPath $PSScriptRoot -Force | Copy-Item -Destination $Prefix -Recurse -Force
}
$bin = Join-Path $Prefix 'bin'
if (-not $NoPath) {
  $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
  $entries = @($userPath -split ';' | Where-Object { $_ })
  if (@($entries | ForEach-Object { $_.TrimEnd('\') }) -notcontains $bin.TrimEnd('\')) {
    [Environment]::SetEnvironmentVariable('Path', (($entries + $bin) -join ';'), 'User')
  }
  $env:Path = "$bin;$env:Path"
}
if ($Editor) { & "$Prefix\install-vsix.ps1" -Vsix "$Prefix\editors\sere.vsix" -Compiler "$bin\sere.exe" }
Write-Host "Installed Sere to $Prefix. Open a new terminal to use sere."
