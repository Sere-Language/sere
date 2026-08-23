# Packaging and the Windows installer

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
