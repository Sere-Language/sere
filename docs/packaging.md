# Packaging

## Zip / copy install (`releases/`)

```powershell
.\scripts\package-vsix.ps1
.\releases\stage.ps1 -Name pre-0.1.4
```

```bash
./scripts/bootstrap-llvm.sh
export SERE_LLVM_DIR="$HOME/.local/share/sere/toolchains/llvm-22.1.8"
cmake --preset linux-clang-relwithdebinfo
cmake --build --preset linux-clang-relwithdebinfo --target sere
ctest --preset linux-clang-relwithdebinfo
./releases/stage.sh pre-0.1.4
```

| Artifact | Path |
| --- | --- |
| Windows tree | `releases/pre-0.1.4/windows-x64/` |
| Linux tree | `releases/pre-0.1.4/linux-x64/` |
| Windows zip | `releases/Sere-pre-0.1.4-windows-x64.zip` |
| Linux tar.gz | `releases/Sere-pre-0.1.4-linux-x64.tar.gz` |

Linux `stage.sh` **bundles a compiler LLVM** into `toolchains/llvm-22.1.8` (`clang`, `ld.lld`, clang resource dir, and LLVM/clang shared libraries). It does **not** copy the full LLVM SDK (static libs and unused tools); that produced a ~12 G tree and archives Cursor cannot upload. After unpack, `. ./bin/sere-path.sh` compiles offline. `install.sh` copies that toolchain into `$HOME/.local/share/sere/toolchains`. `--download-llvm` is only a fallback. The builder still uses `scripts/bootstrap-llvm.sh` and [LLVM-22.1.8-Linux-X64.tar.xz](https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.8).

Linux stdlib **omits** `windows.sere`. `sere --build-installer` remains Windows-only (Inno Setup).

The Linux compiler is **x86_64 glibc 2.35**. It will not run on the [NanoVM](https://userland.run/docs/) RV64GC RISC-V runner.

## Inno Setup wizard (Windows)

`sere --build-installer` stages the compiler, stdlib, LLVM 22.1.8, runtime, C API headers, optional Qt6 DLLs, and the editor VSIX.

```powershell
.\scripts\bootstrap-innosetup.ps1
sere --build-installer
```

Default directory: `%LOCALAPPDATA%\Programs\Sere`.
