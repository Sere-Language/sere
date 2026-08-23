# Sere

Sere is a compiled language with a Python-superset frontend and an LLVM 22 backend.

(If you are here from reddit feel free to DM! I do need help)

Programs are statically type `print` is a compiler intrinsic (also named in the prelude). It is not a statement.

```python
def main() -> i32:
    owned: Unique[i32] = unique[i32](42)
    n: i32 = 0
    while n < 3:
        if n == 1:
            n = n + 2
        else:
            n = n + 1
    print("hello, sere")
    return 0
```

A typed binding may omit an initializer (`ptr: Unique[i32]` default-initializes). `=` always requires an expression.

## Toolchain

| Component | Version / location |
| --- | --- |
| CMake | 3.28+ (4.3 is fine) |
| Ninja | 1.11+ |
| MSVC | Visual Studio 2022 Build Tools, x64 |
| LLVM | **22.1.8** official `clang+llvm` Windows MSVC archive (clang, lld, headers, libs) |

LLVM is installed to `%LOCALAPPDATA%\sere\toolchains\llvm-22.1.8` so it does not live inside OneDrive.

## Bootstrap

From the repository root in PowerShell:

```powershell
.\scripts\bootstrap.ps1
. .\scripts\env.ps1
cmake --preset windows-clang-cl-relwithdebinfo
cmake --build --preset windows-clang-cl-relwithdebinfo
ctest --preset windows-clang-cl-relwithdebinfo --output-on-failure
```

`env.ps1` must be dot-sourced so MSVC `vcvars64` and `SERE_LLVM_DIR` stay in the current session.

## Projects

```powershell
.\bin\sere.exe init myapp
.\myapp\scripts\activate
sere build
sere run
deactivate
```

`scripts/activate` opens a nested shell with the project compiler on `PATH`. Normal shell commands keep working. `sere build` compiles `src/main.sere`; `sere run` builds and executes `bin/<name>.exe`.

## Compile a program

```powershell
.\bin\sere.exe --emit-llvm examples\hello.sere -o hello.ll
.\bin\sere.exe examples\hello.sere -o hello.exe
.\hello.exe
```

`sere` finds the pinned LLVM `clang`/`lld` automatically. You only need `scripts/env.ps1` when configuring or compiling the compiler itself.

`sere --analyze file.sere` prints JSON diagnostics. `sere --lsp` speaks Language Server Protocol on stdin/stdout.

Diagnostics are labeled with exception names, for example `error[NameError]: unknown name 'foo'`. Suppress them with comments:

```python
# type[NameError]: ignore
def main() -> i32:
    n: i32 = "nope"          # TypeError still reported
    return missing           # NameError ignored for the whole file
```

```python
def main() -> i32:
    return missing  # type: ignore
```

`# type: ignore` hides every diagnostic on that line. A comment-only `# type: ignore` on the previous line applies to the next statement. At the top of a file, `# type[TypeError]: ignore` or `# type[Exception]: ignore` applies to the whole file. `# type: ignore[NameError]` is accepted too.

| Exception | Meaning |
| --- | --- |
| `Exception` | Ignore every diagnostic (file or line) |
| `SyntaxError` | Parse errors |
| `IndentationError` | Inconsistent indentation |
| `NameError` | Unknown names, types, functions, macros, modules |
| `AttributeError` | Unknown fields/methods |
| `TypeError` | Type mismatches, invalid operands, wrong arguments |
| `IndexError` | Invalid indexing or slicing |
| `ImportError` | Missing modules or prelude |
| `ValueError` | Values that cannot be inferred or are invalid |
| `AssertionError` | Invalid `assert` |
| `PermissionError` | Private field access |
| `RuntimeError` | Control-flow and compiler internals |
| `RecursionError` | Macro expansion limit |
| `NotImplementedError` | Unsupported or unexpanded constructs |

Unknown names in `# type[Bogus]: ignore` report `ValueError` and list this catalog in the diagnostic help.

