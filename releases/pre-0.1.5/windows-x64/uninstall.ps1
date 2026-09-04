[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$prefix = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not (Test-Path "$prefix\bin\sere.exe") -or -not (Test-Path "$prefix\toolchains\llvm-22.1.8")) { throw 'Run uninstall from a Sere installation.' }
$bin = Join-Path $prefix 'bin'
$entries = @([Environment]::GetEnvironmentVariable('Path','User') -split ';' | Where-Object { $_ -and $_.TrimEnd('\') -ne $bin.TrimEnd('\') })
[Environment]::SetEnvironmentVariable('Path', ($entries -join ';'), 'User')
# Delete only known package entries; leave unrelated user files intact.
foreach ($name in @('licenses','bin','toolchains','stdlib','include','packaging','editors','LICENSE','README.md','install.ps1','install.cmd','install-vsix.ps1','uninstall.cmd','uninstall.ps1')) {
  $target = [IO.Path]::GetFullPath((Join-Path $prefix $name))
  if (-not $target.StartsWith($prefix.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe uninstall target' }
  if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
}
Write-Host 'Sere removed. Open a new terminal to refresh PATH.'
