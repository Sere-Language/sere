[CmdletBinding()]
param(
  [string]$Name,
  [string]$BuildDir,
  [string]$LlvmRoot,
  [string]$Vsix,
  [ValidateSet('Auto', 'Local', 'Wsl', 'Docker')][string]$Engine = 'Auto',
  [string]$Distro,
  [string]$Image = 'ubuntu:22.04',
  [switch]$SkipBuild,
  [switch]$Test,
  [switch]$WithPayload,
  [switch]$WithoutEditor,
  [switch]$SkipArchive
)
# Stage a self-contained Linux x64 distro (compiler + runtime + LLVM + stdlib).
# This packages a Linux build that was produced elsewhere (Linux host or CI);
# it never cross-compiles. Run from any host, including Windows, where the
# POSIX permission bits have to be written into the archives explicitly because
# NTFS does not carry them.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$LLVM_VERSION = '22.1.8'

$versionMatch = [regex]::Match((Get-Content "$repo\CMakeLists.txt" -Raw), 'VERSION\s+(\d+\.\d+\.\d+)')
if (-not $versionMatch.Success) { throw 'Cannot determine CMake project version' }
if (-not $Name) { $Name = $versionMatch.Groups[1].Value }
$Name = $Name -replace '^pre-', ''
if ($Name -notmatch '^\d+\.\d+\.\d+$') { throw 'Name must be x.x.x' }

$releaseRoot = [IO.Path]::GetFullPath((Join-Path $repo "releases\$Name"))
$dest = Join-Path $releaseRoot 'linux-x64'
# Validate the exact removal target before replacing generated output.
if (-not $dest.StartsWith(([IO.Path]::GetFullPath("$repo\releases") + '\'), [StringComparison]::OrdinalIgnoreCase)) {
  throw 'Unsafe release destination'
}

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

function Find-LinuxCompiler {
  param([string]$Root)
  # A host that can only ever produce a Windows build has bin\sere.exe and no
  # bin\sere, so requiring the bare name also guards against cross-staging PE.
  $candidates = @()
  if ($Root) {
    $candidates += (Join-Path $Root 'bin\sere')
    $candidates += (Join-Path $Root 'staging\sere')
  }
  $candidates += Join-Path $repo 'build\linux-clang-relwithdebinfo\bin\sere'
  $candidates += Join-Path $repo 'build\linux-clang-relwithdebinfo\bin\staging\sere'
  $candidates += Join-Path $repo 'bin\sere'
  foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate -PathType Leaf) { return [IO.Path]::GetFullPath($candidate) }
  }
  return $null
}

function Find-LinuxRuntime {
  param([string]$CompilerDir, [string]$Root)
  $candidates = @((Join-Path $CompilerDir 'libsere_rt.a'), (Join-Path $CompilerDir 'sere_rt.a'))
  if ($Root) {
    $candidates += (Join-Path $Root 'libsere_rt.a')
    $candidates += (Join-Path $Root 'runtime\libsere_rt.a')
    $candidates += (Join-Path $Root 'bin\libsere_rt.a')
  }
  $candidates += Join-Path $repo 'bin\libsere_rt.a'
  $candidates += Join-Path $repo 'build\linux-clang-relwithdebinfo\runtime\libsere_rt.a'
  foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate -PathType Leaf) { return [IO.Path]::GetFullPath($candidate) }
  }
  return $null
}

# ---------------------------------------------------------------------------
# Archive helpers. Windows archives are created without POSIX modes, so the
# mode bits are written into the archive structures afterwards.
# ---------------------------------------------------------------------------

function Get-PayloadMode {
  param([string]$EntryName, [string]$TypeFlag)
  $name = $EntryName -replace '^\./', ''
  if ($TypeFlag -eq '5' -or $EntryName.EndsWith('/')) { return 0x1ED }        # 0755
  if ($name -match '^(install\.sh|uninstall\.sh)$' -or
      $name -match '^bin/sere(-path\.sh)?$' -or
      $name -match '^packaging/.*\.sh$' -or
      $name -match '^toolchains/[^/]+/bin/') { return 0x1ED }                 # 0755
  return 0x1A4                                                                # 0644
}

