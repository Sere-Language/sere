Built copies of `sere` and `sere_rt` land here after `cmake --build`. This directory is a convenience output, not source.

```powershell
.\bin\sere.exe examples\hello.sere -o hello.exe
```

Clang is resolved from the pinned LLVM toolchain; you do not need to source `scripts/env.ps1` just to compile a program.
