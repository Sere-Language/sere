# <img src="https://github.com/Sere-Language/sere/blob/main/icon.png" width=64/> Sere
<p align="center">
  <a href="https://github.com/Sere-Language/Sere/releases">
    <img src="https://img.shields.io/github/v/release/Sere-Language/Sere?include_prereleases&style=flat-square&label=release" alt="GitHub Release">
  </a>
  <a href="https://github.com/Sere-Language/Sere/stargazers">
    <img src="https://img.shields.io/github/stars/Sere-Language/Sere?style=flat-square" alt="GitHub Stars">
  </a>
  <a href="https://github.com/Sere-Language/Sere/network/members">
    <img src="https://img.shields.io/github/forks/Sere-Language/Sere?style=flat-square" alt="GitHub Forks">
  </a>
  <a href="https://github.com/Sere-Language/Sere/issues">
    <img src="https://img.shields.io/github/issues/Sere-Language/Sere?style=flat-square" alt="GitHub Issues">
  </a>
  <a href="https://github.com/Sere-Language/Sere/commits">
    <img src="https://img.shields.io/github/last-commit/Sere-Language/Sere?style=flat-square" alt="Last Commit">
  </a>
  <img src="https://img.shields.io/github/repo-size/Sere-Language/Sere?style=flat-square" alt="Repository Size">
  <a href="https://discord.gg/TRJ9nC3Bhb">
    <img src="https://img.shields.io/badge/discord-join%20us-5865F2?style=flat-square&logo=discord&logoColor=white" alt="Discord">
  </a>
</p>

Sere is a statically typed language that compiles to native code through LLVM 22. Its syntax is deliberately Python-shaped (significant indentation, `def`, f-strings, comprehensions, `class`, `match`), so it reads like something you already know. The resemblance stops at the surface. There is no interpreter, no virtual machine, and no CPython underneath; a Sere program becomes a real executable, or a `.slib` library you can drop into another project.

One misunderstanding is worth heading off early.

> **Sere is not a Python superset.** A superset would have to accept every valid Python program, and Sere does not. It is statically typed, it wants types at its interfaces, and it leaves out the parts of Python that do not survive ahead-of-time compilation. It borrows Python's look because that look is familiar and pleasant, not because this is a Python dialect.

The same is true of CPython specifically: its standard library is not available, and nothing that depends on the interpreter's object model comes along for the ride. Roughly, here is how the two relate.

| You might expect from Python | What Sere actually does |
| --- | --- |
| Indentation, `def`, `class`, `for`, f-strings | Kept, with much the same shape |
| Types inferred from annotations, or `Any` everywhere | Gradual typing: locals infer, interfaces declare, `Any` is explicit |
| `list`, `dict`, comprehensions, `match` | Kept, and statically typed (`list[i32]`, `dict[str, f64]`) |
| CPython standard library | Not available; Sere ships its own modules |
| `yield` and generator functions | Not implemented; reported as `NotImplementedError` |
| `global` / `nonlocal` | Not implemented (a nested `def` can close over locals) |
| Keyword-only parameters after `*args` | Not implemented (`*args` and `**kwargs` themselves work) |
| `print` as a statement | `print` is an intrinsic function |
| Runtime exceptions | Compile-time diagnostics named after exceptions: `NameError`, `TypeError`, and so on |

Nothing in that list is silently miscompiled. When the front end accepts something the back end cannot lower yet, you get a diagnostic with a source location instead of code that behaves unlike what you wrote.

The shortest useful program:

```sere
def greet(name: str) -> void:
    print(f"hello {name}")

def main() -> i32:
    xs = [1, 2, 3]
    xs.append(4)
    greet("sere")
    return 0
```

Build and run it:

```powershell
sere hello.sere -o hello.exe
.\hello.exe
```

A typed binding may omit its initializer, in which case the slot is default-initialized:

```sere
count: i32 = 0            # explicit
ptr: Unique[i32]          # default-initialized
```

`=` always needs an expression on the right, so a bare `n: i32 =` is an error.

Project home: <https://sere-lang.com>. Releases: <https://github.com/Sere-Language/Sere/releases>. Questions and show-and-tell: [Discord](https://discord.gg/TRJ9nC3Bhb).

## Install a release

Building the compiler from source takes a while, so start with a release if you only want to write Sere. Every tagged release publishes a portable ZIP and an offline, per-user Windows setup EXE. LLVM and Windows linking support are bundled; recipients need neither admin rights nor a separate toolchain install. Release artifacts and validation steps are described in [releases/README.md](releases/README.md).

