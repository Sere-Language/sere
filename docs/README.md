# Sere compiler handbook

This folder holds two kinds of docs:

- **[language.md](language.md)** — the language as the compiler implements it
  (syntax, types, macros, stdlib). Start there to *write Sere*.
- The rest of this handbook — how source becomes an executable, where each
  subsystem lives, and how to extend the compiler without guessing.

Sere is a **statically typed Python-superset** with an **LLVM 22** backend.
The compiler is `sere` (`tools/sere`). The same binary also speaks LSP
(`sere --lsp`) and drives project commands
(`sere init|init-lib|build|pack|run|clean|shell`).

## Read this first

| If you want to… | Open |
| --- | --- |
| Create and run your first application | [getting-started.md](getting-started.md) |
| Configure projects and distribute libraries | [projects.md](projects.md) |
| Understand `Any` and collection inference | [gradual-typing.md](gradual-typing.md) |
| Work with numeric lists and matrices | [arrays.md](arrays.md) |
| Learn or look up the language | **[language.md](language.md)** |
| See the whole pipeline | [architecture.md](architecture.md) |
| Add a keyword, type, operator, intrinsic, stdlib module, native lib, GC, or LSP feature | **[extending.md](extending.md)** |
| Understand lex / parse / AST / macros / types | [frontend.md](frontend.md) |
| Understand LLVM lowering, linking, and the C runtime | [backend.md](backend.md) |
| Work on hover, completion, or highlighting | [lsp.md](lsp.md) |
| Add or change `stdlib/*.sere` | [stdlib.md](stdlib.md) |
| Add a test or example | [testing.md](testing.md) |
| Package a zip install or the Windows wizard | [packaging.md](packaging.md) |
| Send a patch | [../CONTRIBUTING.md](../CONTRIBUTING.md) |

## Mental model

```
.sere source
    │
    ▼
 Lexer  →  Parser  →  imports + prelude  →  macros  →  TypeChecker
    │
    ▼
 IRGenerator  →  opt pipeline  →  .ll
    │
    ▼
 clang + lld + sere_rt  →  .exe
```

`Frontend::analyze` is the shared front half. The compiler, the language server,
and most unit tests all call it. If a change is not visible to `Frontend`, the
editor will not see it either.

## Library map

CMake builds one static library per subdirectory of `lib/`. Dependencies only
point **down** this list. Do not add reverse or circular links.

| Library | Sources | Responsibility |
| --- | --- | --- |
| `sere_diag` | `lib/diag` | Diagnostics, `# type: ignore`, exception codes |
| `sere_source` | `lib/source` | File text and source locations |
| `sere_lex` | `lib/lex` | Tokens and keywords |
| `sere_types` | `lib/types` | Interned types and intrinsic names |
| `sere_ast` | `lib/ast` | Syntax tree and hover/lookup helpers |
| `sere_parse` | `lib/parse` | Recursive-descent parser |
| `sere_macro` | `lib/macro` | Quote, match, and expansion |
| `sere_sema` | `lib/sema` | Type checker and semantic symbols |
| `sere_codegen` | `lib/codegen` | LLVM IR and the opt pipeline |
| `sere_driver` | `lib/driver`, `lib/lsp` | CLI, projects, frontend, LSP |
| `sere_runtime` | `runtime/` | C ABI linked into user programs |

Public headers live under `include/sere/…` and match those folders. Every `.h`
and `.cpp` starts with a one-line `@file` docstring.

## Command line

```
sere <command> [options]
sere [options] <file.sere>
```

| Flag / command | Effect |
| --- | --- |
| `init`, `build`, `run`, `clean`, `shell` | Project workflow |
| `--build-installer` | Package a Windows setup exe (compiler, LLVM, stdlib, editor) |
| `--emit-llvm` | Stop after writing `.ll` |
| `--emit-asm`, `-S` | Stop after writing native assembly (`.s`) |
| `--dump-tokens` | Print lexer output |
| `--analyze` | JSON diagnostics, no codegen |
| `--lsp` | Language server on stdin/stdout |
| `--link <lib>` | Extra native library at link time |
| `--opt=O0..O3,Os,Oz` | LLVM optimization level |
| `--passes=<pipeline>` | Custom LLVM pass pipeline |
| `-o <path>` | Output path |

Parsing lives in `lib/driver/Options.cpp`. Orchestration lives in
`lib/driver/Compiler.cpp`.
