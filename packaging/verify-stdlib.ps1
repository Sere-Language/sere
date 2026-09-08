[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$Source,
  [Parameter(Mandatory=$true)][string]$Package
)
$ErrorActionPreference = 'Stop'
$Source = [IO.Path]::GetFullPath($Source).TrimEnd('\', '/')
$staged = Join-Path ([IO.Path]::GetFullPath($Package)) 'stdlib'
if (Test-Path -LiteralPath (Join-Path $Package 'bin\stdlib')) { throw 'Legacy bin/stdlib would shadow the packaged standard library.' }
foreach ($root in @($Source, $staged)) {
  if (-not (Test-Path -LiteralPath (Join-Path $root 'prelude.sere') -PathType Leaf)) { throw "Standard library missing: $root" }
}
$expected = @{}
foreach ($file in Get-ChildItem -LiteralPath $Source -File -Recurse -Force) {
  $relative = $file.FullName.Substring($Source.Length + 1)
  $expected[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
}
$actual = @(Get-ChildItem -LiteralPath $staged -File -Recurse -Force)
if ($actual.Count -ne $expected.Count) { throw 'Packaged stdlib file count differs from repository.' }
foreach ($file in $actual) {
  $relative = $file.FullName.Substring($staged.Length + 1)
  if (-not $expected.ContainsKey($relative) -or $expected[$relative] -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {
    throw "Packaged stdlib differs from repository: $relative"
  }
}
Write-Host "Verified $($expected.Count) stdlib files against repository sources."