Once installed, the compiler can keep itself current. `sere update` (also `sere --update`) checks GitHub for the newest Windows x64 portable asset, verifies its SHA-256 digest, and installs it into `%LOCALAPPDATA%\Programs\Sere`. Prereleases count as upgrades, an asset replaced on an otherwise identical release counts as an upgrade, and a newer development build is never downgraded. The previous installation stays next to the new one as a backup and is restored if anything fails. Automatic GitHub installation is Windows x64 only; on other platforms, build from source.

Confirm what you have before filing a bug:

```powershell
sere --version
sere --print-env
```

`--print-env` prints the compiler, standard-library, and toolchain paths as JSON, which is the quickest way to see when a terminal and an editor are using different installations.

## Toolchain

The compiler is C++20 and needs a modern LLVM. These are the pinned versions.

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

## Linux source build

Use Bash on Linux x86_64 (Ubuntu 24.04 is the dependency example below).
You need CMake 3.28+, Ninja 1.11+, a system C/C++ development toolchain,
and the LLVM 22.1.8 development archive, including clang and lld.

```bash
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build curl ca-certificates xz-utils \
  zlib1g-dev libzstd-dev libxml2-dev libffi-dev libedit-dev libncurses-dev

# From the repository root, download LLVM once and build:
bash scripts/bootstrap-llvm.sh
bash scripts/build.sh

# Build and run the test suite:
bash scripts/build.sh --test
```

LLVM is downloaded from the [official LLVM release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.8)
and installed under `${XDG_DATA_HOME:-$HOME/.local/share}/sere/toolchains/llvm-22.1.8`.
Set `SERE_TOOLCHAIN_ROOT` to change that parent directory for both bootstrap and build.
Alternatively, set `SERE_LLVM_DIR` to an existing LLVM development installation;
then skip the download. Bootstrap only installs LLVM, not system packages.
On other distributions, install equivalent development packages and the required
CMake/Ninja versions using your package manager. Qt6 Widgets development packages
are optional; without them the Qt runtime is built as a stub.

For manual CMake use, source the environment in each new Bash session:

```bash
source scripts/env.sh
cmake --preset linux-clang-relwithdebinfo
cmake --build --preset linux-clang-relwithdebinfo
ctest --preset linux-clang-relwithdebinfo --output-on-failure
./build/linux-clang-relwithdebinfo/bin/sere --version
```

The build copies the compiler, runtime, and standard library into
`build/linux-clang-relwithdebinfo/bin/` and `bin/`. Keep these files together.
Use a separate build directory from Windows; the Linux preset does this automatically.
For machines with limited RAM, run `CMAKE_BUILD_PARALLEL_LEVEL=2 bash scripts/build.sh`.
Additional configure options are accepted, for example
`bash scripts/build.sh -DBUILD_TESTING=OFF` (omit `--test` in that case).

## Projects

A project is a directory with a `sere.toml`, a `src/` tree, a `libs/` folder for dependencies, and a `venv/` holding a copy of the standard library. `sere init` writes all of it, then the activation script puts that project's environment in front of the one you already have:

```powershell
.\bin\sere.exe init myapp
cd myapp
. .\scripts\activate.ps1
sere build
sere run
deactivate
```

Dot-source `scripts/activate.ps1` so the new environment lands in the terminal you already have. `deactivate` puts the old `PATH` and prompt back; it does not close the window. Running `activate.ps1` without the leading dot opens a clean nested `sere shell` instead, which is handy but easy to get lost in.

`sere build` compiles `[paths].entry`, which is `src/main.sere` unless you point it elsewhere, and `sere run` builds and then executes `bin/<name>.exe`. `[build].output` selects a different path and `[build].opt` picks an optimization level. Arguments after `--` go to your program:

```powershell
sere run -- first second
```

Put the compiler itself on `PATH` from the repository or from any project:

```powershell
.\bin\sere-path.ps1              # this session only
.\bin\sere-path.ps1 -Persistent  # this session and your user PATH
```

### Project commands

