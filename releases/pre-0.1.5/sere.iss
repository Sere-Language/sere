; Sere Windows installer. Tokens are filled by sere --build-installer.
; Requires Inno Setup 6.

#define MyAppName "Sere"
#define MyAppVersion "pre-0.1.5"
#define MyAppPublisher "Sere"
#define MyAppExeName "sere.exe"
#define HasQt 0
#define HasVsix 1
#define LlvmVersion "22.1.8"

[Setup]
AppId={{A7C4E2F1-9B58-4D3A-8E71-2C6F0B91D5A3}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppVerName={#MyAppName} {#MyAppVersion}
DefaultDirName={localappdata}\Programs\Sere
DefaultGroupName=Sere
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5
OutputBaseFilename=Sere-pre-0.1.5-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesEnvironment=yes
ChangesAssociations=yes
MinVersion=10.0
SetupLogging=yes
UninstallDisplayIcon={app}\bin\{#MyAppExeName}
SetupIconFile=C:\Users\jackw\OneDrive\Desktop\git-projects\sere\icon.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "adduserpath"; Description: "Add sere to the user PATH"; GroupDescription: "Environment:"; Flags: checkedonce
Name: "startmenu"; Description: "Create Start Menu shortcuts"; GroupDescription: "Shortcuts:"; Flags: checkedonce
Name: "associate"; Description: "Associate .sere files with Sere"; GroupDescription: "File types:"; Flags: checkedonce
#if HasVsix
Name: "vscodeext"; Description: "Install the Sere editor extension (VS Code / Cursor) with language server"; GroupDescription: "Editor:"; Flags: unchecked
#endif
#if HasQt
Name: "qt6"; Description: "Install Qt6 GUI runtime (needed for import qt6)"; GroupDescription: "Optional libraries:"; Flags: checkedonce
#endif

[Files]
Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\stdlib\*"; DestDir: "{app}\stdlib"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\include\*"; DestDir: "{app}\include"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\packaging\*"; DestDir: "{app}\packaging"; Flags: ignoreversion
Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\toolchains\*"; DestDir: "{app}\toolchains"; Flags: ignoreversion recursesubdirs createallsubdirs
#if HasVsix
Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\editors\*"; DestDir: "{app}\editors"; Flags: ignoreversion
#endif
#if HasQt
Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\qt6\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: qt6
#endif

Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\README.md"; DestDir: "{app}"; Flags: ignoreversion

Source: "C:\Users\jackw\OneDrive\Desktop\git-projects\sere\releases\pre-0.1.5\windows-x64\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Sere"; Filename: "{cmd}"; Parameters: "/K ""set PATH={app}\bin;%PATH%&& set SERE_LLVM_DIR={app}\toolchains\llvm-{#LlvmVersion}&& set SERE_STDLIB={app}\stdlib&& echo Sere {#MyAppVersion}&& sere --version"""; WorkingDir: "{app}"; Tasks: startmenu
Name: "{group}\Uninstall Sere"; Filename: "{uninstallexe}"; Tasks: startmenu

[Registry]
Root: HKCU; Subkey: "Software\Classes\.sere"; ValueType: string; ValueName: ""; ValueData: "SereSourceFile"; Flags: uninsdeletevalue; Tasks: associate
Root: HKCU; Subkey: "Software\Classes\SereSourceFile"; ValueType: string; ValueName: ""; ValueData: "Sere Source File"; Flags: uninsdeletekey; Tasks: associate
Root: HKCU; Subkey: "Software\Classes\SereSourceFile\DefaultIcon"; ValueType: string; ValueData: "{app}\bin\{#MyAppExeName},0"; Tasks: associate
Root: HKCU; Subkey: "Software\Classes\SereSourceFile\shell\open\command"; ValueType: string; ValueData: """{app}\bin\{#MyAppExeName}"" ""%1"""; Tasks: associate

[Run]
#if HasVsix
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\packaging\install-vsix.ps1"" -Vsix ""{app}\editors\sere.vsix"" -Compiler ""{app}\bin\{#MyAppExeName}"""; StatusMsg: "Installing editor extension..."; Tasks: vscodeext; Flags: runhidden waituntilterminated
#endif

[Code]
function NeedsAddPath(const RootKey: Integer; const SubKey, Dir: string): Boolean;
var
  Orig: string;
  Check: string;
begin
  Result := True;
  if not RegQueryStringValue(RootKey, SubKey, 'Path', Orig) then
    Exit;
  Check := ';' + Uppercase(Orig) + ';';
  Result := Pos(';' + Uppercase(Dir) + ';', Check) = 0;
end;

procedure AddToPath(const RootKey: Integer; const SubKey, Dir: string);
var
  Orig: string;
begin
  if not NeedsAddPath(RootKey, SubKey, Dir) then
    Exit;
  if not RegQueryStringValue(RootKey, SubKey, 'Path', Orig) then
    Orig := '';
  if Orig = '' then
    Orig := Dir
  else if Orig[Length(Orig)] = ';' then
    Orig := Orig + Dir
  else
    Orig := Orig + ';' + Dir;
  RegWriteExpandStringValue(RootKey, SubKey, 'Path', Orig);
end;

procedure RemoveFromPath(const RootKey: Integer; const SubKey, Dir: string);
var
  Orig: string;
  UpperOrig: string;
  UpperDir: string;
  P: Integer;
begin
  if not RegQueryStringValue(RootKey, SubKey, 'Path', Orig) then
    Exit;
  UpperOrig := ';' + Uppercase(Orig) + ';';
  UpperDir := ';' + Uppercase(Dir) + ';';
  P := Pos(UpperDir, UpperOrig);
  if P = 0 then
    Exit;
  Delete(Orig, P, Length(Dir) + 1);
  if (Length(Orig) > 0) and (Orig[1] = ';') then
    Delete(Orig, 1, 1);
  if (Length(Orig) > 0) and (Orig[Length(Orig)] = ';') then
    Delete(Orig, Length(Orig), 1);
  RegWriteExpandStringValue(RootKey, SubKey, 'Path', Orig);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  BinDir: string;
begin
  if CurStep <> ssPostInstall then
    Exit;
  BinDir := ExpandConstant('{app}\bin');
  if WizardIsTaskSelected('adduserpath') then
    AddToPath(HKEY_CURRENT_USER, 'Environment', BinDir);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  BinDir: string;
begin
  if CurUninstallStep <> usPostUninstall then
    Exit;
  BinDir := ExpandConstant('{app}\bin');
  RemoveFromPath(HKEY_CURRENT_USER, 'Environment', BinDir);
end;

