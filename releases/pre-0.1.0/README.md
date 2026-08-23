# Sere pre-0.1.0

First public-style Windows x64 package. This is a **manual** release: unzip or
copy this folder, then either leave it where it is or run the install script.

The script puts the compiler on `PATH`, points Sere at this stdlib and the
pinned LLVM 22.1.8 toolchain, and can install C++ link tools and the editor
extension.

## Quick install

From this folder in PowerShell:

```powershell
.\install.ps1
```

or double-click `install.cmd`.

Default location: `%LOCALAPPDATA%\Programs\Sere`  
(`C:\Users\<you>\AppData\Local\Programs\Sere`)

Open a **new** terminal and check:

```powershell
sere --version
sere examples\hello.sere -o hello.exe
.\hello.exe
```

If you keep this folder and do not want a copy under Programs:

```powershell
.\bin\sere-path.ps1 -Persistent
$env:SERE_STDLIB = (Resolve-Path .\stdlib).Path
```

You still need LLVM 22.1.8 and MSVC link libraries (see below).

## What gets installed

| Path under the prefix | Contents |
| --- | --- |
| `bin\sere.exe` | Compiler, project CLI, `sere --lsp` |
| `bin\sere_rt.lib` | Runtime linked into your programs |
| `stdlib\` | Standard library |
| `include\sere\api\` | `sere_mod.h`, `sere_gc.h` |
| `toolchains\llvm-22.1.8\` | clang / lld (copied or downloaded) |
| `examples\hello.sere` | Smoke program |
| `docs\language.md` | Language reference |

`install.ps1` also sets user environment variables:

- `PATH` — adds `bin`
- `SERE_STDLIB` — stdlib directory
- `SERE_LLVM_DIR` — LLVM root (the folder that contains `bin\clang.exe`)

## Options

```powershell
.\install.ps1                        # copy, PATH, LLVM reuse or download
.\install.ps1 -Prefix D:\Sere        # custom directory
.\install.ps1 -NoPath                # copy only
.\install.ps1 -Associate             # .sere file type
.\install.ps1 -Editor                # Cursor / VS Code VSIX if present
.\install.ps1 -Msvc                  # Visual Studio Build Tools if link libs are missing
.\install.ps1 -DownloadLlvm          # always fetch LLVM 22.1.8
.\uninstall.ps1                      # remove PATH / env and the prefix copy
```

## LLVM and C++ tools

Sere compiles through clang+lld. This zip does **not** ship the full LLVM
tree (hundreds of MB). Install looks for, in order:

1. `toolchains\llvm-22.1.8` next to this README (if you staged it)
2. `%LOCALAPPDATA%\sere\toolchains\llvm-22.1.8` (developer bootstrap)
3. An already-installed `%LOCALAPPDATA%\Programs\Sere\toolchains\llvm-22.1.8`
4. Download of the official `clang+llvm-22.1.8-x86_64-pc-windows-msvc` archive

Linking also needs the MSVC x64 libraries and the Windows SDK. `install.ps1`
does not install those unless you pass `-Msvc` (same helper as the Inno
wizard).

## Manual layout

If you copy by hand, keep this shape so `sere` finds clang without extra env:

```text
Sere\
  bin\sere.exe
  bin\sere_rt.lib
  stdlib\
  include\sere\api\
  toolchains\llvm-22.1.8\bin\clang.exe
```

Then add `Sere\bin` to PATH.

## License

MIT. See `LICENSE`.
