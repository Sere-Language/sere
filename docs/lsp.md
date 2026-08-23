# Language server and editor

`sere --lsp` speaks JSON-RPC on stdin/stdout. The VS Code / Cursor client is
`editors/vscode`. Workspace setting `sere.compilerPath` should point at the
built `sere.exe`.

The language server prefers the CMake build-tree compiler
(`build/windows-clang-cl-relwithdebinfo/bin/sere.exe`) so it does not lock
`./bin/sere.exe`. CLI commands still prefer workspace `bin/`. After a rebuild,
**Sere: Refresh ./bin Compiler** (`sere refresh-bin`) copies the running
compiler, runtime, and stdlib into `./bin`. The client debounce is 80ms on
`didChange` so typing does not re-sema every keystroke. Stdlib, `sere.toml`,
and project `src/` / `libs/` updates are watched separately (120ms) and
re-analyze every open buffer. Open editors overlay disk: unsaved stdlib or
imported module edits apply immediately.

After rebuilding `sere`, run **Sere: Restart Language Server** if hover still
looks stale. Changing `sere.compilerPath` or `sere.stdlibPath` restarts the
server automatically. Stdlib and language-context file edits do not require a
restart.

## Capabilities

Handled in `lib/lsp/LanguageServer.cpp`:

- Diagnostics (exception codes such as `NameError`)
- Hover, completion, signature help, inlay hints
- Definition, references, rename, document highlight
- Document / workspace symbols, code lens, code actions
  (`# type[Code]: ignore` quick-fix on a diagnostic line)
- Semantic tokens, folding, formatting
- Import-path completion (`ImportCompletion.cpp`)
- `workspace/didChangeWatchedFiles`, `workspace/didChangeConfiguration`,
  and `workspace/didChangeWorkspaceFolders` to pick up stdlib and project
  context updates

Each open buffer is analyzed with `Frontend::analyze`, same as the compiler.
Open documents are passed as a `SourceOverlay` so prelude and imports see the
latest buffers instead of only files on disk. Changing a stdlib module,
`sere.toml`, or an imported project file invalidates dependents and refreshes
diagnostics, semantic tokens, inlay hints, and code lens.

## Semantic tokens

`collectSemanticTokens` in `lib/lsp/SemanticTokens.cpp` re-lexes the buffer and
classifies:

- Keywords (contiguous `TokenKind` keyword range)
- Builtin types (`Unique`, `Shared`, `Ptr`, primitives, collections)
- Builtin functions / intrinsics
- Operators, including `*` and `&`
- Symbols from `TypeChecker::symbols()`

The legend is `kSemanticTokenTypeNames` in `SemanticTokens.h`. Keep the enum
and the name array in lockstep.

## Hover and completion

Hover uses `findNodeAt` then formats the node. Unary `*` / `&` show
`*p: i32` / `&x: Ptr[i32]` when the type checker filled `resolvedType()`.

Member completion after `.` uses the record type of the object. Dereference
is **not** implicit: `p.field` is invalid when `p` is `Unique[T]`; `(*p).field`
uses the pointee. That matches C.

Completion triggers on `.`, `"`, `@`, and `!` only. `:` and space do not open
the suggest widget, so `def main() -> i32:` then Enter starts a new indented
line instead of inserting a snippet.

## Grammar

TextMate grammar: `editors/vscode/syntaxes/sere.tmLanguage.json`.
Update `#operator`, `#type`, and `#function` when you add syntax the highlighter
should see before semantic tokens arrive.

The client in `editors/vscode/extension.js` registers every server capability
(hover, completion, definition, type definition, implementation, references,
rename, highlight, symbols, signature help, inlay hints, folding, formatting,
code actions, code lens, and semantic tokens). Package with
`.\scripts\package-vsix.ps1` (writes `editors/vscode/sere-0.2.0.vsix` and `dist/sere-0.2.0.vsix`).

Client commands and `sere.compilerPath` live in `editors/vscode/package.json`.

## Tests

- `tests/macro_lsp.cpp` — macro symbols/uses and pointer hover/tokens
- `tests/lsp_semantic.cpp` — broader semantic-token coverage
- `tests/lsp_context.cpp` — stdlib overlays and language-context stamps

Prefer `Frontend` + `findNodeAt` / `collectSemanticTokens` over spinning up a
JSON-RPC session in unit tests.
