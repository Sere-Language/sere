# Sere for Windows x64

Extract the complete archive anywhere and run `bin\sere.exe`. No administrator
access, LLVM installation, Visual Studio installation, or downloads are needed.
Windows 10 or newer is required.

Install into your user account and add Sere to your user PATH:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\install.ps1
# Include the bundled VS Code/Cursor extension:
powershell -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -Editor
# Custom location, without changing PATH:
.\install.ps1 -Prefix "$env:USERPROFILE\tools\sere" -NoPath
```

Open a new terminal after installation. The setup EXE installs per user too:
`Sere-pre-x.x.x-windows-x64-setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART`.
Add `/TASKS="adduserpath,vscodeext"` to opt into the editor extension.
Use `uninstall.cmd` for script installations or Windows Installed Apps for setup
installations. Do not mix installation methods in the same folder.

The bundle contains clang, lld-link, llvm-ar, clang resources, x64 C/C++ headers
and link libraries, app-local runtime DLLs, Sere runtime, stdlib and C API.
LLVM development libraries, unused LLVM tools and debug symbols are omitted.
Virtual environments keep a reference to this installation for compiler tools;
keep it installed while those environments are in use.

Loose native C/C++ sources are supported. Projects explicitly using CMake or
other build systems must supply those development tools themselves.
The editor extension requires an existing VS Code or Cursor installation.
