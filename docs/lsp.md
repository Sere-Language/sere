# Language server and editor

`sere --lsp` speaks JSON-RPC on stdin/stdout. The VS Code / Cursor client is
`editors/vscode`. Workspace setting `sere.compilerPath` should point at the
built `sere.exe`.

The language server prefers the CMake build-tree compiler
(`build/windows-clang-cl-relwithdebinfo/bin/sere.exe`) so it does not lock
`./bin/sere.exe`. CLI commands still prefer workspace `bin/`. After a rebuild,
**Sere: Refresh ./bin Compiler** (`sere refresh-bin`) copies the running
compiler, runtime, and stdlib into `./bin`. The client debounce is 80ms on
`didChange` so typing does not re-sema every keystroke.

After rebuilding `sere`, run **Sere: Restart Language Server** if hover still
looks stale.

## Capabilities

Handled in `lib/lsp/LanguageServer.cpp`:

- Diagnostics (exception codes such as `NameError`)
- Hover, completion, signature help, inlay hints
- Definition, references, rename, document highlight
- Document / workspace symbols, code lens, code actions
  (`# type[Code]: ignore` quick-fix on a diagnostic line)
- Semantic tokens, folding, formatting
- Import-path completion (`ImportCompletion.cpp`) and member completion
  (`MemberCompletion.cpp`) and member completion
  (`MemberCompletion.cpp`)

Each open buffer is analyzed with `Frontend::analyze`, same as the compiler.
Open `stdlib/*.sere` and `prelude.sere` buffers overlay the on-disk copies, so
a new prelude function is visible in other files as you type. Saving or
changing a watched stdlib file re-analyzes every open buffer.

## Semantic tokens

`collectSemanticTokens` in `lib/lsp/SemanticTokens.cpp` re-lexes the buffer and
classifies:

- Keywords (contiguous `TokenKind` keyword range)
- Builtin types (`Unique`, `Shared`, `Ptr`, primitives, collections)
- Compiler intrinsics (`print`, `alloc`, …); prelude names come from symbols
- Operators, including `*` and `&`
- Symbols from `TypeChecker::symbols()`

The legend is `kSemanticTokenTypeNames` in `SemanticTokens.h`. Keep the enum
and the name array in lockstep.

## Hover and completion

Hover uses `findNodeAt` then formats the node. Unary `*` / `&` show
`*p: i32` / `&x: Ptr[i32]` when the type checker filled `resolvedType()`.

Member completion after `.` resolves the receiver from the type checker
(`foo`, `gl.Window`, imported modules) and lists public fields and methods.
Dereference is **not** implicit: `p.field` is invalid when `p` is `Unique[T]`;
`(*p).field` uses the pointee. That matches C.

`sere.stdlibPath` (or **Sere: Set Stdlib Folder**) pins the folder that
contains `prelude.sere`. The language server uses that path for imports and
completion. Empty falls back to workspace `stdlib/` then the compiler's stdlib.

Completion triggers on `.`, space (for `import `), `"`, `@`, and `!`. `:` does
not open the suggest widget, so `def main() -> i32:` then Enter starts a new
indented line instead of inserting a snippet.

## Grammar

TextMate grammar: `editors/vscode/syntaxes/sere.tmLanguage.json`.
Update `#operator`, `#type`, and `#function` when you add syntax the highlighter
should see before semantic tokens arrive.

The client in `editors/vscode/extension.js` registers every server capability
(hover, completion, definition, type definition, implementation, references,
rename, highlight, symbols, signature help, inlay hints, folding, formatting,
code actions, code lens, and semantic tokens). Package with
`.\scripts\package-vsix.ps1` (writes `dist/sere-0.2.1.vsix`).

Client commands and `sere.compilerPath` live in `editors/vscode/package.json`.

## Tests

- `tests/macro_lsp.cpp` — macro symbols/uses and pointer hover/tokens
- `tests/lsp_semantic.cpp` — broader semantic-token coverage
- `tests/member_completion.cpp` — local class, sibling import, and stdlib members

Prefer `Frontend` + `findNodeAt` / `collectSemanticTokens` over spinning up a
JSON-RPC session in unit tests.
