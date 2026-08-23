# Sere pre-0.1.1

Windows x64 package. Unzip this folder, then run `install.ps1` (or
`install.cmd`) to copy files to `%LOCALAPPDATA%\Programs\Sere`, add `bin`
to the user PATH, and set `SERE_STDLIB` / `SERE_LLVM_DIR`.

```powershell
.\install.ps1
sere --version
```

The editor VSIX is `editors\sere.vsix`. Pass `-Editor` to install it, or
use **Extensions â†’ Install from VSIXâ€¦** in Cursor / VS Code.
