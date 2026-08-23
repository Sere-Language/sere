# Changelog

## 0.2.1

- Ships with Sere **pre-0.1.1**
- Member completion for local classes, imported modules, and stdlib exports
- **Sere: Set Stdlib Folder** picks a `prelude.sere` directory (`sere.stdlibPath`)

## 0.2.0

- Grammar for builtin exceptions, `Int` / `Float`, `|>`, and macro properties
- Snippets for `raise TypeError`, `except TypeError as e`, custom exceptions, imports
- Language server stdlib discovery skips empty `venv/stdlib` (requires `prelude.sere`)
- Commands: **Build Project** and **Run Project**
- Extension and language icon (`icon.ico`) and downloadable `sere-0.2.0.vsix`

## 0.1.1

- Parameter hints while typing function, method, constructor, and macro arguments
- Highlights the current parameter and keeps the hint open on incomplete `foo(` calls

## 0.1.0

- Language id `sere` for `.sere` files
- TextMate grammar, snippets, and indentation rules
- Language server client for hover, completion, diagnostics, rename, symbols, folding, formatting, highlights, and semantic tokens
- Commands: Restart Language Server, Compile Current File, Open Settings