| Command | What it does |
| --- | --- |
| `sere init <name>` | Create an application project (`src`, `libs`, `bin`, `venv`, `scripts`) |
| `sere init-lib <name>` | Create a library project (`src`, `libs`, `dist`) |
| `sere build` | Compile the configured entry to an executable; packs a `kind = "lib"` project to `.slib` |
| `sere pack [file.sere]` | Pack a single module into a drop-in `.slib` |
| `sere run [-- <args>]` | Build, then run the project executable |
| `sere clean` | Remove `bin/`, `dist/`, the native build directory, and the extracted library cache |
| `sere shell [--host <shell>]` | Enter a nested project shell (`powershell`, `cmd`, or `bash`) |
| `sere refresh-bin` | Copy this compiler, runtime, and stdlib into `./bin` |
| `sere update` | Install the newest GitHub portable release, or refresh a replaced asset |
| `sere update-local` | Copy this compiler into the system installation, then refresh the project environment |

`sere refresh-bin` survives a locked `sere.exe` by renaming the old file to `sere.exe.old` before copying, so you can refresh while the language server is running.

### `sere.toml`

```toml
[project]
name = "hello"
version = "0.1.0"
kind = "app"                 # app or lib

[toolchain]
sere = "pre-0.1.9"

[paths]
src = "src"
entry = "src/main.sere"
libs = "libs"
stdlib = "venv/stdlib"

[build]
output = "bin/hello.exe"     # omit to use the platform default
opt = "O0"                   # O0, O1, O2, O3, Os, Oz
native = false               # build libs/native
```

Paths resolve relative to `sere.toml`. A library uses `kind = "lib"`, defaults to `src/lib.sere`, and writes `dist/<name>.slib`. Unknown keys and custom tables are ignored rather than rejected, so you can keep your own metadata in the same file, and a `[tool.*]` table never overrides `[project].name`. Older manifests that keep the same keys at the root still work. `sere update` rewrites the toolchain version without disturbing anything else you added. Full details live in [docs/projects.md](docs/projects.md).

## Libraries

A library is a project with `kind = "lib"` whose entry is `src/lib.sere`. Building it produces a single `.slib` file containing the entry module, the local modules it imports, and any compiled native objects. Drop it into another project's `libs/` folder and import it:

```powershell
sere init-lib mathlib
cd mathlib
sere pack
copy dist\mathlib.slib ..\myapp\libs\
```

```sere
import mathlib

def main() -> i32:
    return mathlib.add(2, 3)
```

There are several ways in. `sere --init-lib mathlib` and `sere init mathlib --lib` are the same as `init-lib`; `sere pack file.sere -o mathlib.slib` packs one file with no project at all; and `sere build` inside a `kind = "lib"` project packs the library as a side effect. Only the entry and the modules it actually imports are included, so stray files sitting next to the source stay out.

Native code is welcome too. C or C++ under `libs/native` (with `native = true`) or loose `.c` / `.cpp` files beside a folder library are compiled into the same `.slib`. If you would rather not pack at all, a plain folder such as `libs/mylib/` containing `lib.sere` or `mylib.sere` acts as the library just as well.

## Registry

Libraries can also travel through the Sere package registry at <https://sere-lang.com>. Publish tokens are issued at <https://sere-lang.com/developers>.

```powershell
sere login <token>       # store a token for this user
sere login               # report whether you are signed in
sere logout              # forget the stored token
sere publish             # pack this library project and upload it
sere add mathlib@0.1.9   # download a package into ./libs
sere add mathlib --force # replace a package already in libs/
sere publish --dry-run   # describe the work without doing it
```

Credentials live in a per-user file (`%LOCALAPPDATA%\sere\credentials.toml` on Windows, `$XDG_CONFIG_HOME/sere/credentials.toml` elsewhere), and `sere logout` removes it. In CI, `SERE_TOKEN` supplies the token without writing anything to disk, and `SERE_REGISTRY_URL` points the client at a different registry for testing or self-hosting. Downloads are checked against the registry's published SHA-256 checksum before anything is written into `libs/`.

## Compiling a single file

You do not need a project to compile a file.

```powershell
sere examples\hello.sere -o hello.exe      # link an executable
sere --emit-llvm examples\hello.sere -o hello.ll
sere --emit-asm examples\hello.sere -o hello.s
sere --analyze examples\hello.sere         # JSON diagnostics, no codegen
```

`sere` locates the pinned LLVM `clang` and `lld` on its own, so `scripts/env.ps1` is only needed when you are building the compiler itself. The remaining flags are for when you want to see what the front end or the optimizer did.

