# Packages editors/vscode into a Cursor/VS Code .vsix (no npm required).

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$ExtDir = Join-Path $Root "editors\vscode"
$PackageJson = Get-Content -Raw -Path (Join-Path $ExtDir "package.json") | ConvertFrom-Json
$Name = $PackageJson.name
$Version = $PackageJson.version
$Publisher = $PackageJson.publisher
$OutFile = Join-Path $ExtDir "$Name-$Version.vsix"

$Staging = Join-Path ([System.IO.Path]::GetTempPath()) ("sere-vsix-" + [Guid]::NewGuid().ToString("N"))
$ExtStaging = Join-Path $Staging "extension"
New-Item -ItemType Directory -Path $ExtStaging | Out-Null

$Files = @(
  "package.json",
  "extension.js",
  "language-configuration.json",
  "README.md",
  "CHANGELOG.md",
  "syntaxes\sere.tmLanguage.json",
  "snippets\sere.json"
)
foreach ($Rel in $Files) {
  $Src = Join-Path $ExtDir $Rel
  $Dest = Join-Path $ExtStaging $Rel
  New-Item -ItemType Directory -Path (Split-Path $Dest) -Force | Out-Null
  Copy-Item -Path $Src -Destination $Dest
}

$Manifest = @"
<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    <Identity Language="en-US" Id="$Name" Version="$Version" Publisher="$Publisher" />
    <DisplayName>$($PackageJson.displayName)</DisplayName>
    <Description xml:space="preserve">$($PackageJson.description)</Description>
    <Tags>sere</Tags>
    <Categories>Programming Languages</Categories>
    <GalleryFlags>Public</GalleryFlags>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="$($PackageJson.engines.vscode)" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionDependencies" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionPack" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionKind" Value="workspace" />
      <Property Id="Microsoft.VisualStudio.Code.LocalizedLanguages" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.EnabledApiProposals" Value="" />
    </Properties>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code"/>
  </Installation>
  <Dependencies/>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Content.Changelog" Path="extension/CHANGELOG.md" Addressable="true" />
  </Assets>
</PackageManifest>
"@
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText((Join-Path $Staging "extension.vsixmanifest"), $Manifest.TrimStart(), $Utf8NoBom)

$ContentTypes = @"
<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension=".json" ContentType="application/json" />
  <Default Extension=".vsixmanifest" ContentType="text/xml" />
  <Default Extension=".js" ContentType="application/javascript" />
  <Default Extension=".md" ContentType="text/markdown" />
</Types>
"@
[System.IO.File]::WriteAllText((Join-Path $Staging "[Content_Types].xml"), $ContentTypes.TrimStart(), $Utf8NoBom)

if (Test-Path $OutFile) {
  Remove-Item -Force $OutFile
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$Zip = [System.IO.Compression.ZipFile]::Open($OutFile, [System.IO.Compression.ZipArchiveMode]::Create)
try {
  function Add-ZipFile([string]$SourcePath, [string]$EntryName) {
    $Entry = $script:Zip.CreateEntry($EntryName.Replace("\", "/"), [System.IO.Compression.CompressionLevel]::Optimal)
    $Bytes = [System.IO.File]::ReadAllBytes($SourcePath)
    $Stream = $Entry.Open()
    try {
      $Stream.Write($Bytes, 0, $Bytes.Length)
    } finally {
      $Stream.Dispose()
    }
  }

  Add-ZipFile (Join-Path $Staging "extension.vsixmanifest") "extension.vsixmanifest"
  Add-ZipFile (Join-Path $Staging "[Content_Types].xml") "[Content_Types].xml"
  Get-ChildItem -Path $ExtStaging -Recurse -File | ForEach-Object {
    $Rel = $_.FullName.Substring($ExtStaging.Length).TrimStart("\")
    Add-ZipFile $_.FullName ("extension/" + $Rel.Replace("\", "/"))
  }
} finally {
  $Zip.Dispose()
}

Remove-Item -Recurse -Force $Staging
Write-Host "Wrote $OutFile"
