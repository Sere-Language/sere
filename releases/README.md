# Sere releases

Each subdirectory is a shippable tree: compiler, runtime, stdlib, and install
scripts. Zip one folder and give it to someone; they can copy files by hand or
run `install.ps1`.

| Folder | Channel | Notes |
| --- | --- | --- |
| [pre-0.1.0](pre-0.1.0/) | Pre-release | First manual Windows x64 package |

The Inno Setup wizard (`sere --build-installer` → `dist/Sere-<version>-setup.exe`)
is a separate, later path. These folders are the zip / copy install.

Refresh a folder from a local compiler build:

```powershell
.\releases\stage.ps1
.\releases\stage.ps1 -Name pre-0.1.0
```

LLVM is **not** stored in git (too large). `install.ps1` reuses a toolchain
already on the machine or downloads the pinned clang+llvm archive.
