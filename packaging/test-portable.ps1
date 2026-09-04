[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference = 'Stop'
$Package = [IO.Path]::GetFullPath($Package)
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('sere-portable-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $testRoot | Out-Null
$names = @('PATH','LIB','INCLUDE','SERE_LLVM_DIR','SERE_STDLIB','SERE_HOME','SERE_ACTIVE','SERE_PROJECT_ROOT','VCToolsInstallDir','VCINSTALLDIR','WindowsSdkDir','WindowsSDKVersion')
$saved = @{}
foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process'); [Environment]::SetEnvironmentVariable($name,$null,'Process') }
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
try {
  Push-Location $testRoot
  $sere = Join-Path $Package 'bin\sere.exe'
  $info = & $sere --print-env | ConvertFrom-Json
  if ($LASTEXITCODE -or -not $info.clang.StartsWith($Package, [StringComparison]::OrdinalIgnoreCase)) { throw 'Compiler selected external clang' }
  & $sere init smoke
  if ($LASTEXITCODE) { throw 'Portable init failed' }
  if (Test-Path 'smoke\venv\stdlib\stdlib') { throw 'Nested stdlib created' }
  Set-Location smoke
  $info = & '.\venv\bin\sere.exe' --print-env | ConvertFrom-Json
  if ($LASTEXITCODE -or -not $info.clang.StartsWith($Package, [StringComparison]::OrdinalIgnoreCase)) { throw 'Virtual environment selected external clang' }
  & '.\venv\bin\sere.exe' build
  if ($LASTEXITCODE) { throw 'Portable virtual environment build failed' }
  & '.\bin\smoke.exe'
  if ($LASTEXITCODE) { throw 'Portable executable failed' }
  Write-Host 'Portable smoke test passed with development environment cleared.'
} finally {
  Pop-Location
  foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
  $resolved = [IO.Path]::GetFullPath($testRoot)
  if ($resolved.StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase) -and (Split-Path $resolved -Leaf) -like 'sere-portable-*') { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
