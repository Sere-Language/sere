# Sere releases

`releases/` holds the installable trees. `pre-0.1.0` is the previous drop;
`pre-0.1.1` is current.

```powershell
.\scripts\package-vsix.ps1
.\releases\stage.ps1                  # default: pre-0.1.1
.\releases\stage.ps1 -Name pre-0.1.1
```

That writes:

- `releases/pre-0.1.1/` — unzipped install tree
- `releases/Sere-pre-0.1.1-windows-x64.zip`
- `releases/sere-0.2.1.vsix` (also `pre-0.1.1/editors/sere.vsix`)

LLVM is **not** stored in git. `install.ps1` reuses a toolchain already on the
machine or downloads the pinned clang+llvm archive.