function Read-FullBytes {
  param([IO.Stream]$Stream, [byte[]]$Buffer, [int]$Count)
  $total = 0
  while ($total -lt $Count) {
    $read = $Stream.Read($Buffer, $total, $Count - $total)
    if ($read -le 0) { break }
    $total += $read
  }
  return $total
}

function Read-HeadBytes {
  param([string]$Path, [int]$Count)
  $stream = [IO.File]::OpenRead($Path)
  try {
    $buffer = [byte[]]::new($Count)
    $read = Read-FullBytes -Stream $stream -Buffer $buffer -Count $Count
    if ($read -lt $Count) { return $buffer[0..($read - 1)] }
    return $buffer
  } finally {
    $stream.Dispose()
  }
}

# The payload is only usable on Linux, so the inputs are verified to be ELF
# rather than trusted. An MSYS/Cygwin or Windows build produces a PE image that
# would silently ship a compiler which cannot run on Linux.
function Assert-LinuxBinary {
  param([string]$Path, [string]$What, [byte[]]$Head)
  if (-not $Head) { $Head = Read-HeadBytes -Path $Path -Count 4 }
  if ($Head.Length -lt 4 -or
      -not ($Head[0] -eq 0x7F -and $Head[1] -eq 0x45 -and $Head[2] -eq 0x4C -and $Head[3] -eq 0x46)) {
    throw "$What is not an ELF binary: $Path. Build the Linux distro on Linux (or in CI) - MSYS/Cygwin and Windows toolchains produce PE images."
  }
}

function Assert-LinuxArchive {
  param([string]$Path, [string]$What)
  $head = Read-HeadBytes -Path $Path -Count 4096
  if ([Text.Encoding]::ASCII.GetString($head[0..7]) -ne '!<arch>' + "`n") {
    throw "$What is not a Unix archive: $Path"
  }
  # An ELF .a carries ELF members; a COFF/PE .lib is the same container format
  # but its members are not usable by clang on Linux.
  for ($i = 0; $i -le $head.Length - 4; $i++) {
    if ($head[$i] -eq 0x7F -and $head[$i + 1] -eq 0x45 -and $head[$i + 2] -eq 0x4C -and $head[$i + 3] -eq 0x46) { return }
  }
  throw "$What contains no ELF members, so it was not produced by a Linux build: $Path"
}

function Write-TarModeField {
  param([byte[]]$Header, [int]$Mode)
  $octal = [Convert]::ToString($Mode, 8).PadLeft(7, '0') + "`0"
  [Array]::Copy([Text.Encoding]::ASCII.GetBytes($octal), 0, $Header, 100, 8)
  for ($i = 148; $i -lt 156; $i++) { $Header[$i] = 32 }
  $sum = 0
  foreach ($byte in $Header) { $sum += $byte }
  $checksum = [Convert]::ToString($sum, 8).PadLeft(6, '0')
  [Array]::Copy([Text.Encoding]::ASCII.GetBytes($checksum + "`0 "), 0, $Header, 148, 8)
}