## Language

The full reference is [`docs/language.md`](docs/language.md) (syntax as the compiler implements it, not a roadmap).

| Kind | Examples |
| --- | --- |
| Primitives | `void`, `bool`, `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `str`, `byte`, `regex` |
| Pointers | `Unique[T]`, `Shared[T]`, `Ptr[T]` |
| Collections | `list[T]`, `array[T]`, `dict[K, V]`, `str` indexing/slicing |
| User types | `class` (identity), `struct` (copy-by-value), `enum Color:` with `Color.Green` |
| Control | `if` / `elif` / `else`, `while`, `for`, `match` / `case`, `try` / `except` / `raise` |
| Macros | `macro twice(x): quote:`, `name!(...)`, indent `html:` raw bodies, `match` token-tree arms |
| Arithmetic | `+ - * / // % **`, bitwise `& | ^ ~ << >>`, comparisons, `in` |
| Module | `__name__`, `__file__`, `__package__`, `__doc__`, `__debug__`, `__sere_version__` |

Memory intrinsics: `unique[T](value)`, `shared[T](value)`, `alloc[T]()`, `load`, `store`, `free`, `len`. `print` and `str` are intrinsics too.

`alloc` / `free` go through the installed collector (`import gc`). Builtins are `none`, `mark_sweep`, and `arena`. Custom collectors implement `SereGcVTable` in C, call `sere_gc_install` from `sere_mod_init`, and link with `--link`. Arenas and pools are in `import heap`.

## Editor IntelliSense

The workspace extension in `editors/vscode` gives `.sere` files syntax highlighting, diagnostics (with exception codes such as `NameError`), markdown hover, completion, rename, semantic tokens, folding, and go-to-definition. It launches `sere --lsp`. `# type: ignore` and `# type[NameError]: ignore` suppress editor diagnostics.

Package a VSIX:

```powershell
.\scripts\package-vsix.ps1
```

That writes `editors/vscode/sere-0.1.1.vsix`. In Cursor / VS Code: **Extensions → … → Install from VSIX…** and choose that file. Reload the window. `sere.compilerPath` in `.vscode/settings.json` already points at the RelWithDebInfo `sere.exe`.

## Windows installer

Package a full toolchain installer (compiler, LLVM 22.1.8, stdlib, runtime, optional Qt6, editor VSIX):

```powershell
.\scripts\bootstrap-innosetup.ps1
sere --build-installer
```

That writes `dist/Sere-<version>-setup.exe`. The wizard can add `sere` to PATH, install C++ build tools if they are missing, associate `.sere` files, and install the VS Code / Cursor extension (`sere --lsp`). After install, a new terminal can compile `.sere` programs without this repository. See [docs/packaging.md](docs/packaging.md).

## Layout

```
include/sere/   public compiler headers (ast, types, sema, macro, codegen, lsp)
lib/            lexer, parser, macros, type checker, LLVM codegen, driver, LSP
runtime/        heap, collectors, shared boxes, lists, print
stdlib/         prelude plus io, fs, gc, heap, random, hash, sys, and more
tools/sere/     sere executable (CLI, compiler, LSP, installer driver)
tests/          LLVM, lexer, parser, sema, macros, example emit
examples/       hello, structs, enums, strings, macros, dunders, errors, introspect
bin/            local sere.exe after a build (gitignored convenience copy)
editors/vscode  language grammar, LSP client, and .vsix
scripts/        bootstrap, environment activation, Inno Setup helper
cmake/          LLVM discovery and warning policy
docs/           language reference plus compiler internals handbook
packaging/      Windows installer templates
```

## Documentation

- **Language:** [`docs/language.md`](docs/language.md) — types, syntax, macros, stdlib surface, as implemented.
- **Compiler internals:** [`docs/README.md`](docs/README.md) — pipeline, libraries, how to add a keyword or module.
- **Patches:** [`CONTRIBUTING.md`](CONTRIBUTING.md)
