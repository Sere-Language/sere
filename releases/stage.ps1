[CmdletBinding()]
param(
  [string]$Name,
  [string]$BuildDir,
  [string]$LlvmRoot = $env:SERE_LLVM_DIR,
  [string]$Iscc,
  [string]$InstallerOutput,
  [switch]$SkipBuild,
  [switch]$WithoutEditor,
  [switch]$PortableOnly
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$versionMatch = [regex]::Match((Get-Content "$repo\CMakeLists.txt" -Raw), 'VERSION\s+(\d+\.\d+\.\d+)')
if (-not $Name) { $Name = 'pre-' + $versionMatch.Groups[1].Value }
if ($Name -notmatch '^pre-\d+\.\d+\.\d+$') { throw 'Name must be pre-x.x.x' }
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build\windows-clang-cl-release' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
if (-not $LlvmRoot) { $LlvmRoot = Join-Path $env:LOCALAPPDATA 'sere\toolchains\llvm-22.1.8' }
$LlvmRoot = [IO.Path]::GetFullPath($LlvmRoot)
if (-not $PortableOnly -and -not $Iscc) {
  $Iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source) |
    Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
  if (-not $Iscc) { throw 'Release builder requires Inno Setup 6 (ISCC.exe). Pass -Iscc or use scripts/bootstrap-innosetup.ps1.' }
}
if (-not $SkipBuild) {
  $env:SERE_LLVM_DIR = $LlvmRoot
  . "$repo\scripts\env.ps1"
  & cmake -S $repo -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl "-DLLVM_DIR=$LlvmRoot/lib/cmake/llvm"
  if ($LASTEXITCODE) { throw 'CMake configure failed' }
  & cmake --build $BuildDir --config Release
  if ($LASTEXITCODE) { throw 'Build failed' }
  & ctest --test-dir $BuildDir -C Release --output-on-failure
  if ($LASTEXITCODE) { throw 'Tests failed' }
}
Write-Host "Staging $Name from $BuildDir"
$compilerDir = Join-Path $BuildDir 'bin'
$exe = Join-Path $compilerDir 'sere.exe'
if (-not (Test-Path $exe)) { throw "Compiler missing: $exe" }
$compilerVersion = & $exe --version
if ($LASTEXITCODE -or $compilerVersion -notmatch [regex]::Escape($Name)) { throw "Compiler version does not match $Name : $compilerVersion" }
$releaseRoot = [IO.Path]::GetFullPath((Join-Path $repo "releases\$Name"))
$dest = Join-Path $releaseRoot 'windows-x64'
# Validate the exact removal target before replacing generated output.
if (-not $dest.StartsWith(([IO.Path]::GetFullPath("$repo\releases") + '\'), [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe release destination' }
if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$dest\bin", "$dest\packaging", "$dest\editors" | Out-Null
function Copy-Required([string]$From, [string]$To) {
  if (-not (Test-Path -LiteralPath $From)) { throw "Required release file missing: $From" }
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $To) | Out-Null
  Copy-Item -LiteralPath $From -Destination $To -Force
}
function Copy-Contents([string]$From, [string]$To) {
  if (-not (Test-Path -LiteralPath $From)) { throw "Required release directory missing: $From" }
  New-Item -ItemType Directory -Force -Path $To | Out-Null
  Get-ChildItem -LiteralPath $From -Force | Copy-Item -Destination $To -Recurse -Force
}
foreach ($file in @('sere.exe', 'sere_rt.lib', 'sere_qt6.lib', 'sere_icon.res')) { Copy-Required "$compilerDir\$file" "$dest\bin\$file" }
Get-ChildItem $compilerDir -Filter '*.dll' | Copy-Item -Destination "$dest\bin"
if (Test-Path "$compilerDir\platforms") { Copy-Contents "$compilerDir\platforms" "$dest\bin\platforms" }
Copy-Contents "$repo\stdlib" "$dest\stdlib"
Copy-Contents "$repo\include\sere\api" "$dest\include\sere\api"
Copy-Required "$repo\LICENSE" "$dest\LICENSE"
Copy-Contents "$repo\packaging\licenses" "$dest\licenses"
Copy-Required "$repo\icon.ico" "$dest\bin\icon.ico"
foreach ($file in @('install.ps1','install.cmd','uninstall.ps1','uninstall.cmd','install-vsix.ps1')) { Copy-Required "$repo\packaging\$file" "$dest\$file" }
Copy-Required "$repo\packaging\install-vsix.ps1" "$dest\packaging\install-vsix.ps1"
Write-Host "Bundling compiler tools and x64 link support"
$llvm = Join-Path $dest 'toolchains\llvm-22.1.8'
foreach ($tool in @('clang.exe','lld-link.exe','llvm-ar.exe')) { Copy-Required "$LlvmRoot\bin\$tool" "$llvm\bin\$tool" }
# Follow actual PE imports; do not ship unused LLVM-C/libclang/LTO DLLs.
$queue = [Collections.Generic.Queue[string]]::new()
foreach ($tool in @('clang.exe','lld-link.exe','llvm-ar.exe')) { $queue.Enqueue($tool) }
$seen = @{}
while ($queue.Count) {
  $tool = $queue.Dequeue()
  if ($seen.ContainsKey($tool)) { continue }
  $seen[$tool] = $true
  $imports = & "$LlvmRoot\bin\llvm-readobj.exe" --coff-imports "$LlvmRoot\bin\$tool"
  if ($LASTEXITCODE) { throw "Cannot inspect dependencies of $tool" }
  foreach ($line in $imports) {
    if ($line -match '^\s*Name:\s*(.+\.dll)\s*$') {
      $dll = $Matches[1].Trim()
      if (Test-Path "$LlvmRoot\bin\$dll") {
        Copy-Required "$LlvmRoot\bin\$dll" "$llvm\bin\$dll"
        $queue.Enqueue($dll)
      } elseif ($dll -notmatch '^(api-ms-|ext-ms-)' -and -not (Test-Path "$env:SystemRoot\System32\$dll")) {
        throw "Missing runtime dependency: $dll ($tool)"
      }
    }
  }
}
Get-ChildItem "$LlvmRoot\lib\clang" -Directory | ForEach-Object {
  Copy-Contents "$($_.FullName)\include" "$llvm\lib\clang\$($_.Name)\include"
  $resourceVersion = $_.Name
  Get-ChildItem "$($_.FullName)\lib\windows" -Filter 'clang_rt.builtins-x86_64.lib' -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Required $_.FullName "$llvm\lib\clang\$resourceVersion\lib\windows\$($_.Name)"
  }
}
Get-ChildItem $LlvmRoot -Filter '*LICENSE*' | ForEach-Object { Copy-Required $_.FullName "$llvm\$($_.Name)" }
# Only the x64 headers/libraries needed by clang are staged, not VS or the full LLVM SDK.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -or -not $vs) { throw 'Release builder needs the MSVC x64 toolset' }
$vc = Get-ChildItem "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$sdk = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Lib" -Directory | Where-Object { Test-Path "$($_.FullName)\um\x64\kernel32.lib" } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $sdk -or -not $vc) { throw 'Release builder needs Windows SDK and MSVC libraries' }
foreach ($libDir in @("$($vc.FullName)\lib\x64", "$($sdk.FullName)\ucrt\x64", "$($sdk.FullName)\um\x64")) {
  New-Item -ItemType Directory -Force "$llvm\sysroot\lib" | Out-Null
  Get-ChildItem $libDir -Filter '*.lib' | Where-Object { $_.Name -notmatch '^(clang_rt\.|(?:libcmt|libcpmt|libucrt|libvcruntime|libconcrt|msvcrt|msvcprt|ucrt|vcruntime|concrt)d[0-9]*\.)' } | Copy-Item -Destination "$llvm\sysroot\lib"
}
Copy-Contents "$($vc.FullName)\include" "$llvm\sysroot\include\msvc"
$sdkInclude = "${env:ProgramFiles(x86)}\Windows Kits\10\Include\$($sdk.Name)"
foreach ($dir in @('ucrt','shared','um')) { Copy-Contents "$sdkInclude\$dir" "$llvm\sysroot\include\$dir" }
# App-local CRT for dynamically linked optional modules and compiler dependencies.
$redist = Get-ChildItem "$vs\VC\Redist\MSVC" -Directory | Where-Object { Test-Path "$($_.FullName)\x64" } | Sort-Object Name -Descending | Select-Object -First 1
$crt = Get-ChildItem "$($redist.FullName)\x64" -Directory -Filter '*.CRT' | Select-Object -First 1
if (-not $crt) { throw 'MSVC app-local CRT missing' }
Get-ChildItem $crt.FullName -Filter '*.dll' | ForEach-Object {
  Copy-Required $_.FullName "$dest\bin\$($_.Name)"
  Copy-Required $_.FullName "$llvm\bin\$($_.Name)"
}
if (-not $WithoutEditor) {
  & "$repo\scripts\package-vsix.ps1"
  if (-not $?) { throw 'VSIX packaging failed' }
  $extension = Get-Content "$repo\editors\vscode\package.json" -Raw | ConvertFrom-Json
  Copy-Required "$repo\dist\$($extension.name)-$($extension.version).vsix" "$dest\editors\sere.vsix"
}
Copy-Required "$repo\packaging\README.md" "$dest\README.md"
# Compilation must succeed without development environment variables or PATH tools.
& "$repo\packaging\test-portable.ps1" -Package $dest
Write-Host "Compressing portable archive"
$zip = Join-Path $releaseRoot "Sere-$Name-windows-x64-portable.zip"
Compress-Archive -Path "$dest\*" -DestinationPath $zip -Force
if (-not $PortableOnly) {
  if (-not $InstallerOutput) { $InstallerOutput = Join-Path $releaseRoot "Sere-$Name-windows-x64-setup.exe" }
  $InstallerOutput = [IO.Path]::GetFullPath($InstallerOutput)
  New-Item -ItemType Directory -Force (Split-Path $InstallerOutput -Parent) | Out-Null
  $iss = Get-Content "$repo\packaging\sere.iss.in" -Raw
  $tokens = @{
    SERE_VERSION=$Name; PAYLOAD_DIR=$dest; OUTPUT_DIR=(Split-Path $InstallerOutput -Parent)
    OUTPUT_BASE=[IO.Path]::GetFileNameWithoutExtension($InstallerOutput); HAS_QT='0'
    HAS_VSIX=([int](-not $WithoutEditor)).ToString(); LLVM_VERSION='22.1.8'; SETUP_ICON="$repo\icon.ico"
  }
  foreach ($key in $tokens.Keys) { $iss = $iss.Replace("@$key@", $tokens[$key]) }
  $issPath = Join-Path $releaseRoot 'sere.iss'
  Set-Content -LiteralPath $issPath -Value $iss -Encoding UTF8
  & $Iscc $issPath
  if ($LASTEXITCODE) { throw 'Installer compilation failed' }
}
Get-ChildItem $releaseRoot -File | Where-Object { $_.Extension -in '.exe','.zip' } | ForEach-Object {
  '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name
} | Set-Content "$releaseRoot\SHA256SUMS.txt" -Encoding ascii
Write-Host "Release ready: $releaseRoot"