function Set-TarUnixModes {
  param([string]$TarPath)
  $stream = [IO.File]::Open($TarPath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
  try {
    $header = [byte[]]::new(512)
    while ($true) {
      if ((Read-FullBytes -Stream $stream -Buffer $header -Count 512) -lt 512) { break }
      $empty = $true
      foreach ($byte in $header) { if ($byte -ne 0) { $empty = $false; break } }
      if ($empty) { break }
      $headerOffset = $stream.Position - 512
      $name = ([Text.Encoding]::ASCII.GetString($header, 0, 100)).Split([char]0)[0]
      $prefix = ([Text.Encoding]::ASCII.GetString($header, 345, 155)).Split([char]0)[0]
      if ($prefix) { $name = "$prefix/$name" }
      $typeFlag = [string][char]$header[156]
      $sizeText = ([Text.Encoding]::ASCII.GetString($header, 124, 12)).Trim([char]0, ' ')
      # GNU/pax long-name records carry the real payload; their size keeps the
      # block walk in sync.
      $size = if ($sizeText) { [Convert]::ToInt64($sizeText, 8) } else { [int64]0 }
      $mode = Get-PayloadMode -EntryName $name -TypeFlag $typeFlag
      if ($mode) {
        Write-TarModeField -Header $header -Mode $mode
        $stream.Position = $headerOffset
        $stream.Write($header, 0, 512)
      }
      $stream.Seek([int64]([math]::Ceiling($size / 512.0) * 512), [IO.SeekOrigin]::Current) | Out-Null
    }
  } finally {
    $stream.Dispose()
  }
}

function Set-ZipUnixModes {
  param([string]$ZipPath)
  $bytes = [IO.File]::ReadAllBytes($ZipPath)
  # Locate the end-of-central-directory record and walk the central directory.
  $eocd = -1
  $limit = [math]::Max(0, $bytes.Length - 65557)
  for ($i = $bytes.Length - 22; $i -ge $limit; $i--) {
    if ($bytes[$i] -eq 0x50 -and $bytes[$i + 1] -eq 0x4B -and $bytes[$i + 2] -eq 0x05 -and $bytes[$i + 3] -eq 0x06) {
      $eocd = $i
      break
    }
  }
  if ($eocd -lt 0) { throw "Not a zip archive: $ZipPath" }
  $entries = [BitConverter]::ToUInt16($bytes, $eocd + 10)
  $offset = [int64][BitConverter]::ToUInt32($bytes, $eocd + 16)
  if ($entries -eq 0xFFFF -or $offset -eq 0xFFFFFFFF) {
    throw "Zip64 archives are not supported: $ZipPath"
  }
  $unixMadeBy = [BitConverter]::GetBytes([uint16]0x031E)
  for ($n = 0; $n -lt $entries; $n++) {
    if ([BitConverter]::ToUInt32($bytes, $offset) -ne 0x02014B50) { throw "Corrupt central directory in $ZipPath" }
    $nameLength = [BitConverter]::ToUInt16($bytes, $offset + 28)
    $extraLength = [BitConverter]::ToUInt16($bytes, $offset + 30)
    $commentLength = [BitConverter]::ToUInt16($bytes, $offset + 32)
    $name = [Text.Encoding]::UTF8.GetString($bytes, $offset + 46, $nameLength)
    $isDir = $name.EndsWith('/')
    $typeFlag = if ($isDir) { '5' } else { '0' }
    $mode = Get-PayloadMode -EntryName $name -TypeFlag $typeFlag
    [Array]::Copy($unixMadeBy, 0, $bytes, $offset + 4, 2)
    # The high 16 bits hold the st_mode (including S_IFREG/S_IFDIR) once the
    # creating host is recorded as UNIX.
    $attributes = [int](($mode -bor $(if ($isDir) { 0x4000 } else { 0x8000 })) -shl 16)
    [Array]::Copy([BitConverter]::GetBytes($attributes), 0, $bytes, $offset + 38, 4)
    $offset += 46 + $nameLength + $extraLength + $commentLength
  }
  [IO.File]::WriteAllBytes($ZipPath, $bytes)
}

function New-PortableTarGz {
  param([string]$SourceDir, [string]$OutFile, [string]$TarExe, [string]$TempDir)
  $tarPath = Join-Path $TempDir 'payload.tar'
  Remove-Item $tarPath -ErrorAction SilentlyContinue
  # A portable (pax) archive so long resource paths survive GNU tar on Linux.
  & $TarExe --format=pax -cf $tarPath -C $SourceDir .
  if ($LASTEXITCODE) { throw 'tar creation failed' }
  Set-TarUnixModes -TarPath $tarPath
  Remove-Item $OutFile -ErrorAction SilentlyContinue
  $input = [IO.File]::OpenRead($tarPath)
  try {
    $output = [IO.File]::Open($OutFile, [IO.FileMode]::Create)
    try {
      $gzip = New-Object IO.Compression.GZipStream($output, [IO.Compression.CompressionLevel]::Optimal)
      try { $input.CopyTo($gzip) } finally { $gzip.Dispose() }
    } finally { $output.Dispose() }
  } finally {
    $input.Dispose()
    Remove-Item $tarPath -ErrorAction SilentlyContinue
  }
}

function New-PortableZip {
  param([string]$SourceDir, [string]$OutFile)
  Remove-Item $OutFile -ErrorAction SilentlyContinue
  $stream = [IO.File]::Open($OutFile, [IO.FileMode]::Create)
  $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
  try {
    $rootLength = $SourceDir.Length + 1
    # Directories are listed explicitly so extraction creates 0755 directories.
    $entries = @(Get-ChildItem -LiteralPath $SourceDir -Recurse -Force -Directory)
    $entries += @(Get-ChildItem -LiteralPath $SourceDir -Recurse -Force -File)
    foreach ($item in $entries) {
      $relative = $item.FullName.Substring($rootLength).Replace('\', '/')
      if ($item.PSIsContainer) { $relative += '/' }
      $entry = $archive.CreateEntry($relative, [IO.Compression.CompressionLevel]::Optimal)
      if ($item.PSIsContainer) { continue }
      $entryStream = $entry.Open()
      try {
        $file = [IO.File]::OpenRead($item.FullName)
        try { $file.CopyTo($entryStream) } finally { $file.Dispose() }
      } finally { $entryStream.Dispose() }
    }
  } finally {
    $archive.Dispose()
    $stream.Dispose()
  }
  Set-ZipUnixModes -ZipPath $OutFile
}

function New-SelfExtractingInstaller {
  param([string]$TarGz, [string]$OutFile)
  # The header must use LF endings: it is executed by bash on Linux.
  $header = @'
#!/usr/bin/env bash
set -euo pipefail
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
line="$(awk '/^__SERE_PAYLOAD__$/ {print NR + 1; exit}' "$0")"
tail -n +"$line" "$0" | tar -xz -C "$work"
bash "$work/install.sh" "$@"
exit 0
__SERE_PAYLOAD__
'@
  $header = ($header -replace "`r`n", "`n")
  if (-not $header.EndsWith("`n")) { $header += "`n" }
  Remove-Item $OutFile -ErrorAction SilentlyContinue
  $stream = [IO.File]::Open($OutFile, [IO.FileMode]::Create)
  try {
    $bytes = [Text.Encoding]::ASCII.GetBytes($header)
    $stream.Write($bytes, 0, $bytes.Length)
    $payload = [IO.File]::OpenRead($TarGz)
    try { $payload.CopyTo($stream) } finally { $payload.Dispose() }
  } finally {
    $stream.Dispose()
  }
}

# ---------------------------------------------------------------------------
# Stage the payload.
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# Without a build tree the distro is built inside a Linux environment: the
# linux-clang-relwithdebinfo preset is Linux-only and LLVM is downloaded as a
# Linux toolchain, so no Windows toolchain can produce this compiler.
# ---------------------------------------------------------------------------
function Test-WslShare {
  param([string]$Name)
  foreach ($prefix in @('\\wsl.localhost', '\\wsl$')) {
    if (Test-Path -LiteralPath "$prefix\$Name\" -ErrorAction SilentlyContinue) { return $true }
  }
  return $false
}

function Get-WslDistro {
  # wsl.exe is never invoked to probe: when WSL is absent it prompts to install
  # and then aborts after a timeout. Registered distributions live in the
  # registry, and the \\wsl.localhost share proves one is reachable.
  $key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Lxss'
  if (-not (Test-Path -LiteralPath $key)) { return $null }
  $candidates = Get-ChildItem -LiteralPath $key -ErrorAction SilentlyContinue |
    ForEach-Object { (Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction SilentlyContinue).DistributionName } |
    Where-Object { $_ }
  foreach ($candidate in $candidates) {
    if (Test-WslShare -Name $candidate) { return $candidate }
  }
  return $null
}

function Test-DockerHost {
  $cli = (Get-Command docker -ErrorAction SilentlyContinue).Source
  if (-not $cli) { return $false }
  $previous = $ErrorActionPreference
  $ErrorActionPreference = 'SilentlyContinue'
  try {
    $null = & $cli info --format '{{.ServerVersion}}' 2>&1
    return ($LASTEXITCODE -eq 0)
  } catch {
    return $false
  } finally {
    $ErrorActionPreference = $previous
  }
}

function Resolve-LinuxEngine {
  param([string]$Requested, [string]$DistroName)
  if ($Requested -ne 'Auto') { return $Requested }
  if ($IsLinux) { return 'Local' }
  if ($DistroName) { return 'Wsl' }
  if (Test-DockerHost) { return 'Docker' }
  return $null
}

function ConvertTo-WslPath {
  param([string]$Path)
  $value = $Path -replace '\\', '/'
  if ($value -match '^([A-Za-z]):/(.*)$') { return '/' + $Matches[1].ToLowerInvariant() + '/' + $Matches[2] }
  return $value
}

function Invoke-LinuxBuild {
  param([string]$Kind, [string]$DistroName, [string]$ImageName, [string]$DriverPath, [string]$VsixPath)

  if ($VsixPath) {
    # stage.sh bundles the newest dist/sere-*.vsix, so the supplied extension is
    # placed where that glob finds it.
    New-Item -ItemType Directory -Force -Path (Join-Path $repo 'dist') | Out-Null
    Copy-Item -LiteralPath $VsixPath -Destination (Join-Path $repo "dist\sere-$Name.vsix") -Force
    Write-Host "Using $VsixPath as the editor extension"
  }

  $driverArguments = @('--name', $Name, '--work', "/opt/sere-release/$Name")
  if ($SkipBuild) { $driverArguments += '--skip-build' }
  if ($Test) { $driverArguments += '--test' }
  if ($WithPayload) { $driverArguments += '--with-payload' }
  if ($WithoutEditor) { $driverArguments += '--without-editor' }

  switch ($Kind) {
    'Local' {
      Write-Host 'Building the Linux distro in this environment'
      & bash $DriverPath --repo $repo @driverArguments
      if ($LASTEXITCODE) { throw "The Linux build failed (exit $LASTEXITCODE)" }
    }
    'Wsl' {
      $wsl = (Get-Command wsl -ErrorAction SilentlyContinue).Source
      if (-not $wsl) { throw 'wsl.exe not found, but a WSL distribution is registered' }
      $repoPath = ConvertTo-WslPath -Path $repo
      $driverLinux = ConvertTo-WslPath -Path $DriverPath
      # WSL forwards a command line to the distribution, so spaces would split it.
      foreach ($value in @($repoPath, $driverLinux)) {
        if ($value -match '\s') { throw "WSL cannot carry a path containing spaces: $value" }
      }
      Write-Host "Building the Linux distro in WSL ($DistroName)"
      # root avoids interactive sudo prompts for the build dependencies.
      & $wsl -d $DistroName -u root -- bash $driverLinux --repo $repoPath @driverArguments
      if ($LASTEXITCODE) { throw "The Linux build failed (exit $LASTEXITCODE)" }
    }
    'Docker' {
      $docker = (Get-Command docker -ErrorAction SilentlyContinue).Source
      if (-not $docker) { throw 'docker not found' }
      $driverDir = Split-Path -Parent $DriverPath
      $driverName = Split-Path -Leaf $DriverPath
      Write-Host "Building the Linux distro in $ImageName"
      # The toolchain lives in a volume so repeated runs do not re-download LLVM.
      & $docker run --rm --platform linux/amd64 `
        -v "${driverDir}:/driver:ro" `
        -v "${repo}:/mnt/repo" `
        -v 'sere-linux-toolchain:/root/.local/share' `
        $ImageName bash "/driver/$driverName" --repo /mnt/repo @driverArguments
      if ($LASTEXITCODE) { throw "The Linux build failed (exit $LASTEXITCODE)" }
    }
  }
}

if (-not $BuildDir) {
  $driverPath = Join-Path $repo 'releases\linux-build.sh'
  if (-not (Test-Path -LiteralPath $driverPath)) { throw "Linux build driver missing: $driverPath" }
  if ($SkipArchive) { throw '-SkipArchive requires -BuildDir; the Linux build always stages an archive' }

  $distroName = if ($Distro) { $Distro } else { Get-WslDistro }
  $resolvedEngine = Resolve-LinuxEngine -Requested $Engine -DistroName $distroName
  if (-not $resolvedEngine) {
    Write-Host (@(
        'No Linux environment is available to build the Linux distro.',
        'The compiler has to be built on Linux: the linux-clang-relwithdebinfo preset is Linux-only',
        'and LLVM is fetched as a Linux toolchain. Use one of:',
        '  - WSL2:  wsl --install   (as administrator, then reopen the terminal)',
        '  - Docker Desktop',
        '  - run releases/linux-build.sh on a Linux host or in CI',
        'To package a Linux build produced elsewhere instead:',
        '  .\releases\stage-linux.ps1 -BuildDir <linux build> -LlvmRoot <linux llvm>'
      ) -join [Environment]::NewLine) -ForegroundColor Red
    exit 2
  }
  if ($resolvedEngine -eq 'Local' -and -not $IsLinux) {
    Write-Host '-Engine Local requires running this script on Linux' -ForegroundColor Red
    exit 2
  }
  if ($resolvedEngine -eq 'Wsl' -and -not (Test-WslShare -Name $distroName)) {
    # Without this guard wsl.exe would offer to install WSL interactively.
    Write-Host "WSL distribution '$distroName' is not reachable. Register one with 'wsl --install'." -ForegroundColor Red
    exit 2
  }

  Invoke-LinuxBuild -Kind $resolvedEngine -DistroName $distroName -ImageName $Image -DriverPath $driverPath -VsixPath $Vsix
  Write-Host "Linux release ready: $releaseRoot"
  Get-ChildItem -LiteralPath $releaseRoot -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like 'Sere-*-linux-x64*' -or $_.Name -eq 'SHA256SUMS-linux.txt' } |
    ForEach-Object { Write-Host ("   {0} ({1:N1} MB)" -f $_.Name, ($_.Length / 1MB)) }
  return
}

$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$compiler = Find-LinuxCompiler -Root $BuildDir
if (-not $compiler) {
  throw "No Linux compiler found in $BuildDir. Pass -BuildDir with a Linux build (expected <dir>\bin\sere), or configure and build on Linux with cmake --preset linux-clang-relwithdebinfo."
}
$compilerDir = Split-Path -Parent $compiler
$runtime = Find-LinuxRuntime -CompilerDir $compilerDir -Root $BuildDir
if (-not $runtime) { throw 'libsere_rt.a not found; stage the runtime archive from the Linux build' }
Assert-LinuxBinary -Path $compiler -What 'The compiler'
Assert-LinuxArchive -Path $runtime -What 'The runtime archive'

if (-not $LlvmRoot) { $LlvmRoot = $env:SERE_LLVM_DIR }
if (-not $LlvmRoot -and $env:LOCALAPPDATA) { $LlvmRoot = Join-Path $env:LOCALAPPDATA "sere\toolchains\llvm-$LLVM_VERSION" }
if (-not $LlvmRoot) { $LlvmRoot = Join-Path $HOME ".local/share/sere/toolchains/llvm-$LLVM_VERSION" }
$LlvmRoot = [IO.Path]::GetFullPath($LlvmRoot)
if (-not (Test-Path -LiteralPath (Join-Path $LlvmRoot 'bin\clang')) -or
    -not (Test-Path -LiteralPath (Join-Path $LlvmRoot 'bin\ld.lld'))) {
  throw "Linux LLVM $LLVM_VERSION not found at $LlvmRoot (needs bin/clang and bin/ld.lld). Pass -LlvmRoot with the Linux toolchain to bundle."
}
$tarExe = (Get-Command tar -ErrorAction SilentlyContinue).Source
if (-not $tarExe) { throw 'tar is required to build the Linux archives' }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

Write-Host "Staging Linux $Name from $compiler"
Write-Host "Bundling Linux LLVM from $LlvmRoot"
New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null
if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$dest\bin", "$dest\include\sere\api", "$dest\examples",
  "$dest\docs", "$dest\packaging", "$dest\editors" | Out-Null

Copy-Required $compiler "$dest\bin\sere"
Copy-Required $runtime "$dest\bin\libsere_rt.a"
Copy-Required $runtime "$dest\bin\sere_rt.a"
Copy-Required "$repo\scripts\sere-path.sh" "$dest\bin\sere-path.sh"
Copy-Required "$repo\scripts\bootstrap-llvm.sh" "$dest\packaging\bootstrap-llvm.sh"
Copy-Required "$repo\packaging\install.sh" "$dest\install.sh"
Copy-Required "$repo\packaging\uninstall.sh" "$dest\uninstall.sh"
Copy-Required "$repo\packaging\README-linux.md" "$dest\README.md"
Copy-Contents "$repo\stdlib" "$dest\stdlib"
# Windows-only stdlib must never reach a Linux distro.
Remove-Item -LiteralPath "$dest\stdlib\windows.sere" -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath "$repo\include\sere\api") { Copy-Contents "$repo\include\sere\api" "$dest\include\sere\api" }
Copy-Required "$repo\LICENSE" "$dest\LICENSE"
if (Test-Path -LiteralPath "$repo\docs") { Copy-Contents "$repo\docs" "$dest\docs" }
foreach ($example in (Get-ChildItem "$repo\examples" -Filter '*.sere' -File)) {
  if ((Get-Content -LiteralPath $example.FullName) -match '(?m)^\s*import windows\s*$') { continue }
  Copy-Required $example.FullName "$dest\examples\$($example.Name)"
}

$llvm = Join-Path $dest "toolchains\llvm-$LLVM_VERSION"
New-Item -ItemType Directory -Force -Path "$llvm\bin" | Out-Null
$tools = @('clang', 'clang++', 'clang-22', 'clang-22.1', 'clang-22.1.8', 'ld.lld', 'lld', 'lld-link',
  'llvm-ar', 'llvm-ranlib', 'llvm-nm', 'llvm-objcopy', 'llvm-strip', 'llvm-config')
foreach ($tool in $tools) {
  $source = Join-Path $LlvmRoot "bin\$tool"
  if (Test-Path -LiteralPath $source -PathType Leaf) { Copy-Required $source "$llvm\bin\$tool" }
}
if (Test-Path -LiteralPath (Join-Path $LlvmRoot 'lib\clang')) {
  Copy-Contents (Join-Path $LlvmRoot 'lib\clang') "$llvm\lib\clang"
}
# Only the libraries the bundled tools actually link: not the full LLVM SDK.
foreach ($pattern in @('libLLVM*.so*', 'libclang*.so*', 'libLTO.so*', 'LLVMgold.so')) {
  Get-ChildItem -Path (Join-Path $LlvmRoot 'lib') -Filter $pattern -File -ErrorAction SilentlyContinue |
    ForEach-Object { Copy-Required $_.FullName "$llvm\lib\$($_.Name)" }
}
Get-ChildItem $LlvmRoot -Filter '*LICENSE*' -File | ForEach-Object { Copy-Required $_.FullName "$llvm\$($_.Name)" }
if (-not (Test-Path -LiteralPath "$llvm\bin\clang") -or -not (Test-Path -LiteralPath "$llvm\bin\ld.lld")) {
  throw "Bundled LLVM is missing clang or ld.lld under $llvm"
}
# A Windows LLVM install has clang.exe/ld.lld.exe and no bare names, but a stray
# one copied next to the tree would pass the existence check above.
Assert-LinuxBinary -Path "$llvm\bin\clang" -What 'The bundled clang'
Assert-LinuxBinary -Path "$llvm\bin\ld.lld" -What 'The bundled ld.lld'

$hasVsix = $false
if (-not $WithoutEditor) {
  if (-not $Vsix) {
    $Vsix = Get-ChildItem "$repo\dist" -Filter 'sere-*.vsix' -File -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
  }
  if (-not $Vsix) {
    $Vsix = Get-ChildItem "$releaseRoot\windows-x64\editors" -Filter 'sere*.vsix' -File -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
  }
  if ($Vsix) {
    Copy-Required $Vsix "$dest\editors\sere.vsix"
    $hasVsix = $true
  } else {
    Write-Warning 'No VSIX found for editors/sere.vsix; pass -Vsix or use -WithoutEditor'
  }
}

$commit = & git -C $repo rev-parse --short HEAD 2>$null
if (-not $commit) { $commit = 'unknown' }
$manifest = @(
  "Sere $Name"
  'platform: linux-x64'
  "self-contained: yes (compiler + LLVM $LLVM_VERSION + stdlib + runtime)"
  "staged: $([DateTime]::UtcNow.ToString('yyyy-MM-dd HH:mm:ss')) UTC"
  "git: $commit"
  "llvm: $LLVM_VERSION (bundled under toolchains/llvm-$LLVM_VERSION)"
  'glibc baseline: 2.35 (Ubuntu 22.04)'
  "compiler: $compiler"
  "llvm source: $LlvmRoot"
  "editor extension: $(if ($hasVsix) { 'editors/sere.vsix' } else { 'none' })"
) -join "`n"
[IO.File]::WriteAllText("$dest\MANIFEST.txt", $manifest + "`n", [Text.UTF8Encoding]::new($false))

# Fail loudly rather than shipping a payload that cannot run on Linux.
$pe = Get-ChildItem $dest -Recurse -File -Force -Include '*.exe', '*.dll', '*.lib', '*.pdb'
if ($pe) {
  throw "Refusing to ship PE objects in the Linux distro:`n$($pe.FullName -join "`n")"
}
if (Test-Path -LiteralPath "$dest\stdlib\windows.sere") { throw 'windows.sere must not be in the Linux distro' }

Write-Host "Linux payload ready: $dest"
if ($SkipArchive) { return }

Write-Host 'Compressing portable archives'
$archiveName = "Sere-$Name-linux-x64"
$tarGz = Join-Path $releaseRoot "$archiveName-portable.tar.gz"
$zip = Join-Path $releaseRoot "$archiveName-portable.zip"
$installer = Join-Path $releaseRoot "$archiveName-setup.run"
$tempDir = Join-Path ([IO.Path]::GetTempPath()) ("sere-linux-" + [Guid]::NewGuid().ToString('n'))
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
try {
  New-PortableTarGz -SourceDir $dest -OutFile $tarGz -TarExe $tarExe -TempDir $tempDir
  New-PortableZip -SourceDir $dest -OutFile $zip
  New-SelfExtractingInstaller -TarGz $tarGz -OutFile $installer
} finally {
  Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
}

# sha256sum output format, so the Linux checksum files stay interchangeable.
@($tarGz, $installer) | ForEach-Object {
  '{0}  {1}' -f (Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant(), (Split-Path $_ -Leaf)
} | Set-Content "$releaseRoot\SHA256SUMS-linux.txt" -Encoding ascii

Write-Host "tar.gz    $tarGz"
Write-Host "zip       $zip"
Write-Host "installer $installer"
Write-Host "in-place: . $dest/bin/sere-path.sh"
Write-Host "Linux release ready: $releaseRoot"
