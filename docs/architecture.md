# Architecture

Sere is split so the compiler and the editor share one analysis pipeline.
Codegen, linking, and the language server are thin clients of that pipeline.

## Process shape

`tools/sere/main.cpp` only parses argv and calls `Compiler::run`.

```
Compiler::run
 ├── --lsp            → runLanguageServer()
 ├── init/build/run/clean/shell
 └── compileInput()
      ├── Frontend::analyze()     lex, parse, import, prelude, macros, sema
      ├── IRGenerator::emit()     typed AST → LLVM module
      ├── runOptPipeline()
      └── clang/lld + sere_rt     unless --emit-llvm or --emit-asm
```

`--analyze` runs `Frontend` and prints JSON diagnostics. It never touches LLVM.
That is the same path the LSP uses for `textDocument/publishDiagnostics`.

## Frontend pipeline

`Frontend::analyze(path, text, stdlibDir)` in `lib/driver/Frontend.cpp`:

1. Reset diagnostics, AST, types, imports.
2. Wrap `text` in a `SourceManager`.
3. **Lex** the whole file (`Lexer::tokenizeAll`).
4. **Parse** a `Module` (`Parser::parseModule`).
5. **Load imports** by walking `import` / `from … import` statements. Search
   order is the origin directory, then `stdlib/`. See `ImportPath.h`.
6. **Load prelude** (`stdlib/prelude.sere`) into the user module, marked
   `fromPrelude()` so it is not emitted as user code.
7. Collect macro use sites for the LSP (`collectMacroUses`).
8. **Expand macros** on imported modules, then on the user module.
9. Build a `TypeContext` and typecheck imports (export bindings become module
   fields).
10. Typecheck the user module. `TypeChecker` fills `resolvedType()` on nodes
    and a `SemanticSymbol` table for the LSP.

If parse fails, `Frontend` still keeps an empty module so the LSP can report
diagnostics instead of crashing.

## Types

Types are interned. Pointer identity is equality (`TypeContext`). Kinds:

| `TypeKind` | Examples |
| --- | --- |
| `Primitive` | `i32`, `bool`, `str`, `void`, `never` |
| `Generic` | `Unique[T]`, `Ptr[i32]`, `list[str]`, `dict[K, V]` |
| `Record` | `class`, `struct`, `enum` |
| `Function` | `(i32, i32) -> i32` |
| `Alias` | `type Meters = i32` |
| `TypeParam` | `T` on a generic `def` / `class` |
| `Module` | imported package object |
| `Union` | `T \| U` |

`Unique`, `Shared`, and `Ptr` are pointer-like (`Type::isPointerLike`,
`pointeeType()`). Unary `*` loads the pointee; unary `&` produces `Ptr[T]`.

## Intrinsics vs stdlib vs extern

Three different ways a name becomes callable:

| Kind | How it exists | Example |
| --- | --- | --- |
| **Intrinsic** | `IntrinsicKind` + `TypeChecker::registerBuiltins` + `IRGenerator::emitIntrinsic` | `unique`, `alloc`, `len`, `print` |
| **Prelude / stdlib Sere** | Parsed from `stdlib/*.sere`, often `extern "C" "symbol"` | `io.read_line`, `gc.use` |
| **User / native** | `def` in the program, or `extern "C"` + `--link` | `examples/native_add.sere` |

Intrinsics are always in scope. They are not “magic syntax”; they are names the
type checker and codegen special-case. Prefer a stdlib `extern "C"` wrapper
when the operation is just a runtime call.

## Diagnostics

The compiler does not use C++ exceptions for user errors.
`DiagnosticEngine` collects `error` / `warn` / `note` with a `DiagnosticCode`
(`NameError`, `TypeError`, …). Messages print as `error[NameError]: …`.

`# type: ignore` and `# type[NameError]: ignore` are parsed by
`IgnoreDirective` from source comments. Unknown names in those comments are
`ValueError` and list the catalog.

## Codegen contract

`IRGenerator` lowers a **typed** AST. It should not re-run language rules.
If a construct is legal, sema has already set `resolvedType()`. If codegen
needs a new fact, put it on the AST or the `Type`, not in an LLVM pass.

Reachable functions are emitted; `main` is wrapped as a C `main` that converts
`argv` into `list[str]` when the user `main` takes it.

Linking always includes `sere_rt`. Extra native libs come from `--link`.
Importing `qt6` also pulls `sere_qt6` when that library was built.

## Invariants

- No circular CMake library deps.
- UI/editor code stays in `lib/lsp` and `editors/vscode`. Language rules stay
  in parse / sema / codegen.
- Heavy work stays off the UI thread in the editor by running in `sere --lsp`
  as a child process.
- New public API goes in `include/sere/…` with a file docstring.
