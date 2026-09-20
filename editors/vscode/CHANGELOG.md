# Changelog

## 0.2.8

- Member completion now resolves call and index receivers: `add(5, 4).` and `xs[0].` list the members of the returned type, including generic substitutions such as `unwrap() -> i32`
- `Type.` completion offers variant constructors with payload snippets, the constructor signature, and static members instead of instance fields
- Enum values complete their variant `name`/`value`
- Method and module completions show parameter names, type parameters, and declaration docstrings
- An error no longer disables the rest of the file: analysis is best-effort, so completion, hover, and semantic highlighting keep working next to diagnostics
- Request handlers always answer from the text the editor sent, so highlighting no longer goes stale or blank after edits
- Opening or editing a file only re-analyzes the documents that can observe the change, instead of every open tab
- Empty completion results fall back to the editor's own suggestions rather than an empty list

## 0.2.7

- Scope-aware completion hides locals and parameters from unrelated functions and blocks
- Type-directed member completion for string, list, and dictionary literals
- Method completions insert calls with named tab stops, such as `replace(${1:old}, ${2:new})`

## 0.2.6

- Ships with Sere **pre-0.1.4**
- Python-style custom decorators (`@fn`, `@fn(args)`, `@Class.method`)

## 0.2.5

- Ships with Sere **pre-0.1.3**
- Language server is the installed `sere.exe`; run `sere --update` to refresh `%LOCALAPPDATA%\\Programs\\Sere`

## 0.2.4

- Ships with Sere **pre1-0.1.2**
- `gl.Window` stays the GL class when a local `Window` exists (`import gl` does not alias names)

## 0.2.3

- Ships with Sere **pre1-0.1.2**
- Multiline list/dict/call literals (newlines and trailing commas)
- `gl.Window` and other dotted imported types resolve as the class, not a smashed hover string

## 0.2.2

- Ships with Sere **pre-0.1.2**
- Completions for `Callable`, `Function`, and `Class`

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
