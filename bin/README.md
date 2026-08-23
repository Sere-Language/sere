Built copies of `sere` and `sere_rt` land here after `cmake --build`. This directory is a convenience output, not source.

```powershell
.\bin\sere.exe examples\hello.sere -o hello.exe
```

Put this folder on `PATH` (session, or user PATH for new terminals):

```powershell
.\bin\sere-path.ps1
.\bin\sere-path.ps1 -Persistent
```

`sere-path.cmd` and `sere-path.sh` do the same thing from cmd or bash. The scripts live in `scripts/` and are copied here on each compiler build.

Clang is resolved from the pinned LLVM toolchain; you do not need to source `scripts/env.ps1` just to compile a program.
