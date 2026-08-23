# Packaging and the Windows installer

## Zip / copy install (`releases/`)

The first shippable tree is [`releases/pre-0.1.0`](../releases/pre-0.1.0/).
It is a manual install: unzip the folder, then run `install.ps1` (or
`install.cmd`) to copy files to `%LOCALAPPDATA%\Programs\Sere`, add `bin` to
the user `PATH`, and set `SERE_STDLIB` / `SERE_LLVM_DIR`. LLVM is reused from
a previous bootstrap or downloaded; it is not stored in git.

```powershell
.\releases\stage.ps1
.\releases\pre-0.1.0\install.ps1
.\releases\pre-0.1.0\uninstall.ps1
```

Options: `-Prefix`, `-NoPath`, `-Associate`, `-Editor`, `-Msvc`, `-DownloadLlvm`.

## Inno Setup wizard

`sere --build-installer` (alias `sere build-installer`) stages the compiler,
stdlib, LLVM 22.1.8 toolchain, runtime, C API headers, optional Qt6 DLLs, and
the editor VSIX, then compiles an Inno Setup installer.

```powershell
.\scripts\bootstrap-innosetup.ps1   # once, if ISCC.exe is missing
sere --build-installer
sere --build-installer -o dist\Sere-0.1.0-setup.exe
```

The setup exe is written to `dist/Sere-<version>-setup.exe` by default.

## What the installer puts on the machine

Default directory: `%LOCALAPPDATA%\Programs\Sere`.

| Path | Contents |
| --- | --- |
| `bin/sere.exe` | Compiler, project CLI, and `sere --lsp` |
| `bin/sere_rt.lib` | Runtime linked into user programs |
| `stdlib/` | Standard library |
| `include/sere/api/` | `sere_mod.h`, `sere_gc.h` |
| `toolchains/llvm-22.1.8/` | clang, lld, clang resource dir |
| `editors/sere.vsix` | VS Code / Cursor extension |

Wizard tasks: user/system PATH, LLVM, C++ Build Tools if missing, Start Menu,
`.sere` association, editor extension (`sere --lsp`), optional Qt6 runtime.

After install, a new terminal should run `sere --version` and compile a
`.sere` file without this git checkout or `scripts/bootstrap.ps1`.

From a source checkout, `sere refresh-bin` (or **Sere: Refresh ./bin Compiler**)
replaces `./bin/sere.exe` from the compiler that is currently running. CMake
install also renames a locked `bin/sere.exe` out of the way.

Building Sere **from source** still uses `scripts/bootstrap.ps1` and
`scripts/env.ps1`. Those are developer scripts, not end-user steps.
