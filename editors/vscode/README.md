# Sere

Language support for the Sere compiler in Cursor and VS Code.

The language server is the same `sere` binary (`sere --lsp`).

## Features

- Syntax highlighting (TextMate + semantic tokens)
- Diagnostics with exception codes (`NameError`, `TypeError`, …)
- Hover types, including `&x: Ptr[T]` and `*p: T`
- Completion (members, imports, keywords, `unique` / `shared` / `alloc`)
- Parameter hints while typing call arguments (`min(left: i32, right: i32)`)
- Signature help, inlay hints, and snippets
- Go to definition, type definition, implementation, and references
- Rename, document highlight, document / workspace symbols
- Folding, format-on-request, and code lens reference counts
- `# type: ignore` and `# type[NameError]: ignore` suppress diagnostics

## Setup (system install)

If you installed Sere with the Windows installer and left **Add sere to PATH** and **Install the Sere editor extension** checked, this extension is already installed. `sere.compilerPath` can stay empty: the client finds `sere` on PATH.

## Setup (this repository)

1. Build `sere` from the Sere repository.
2. Install this VSIX: **Extensions → … → Install from VSIX…**
3. Point `sere.compilerPath` at `sere.exe` if it is not already on `PATH`.

In this repo the workspace default is:

`${workspaceFolder}/build/windows-clang-cl-relwithdebinfo/bin/sere.exe`

After rebuilding the compiler, run **Sere: Restart Language Server**.