| Flag | Effect |
| --- | --- |
| `--emit-llvm` | Write LLVM IR instead of linking |
| `--emit-asm`, `-S` | Write native assembly instead of linking |
| `--dump-tokens` | Print lexer tokens |
| `--dump-ast` | Print the parsed AST as JSON and stop |
| `--dump-symbols` | Print the semantic symbol table as JSON and stop |
| `--dump-llvm-ir-raw` | Write pre-optimization IR to `sere-raw-before-pipeline.ll` |
| `--analyze` | Print JSON diagnostics and stop |
| `--opt=<level>` | `O0`, `O1`, `O2`, `O3`, `Os`, `Oz` |
| `--passes=<pipeline>` | Custom LLVM pass pipeline (PassBuilder syntax) |
| `--link <lib>` | Link an extra native C/C++ library into the program |
| `--color=<mode>`, `--no-color` | `auto`, `always`, or `never` |
| `--lsp` | Run the language server on stdin/stdout |

## Diagnostics

A compile error names the exception it corresponds to, which makes the message easier to search for and easier to silence:

```
error[NameError]: unknown name 'foo'
```

Suppression is local and deliberate, so it never hides more than you intended:

```sere
# type[NameError]: ignore
def main() -> i32:
    n: i32 = "nope"          # TypeError is still reported
    return missing           # NameError is ignored for the whole file
```

```sere
def main() -> i32:
    return missing  # type: ignore
```

`# type: ignore` hides every diagnostic on its line. A comment-only `# type: ignore` on the line above applies to the next statement, and `# type[TypeError]: ignore` or `# type[Exception]: ignore` at the top of a file applies to the whole file. `# type: ignore[NameError]` is accepted as well. Name an exception that does not exist and you get a `ValueError` listing the valid ones.

| Exception | Covers |
| --- | --- |
| `Exception` | Every diagnostic (whole file, or one line) |
| `SyntaxError` | Parse errors |
| `IndentationError` | Inconsistent indentation |
| `NameError` | Unknown names, types, functions, macros, modules |
| `AttributeError` | Unknown fields and methods |
| `TypeError` | Type mismatches, invalid operands, wrong arguments |
| `IndexError` | Invalid indexing or slicing |
| `ImportError` | Missing modules or prelude |
| `ValueError` | Values that cannot be inferred, or are invalid |
| `AssertionError` | Invalid `assert` |
| `PermissionError` | Private field access |
| `RuntimeError` | Control-flow and compiler internals |
| `RecursionError` | Macro expansion limit |
| `NotImplementedError` | Unsupported or unexpanded constructs |

## The language

[`docs/language.md`](docs/language.md) is the full reference, written to match what the compiler actually accepts rather than what is planned. This is the quick tour.

| Category | What you get |
| --- | --- |
| Primitives | `void`, `bool`, `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `str`, `byte`, `regex` |
| Pointers | `Unique[T]`, `Shared[T]`, `Ptr[T]` |
| Collections | `list[T]`, `array[T]`, `dict[K, V]`, string indexing and slicing |
| User types | `class` (identity), `struct` (copied by value), `enum Color:` with `Color.Green` |
| Control flow | `if` / `elif` / `else`, `while`, `for`, `match` / `case`, `try` / `except` / `raise`, `defer`, `with` |
| Functions | `def`, defaults, `*args`, `**kwargs`, `lambda`, decorators, generics |
| Macros | `macro twice(x): quote:`, `name!(...)`, indented raw bodies, `match` token-tree arms |
| Operators | `+ - * / // % **`, bitwise `& | ^ ~ << >>`, comparisons, `in`, `is`, `as` casts |
| Async | `async def`, `await`, `Task[T]` / `Future[T]` |
| Module globals | `__name__`, `__file__`, `__package__`, `__doc__`, `__debug__`, `__sere_version__` |

Memory is explicit when you want it and automatic when you do not. The intrinsics are `unique[T](value)`, `shared[T](value)`, `alloc[T]()`, `load`, `store`, `free`, and `len`; `print` and `str` are intrinsics too.

`alloc` and `free` go through whichever collector is installed with `import gc`. The built-ins are `none`, `mark_sweep`, and `arena`. A custom collector implements `SereGcVTable` in C, calls `sere_gc_install` from `sere_mod_init`, and links with `--link`. Arenas and pools live in `import heap`.

## Standard library

The prelude is injected automatically, so `print`, `len`, `abs`, `min`, `max`, the collection methods, and the casts are always available. Everything else is an explicit import. The modules that ship today are:

