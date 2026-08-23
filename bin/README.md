Local compiler after `cmake --build`. This is the copy you put on `PATH`.

| Folder | Role |
| --- | --- |
| `build/` | CMake compile tree. The language server uses `build/<preset>/bin/sere.exe` so it does not lock this folder. |
| `bin/` | This directory: run `sere` day to day. |
| `dist/` | Packaged outputs only (`.vsix`, zip, installer). Never commit. |

```powershell
.\bin\sere.exe examples\hello.sere -o hello.exe
```

Put this folder on `PATH` (session, or user PATH for new terminals):

```powershell
.\bin\sere-path.ps1
.\bin\sere-path.ps1 -Persistent
```

`sere-path.cmd` and `sere-path.sh` do the same thing from cmd or bash. The scripts live in `scripts/` and are copied here on each compiler build.

After editing `stdlib/*.sere`, sync the copies next to `sere.exe` without a rebuild:

```powershell
.\scripts\refresh-stdlib.ps1
```

Clang is resolved from the pinned LLVM toolchain; you do not need to source `scripts/env.ps1` just to compile a program.
