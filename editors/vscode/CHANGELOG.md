# Changelog

## Unreleased

- Language server picks up stdlib, `sere.toml`, and project `src/` / `libs/`
  updates without **Restart Language Server**
- Unsaved stdlib and imported-module buffers overlay disk for diagnostics,
  hover, and completion
- Changing `sere.compilerPath` or `sere.stdlibPath` restarts the server and
  re-watches the resolved stdlib

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