`arrays`, `bit`, `bytes`, `encoding`, `env`, `fs`, `gc`, `hash`, `heap`, `html_lang`, `inspect`, `io`, `iterator`, `log`, `math`, `matrix`, `memory`, `ml`, `os`, `path`, `random`, `regex`, `requests`, `serestower`, `string`, `subprocess`, `sys`, `time`, `util`, `vec`, `windows`, `wsgi`.

[`docs/stdlib.md`](docs/stdlib.md) and the [topic reference](docs/reference/README.md) cover the surface in detail: exact method sets, the format mini-language, operator rules, the pointer vocabulary, and the dunder names the compiler recognizes.

## Editor support

The extension in `editors/vscode` turns `.sere` files into first-class citizens: syntax highlighting, diagnostics carrying the same exception codes as the compiler, markdown hover, completion, rename, semantic tokens, folding, and go-to-definition. It talks to `sere --lsp`, and the same `# type: ignore` comments that silence the compiler silence the editor.

Build the VSIX:

```powershell
.\scripts\package-vsix.ps1
```

That writes `dist/sere-<version>.vsix`. In VS Code or Cursor, open **Extensions → … → Install from VSIX…**, pick the file, and reload the window. The language server uses the RelWithDebInfo `sere.exe` under `build/` so it does not lock the `./bin/sere.exe` you may be refreshing.

## Packaging a release

Building a release produces a portable ZIP and an offline per-user setup EXE in one pass:

```powershell
.\releases\stage.ps1
# After extracting the portable package, -Editor installs the VSIX too:
.\releases\pre-<version>\windows-x64\install.ps1 -Editor
```

Outputs land in `releases/pre-<version>/`. LLVM and Windows linking support are bundled, so recipients need no admin access and no separate development install. `-PortableOnly`, `-WithoutEditor`, `-SkipBuild`, `-BuildDir`, and `-LlvmRoot` adjust what gets built; `sere --build-installer` and the `sere_release` CMake target drive the same pipeline. See [releases/README.md](releases/README.md) for build inputs and validation, and [docs/packaging.md](docs/packaging.md) for the installer internals.

## Repository layout

```
include/sere/   public compiler headers (ast, types, sema, macro, codegen, lsp)
lib/            lexer, parser, macros, type checker, LLVM codegen, driver, LSP
runtime/        heap, collectors, shared boxes, lists, printing
stdlib/         prelude plus io, fs, gc, heap, random, hash, sys, and the rest
tools/sere/     the sere executable (CLI, project driver, LSP, installer)
tests/          LLVM, lexer, parser, sema, macro, and example emit tests
examples/       hello, structs, enums, strings, macros, dunders, errors, gc, async
build/          CMake build trees (gitignored)
bin/            locally built sere.exe plus the sere-path helpers
dist/           VSIX, ZIP, and installer outputs (gitignored)
editors/vscode  language grammar and LSP client
scripts/        bootstrap, sere-path, project activation templates, Inno Setup helper
cmake/          LLVM discovery and warning policy
docs/           language reference and compiler handbook
packaging/      Windows installer templates
releases/       packaging scripts and versioned release artifacts
```

## Documentation

- [Getting started](docs/getting-started.md): create, run, and split an application into modules.
- [Language reference](docs/language.md): types, syntax, macros, and the stdlib surface, as implemented.
- [Topic reference](docs/reference/README.md): one page per subject, from strings to memory.
- [Projects](docs/projects.md): manifest settings, libraries, and troubleshooting.
- [Gradual typing](docs/gradual-typing.md): inference, `Any`, and typed boundaries.
- [Numeric arrays](docs/arrays.md): operations, statistics, and matrix examples.
- [Decorators](docs/decorators.md): `@name` and `@name(...)` on functions and classes.
- [Standard library](docs/stdlib.md): what ships and how to add to it.
- [Compiler handbook](docs/README.md): the pipeline, the library map, and how to extend the compiler.
- [Extending the compiler](docs/extending.md): where to add a keyword, type, intrinsic, module, or LSP feature.
- [Packaging](docs/packaging.md): release and installer internals.
- [Editor support](docs/lsp.md): how the language server is put together.

## Contributing

Patches are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) covers the build, the shape of a good patch, and what a change needs in the way of tests. The short version: one concern per pull request, C++20 in the existing style, and a test that fails before your fix. A compiler change without a test will regress.

## License

Sere is released under the [MIT License](LICENSE).

## Was this made with AI?

AI agents have been used for error detection, optimization opportunities, and testing. They have had no part in the language's creative design.
