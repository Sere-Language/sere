# <img src="https://github.com/Sere-Language/sere/blob/main/Icon.PNG" width=64/> Sere
Sere is a compiled **typed Python superset** with an LLVM 22 backend. It is not
CPython: the CPython standard library, `async`/`yield`, `*args`, and capturing
lambdas are out of scope. Unsupported constructs diagnose (often
`NotImplementedError`) instead of generating silent wrong code.

Sere now has a website! Visit 
https://sere-lang.vercel.app/

```python
def greet(name):
    print(f"hello {name}")

def main() -> i32:
    xs = [1, 2, 3]
    xs.append(4)
    greet("sere")
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
cd myapp
. .\scripts\activate.ps1
sere build
sere run
deactivate
```

Dot-source `scripts/activate.ps1` so this terminal stays put. `deactivate` restores `PATH` and the prompt; it does not close the window. Running `activate.ps1` without the leading `.` starts a clean nested `sere shell` instead.

Put the compiler on `PATH` from the repo or a project:

```powershell
.\bin\sere-path.ps1              # this session
.\bin\sere-path.ps1 -Persistent  # this session + your user PATH
```

`sere build` compiles `src/main.sere`; `sere run` builds and executes `bin/<name>.exe`.
`sere refresh-bin` copies this compiler, runtime, and stdlib into `./bin` even
when the previous `sere.exe` is locked (it is renamed to `sere.exe.old`).
`sere --update` (or `sere update`) copies **this** compiler into
`%LOCALAPPDATA%\\Programs\\Sere` if the system install is a different version
or binary. Run it from the `sere.exe` you want on PATH (for example
`.\releases\dev-0.1.4\bin\sere.exe update`). If you run it from a project with
`sere.toml`, it also refreshes that project's venv **without changing your
source**.

## Libraries

Create a drop-in library, pack it into one `.slib` file, then copy that file
into another project's `libs/` folder:

```powershell
sere init-lib mathlib
cd mathlib
sere pack
copy dist\mathlib.slib ..\myapp\libs\
```

```python
import mathlib

def main() -> i32:
    return mathlib.add(2, 3)
```

`sere --init-lib mathlib` and `sere init mathlib --lib` do the same as
`init-lib`. `sere pack file.sere -o mathlib.slib` packs a single module
without a project. `sere build` in a `kind = "lib"` project also writes the
`.slib` that contains only the entry, the local modules it imports, and
compiled native objects. Unused files next to the library are not packed.
Native C/C++ under `libs/native` (when `native = true`) or loose `.c` / `.cpp`
next to a folder library is compiled and stored in that same file. If you
prefer not to pack, a folder `libs/mylib/` with `lib.sere` or `mylib.sere`
plus native sources acts as the library.

## Compile a program

```powershell
.\bin\sere.exe --emit-llvm examples\hello.sere -o hello.ll
.\bin\sere.exe --emit-asm examples\hello.sere -o hello.s
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

That writes `dist/sere-0.2.6.vsix`. In Cursor / VS Code: **Extensions → … → Install from VSIX…** and choose that file. Reload the window. The language server uses the RelWithDebInfo `sere.exe` under `build/` so it does not lock `./bin/sere.exe`.

## Windows installer

A **manual / zip** release is built into `dist/`:

```powershell
.\scripts\package-vsix.ps1
.\releases\stage.ps1                    # dist/Sere-dev-0.1.4 + zip
.\releases\dev-0.1.4\install.ps1        # %LOCALAPPDATA%\Programs\Sere and PATH
```

The Inno Setup wizard is a later option (`sere --build-installer` →
`dist/Sere-<version>-setup.exe`). See [docs/packaging.md](docs/packaging.md).

## Layout

```
include/sere/   public compiler headers (ast, types, sema, macro, codegen, lsp)
lib/            lexer, parser, macros, type checker, LLVM codegen, driver, LSP
runtime/        heap, collectors, shared boxes, lists, print
stdlib/         prelude plus io, fs, gc, heap, random, hash, sys, and more
tools/sere/     sere executable (CLI, compiler, LSP, installer driver)
tests/          LLVM, lexer, parser, sema, macros, example emit
examples/       hello, structs, enums, strings, macros, dunders, errors, gl, introspect
build/          CMake compile tree (gitignored)
bin/            local sere.exe after a build, plus sere-path PATH helpers
dist/           vsix, zip, and installer outputs (gitignored)
editors/vscode  language grammar and LSP client
scripts/        bootstrap, sere-path, project activate templates, Inno Setup helper
cmake/          LLVM discovery and warning policy
docs/           language reference plus compiler internals handbook
packaging/      Windows installer templates
releases/       stage.ps1 plus the historical pre-0.1.0 tree
```

## Documentation

- **Language:** [`docs/language.md`](docs/language.md) — types, syntax, macros, stdlib surface, as implemented.
- **Compiler internals:** [`docs/README.md`](docs/README.md) — pipeline, libraries, how to add a keyword or module.
- **Patches:** [`CONTRIBUTING.md`](CONTRIBUTING.md)
