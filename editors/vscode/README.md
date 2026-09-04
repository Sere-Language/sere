# Sere

Language support for the Sere compiler in Cursor and VS Code.

The language server is the same `sere` binary (`sere --lsp`).

## Features

- Syntax highlighting (TextMate + semantic tokens)
- Diagnostics with exception codes (`NameError`, `TypeError`, …)
- Hover types, including `&x: Ptr[T]` and `*p: T`
- Scope-aware completion (locals, parameters, imports, keywords, macros, exceptions)
- Type-directed members for variables and literals, such as `"hello".upper()`
- Method-call snippets with named argument tab stops
- Parameter hints, signature help, and inlay hints
- Go to definition, type definition, implementation, and references
- Rename, document highlight, document / workspace symbols
- Folding, format-on-request, and code lens reference counts
- `# type: ignore` and `# type[NameError]: ignore` suppress diagnostics
- Commands: compile the current file, `sere build`, `sere run`

## Install from VSIX

1. **Extensions → … → Install from VSIX…**
2. Choose `dist/sere-0.2.4.vsix`
3. Reload the window

If `sere` is on `PATH` (Windows installer, `.\bin\sere-path.ps1`, or `. .\scripts\activate.ps1`), leave `sere.compilerPath` empty.

## Setup (this repository)

1. Build `sere`.
2. Install the VSIX from `dist/sere-0.2.4.vsix`.
3. Point `sere.compilerPath` at `sere.exe` if it is not on `PATH`.

Workspace default in this repo:

`${workspaceFolder}/build/windows-clang-cl-relwithdebinfo/bin/sere.exe`

After rebuilding the compiler, run **Sere: Restart Language Server**.

## Commands

| Command | Action |
| --- | --- |
| Sere: Restart Language Server | Restart `sere --lsp` |
| Sere: Compile Current File | Compile the open `.sere` file |
| Sere: Build Project | `sere build` in the nearest `sere.toml` |
| Sere: Run Project | `sere run` in the nearest `sere.toml` |
| Sere: Set Stdlib Folder | Pick the folder that contains `prelude.sere` |
| Sere: Set Stdlib Folder | Pick the folder that contains `prelude.sere` |
| Sere: Open Settings | Open Sere settings |
