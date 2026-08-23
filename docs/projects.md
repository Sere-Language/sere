# Projects, libraries, and the CLI

The driver is both a compiler and a small project tool. Commands live in
`lib/driver/Options.cpp` (parse) and `lib/driver/Project*.cpp` /
`Library.cpp` (work). A project is any tree with a `sere.toml`.

## Commands

```
sere <command> [options]
sere [options] <file.sere>
```

| Command | Also accepted | Effect |
| --- | --- | --- |
| `init <name>` | `--init`, `init --lib` | Scaffold an app (`src/main.sere`, `venv/`, `scripts/`) |
| `init-lib <name>` | `--init-lib`, `init_lib` | Scaffold a library (`src/lib.sere`, `dist/`) |
| `build` | | App: compile `entry` to `output`. Lib: same as `pack` |
| `pack [file.sere]` | `--pack`, `build-lib` | Write a reachable `.slib` |
| `run [-- <args>]` | | `build` then execute the app |
| `clean` | | Remove `bin/` and `dist/` artifacts |
| `shell` | `activate`, `--host` | Nested project shell |
| `refresh-bin` | `--refresh-bin`, `self-update` | Copy this compiler, runtime, and stdlib into `./bin` |
| `build-installer` | `--build-installer` | Windows setup exe ([packaging.md](packaging.md)) |

Default names when you omit `<name>`: `sere-project` (app) and `sere-lib`
(library).

Compiler flags that apply to a single file or to `build` / `run`:

| Flag | Effect |
| --- | --- |
| `--emit-llvm` / `--emit-asm` / `-S` | Stop before linking |
| `--analyze` | JSON diagnostics, no codegen |
| `--lsp` | Language server on stdin/stdout |
| `--link <lib>` | Extra native library |
| `--opt=` `O0`…`O3`,`Os`,`Oz` | LLVM level (overrides `sere.toml`) |
| `--passes=` | Custom PassBuilder pipeline |
| `--color=` `auto`\|`always`\|`never` | Diagnostics color (`--no-color` = never) |
| `-o <path>` | Output path |

`--emit-llvm` and `--emit-asm` cannot be combined.

## App workflow

```powershell
sere init myapp
cd myapp
. .\scripts\activate.ps1
sere build
sere run -- hello
deactivate
```

Dot-source `scripts/activate.ps1` (or `scripts/activate` / `activate.bat`) so
this terminal stays put. Running the script without `.` / `source` starts
`sere shell` in a nested process instead. `deactivate` restores `PATH` and the
prompt; it does not close the window.

`sere shell --host powershell|cmd|bash` does the nested form on purpose.

## Library workflow

```powershell
sere init-lib mathlib
cd mathlib
sere pack
copy dist\mathlib.slib ..\myapp\libs\
```

```sere
import mathlib
```

`sere pack file.sere -o mathlib.slib` packs a single module without a project.
`sere build` in a `kind = "lib"` tree is `pack`.

## `sere.toml`

Walked from the file or cwd up to the first `sere.toml`
(`findProjectRoot` in `Project.cpp`). Keys are flat `key = value` lines.
`#` comments and `[sections]` are ignored.

| Key | Default (app / lib) | Meaning |
| --- | --- | --- |
| `kind` | `app` / `lib` | `lib` or `library` packs on `build` |
| `name` | directory name | Binary or `.slib` stem |
| `version` | `0.1.0` | Stored in the `.slib` header |
| `src` | `src` | Source root (also an import search dir) |
| `entry` | `src/main.sere` / `src/lib.sere` | File compiled or packed |
| `libs` | `libs` | Drop-in `.slib` / folder libraries |
| `stdlib` | `venv/stdlib` | Copied from the compiler if prelude is missing |
| `output` | `bin/<name>` / `dist/<name>.slib` | Build product |
| `opt` | `O0` | Used unless `--opt` is on the command line |
| `native` | `false` | Build `libs/native` (CMake or loose `.c`/`.cpp`) |

`native` accepts `true` / `1` / `yes`. A failed native build prints a note and
continues with Sere sources.

## How analysis finds the project

`resolveLanguageContext` (`Project.h`) is shared by `sere`, `sere --lsp`,
`sere --analyze`, and `sere pack`:

1. Nearest `sere.toml` walking from the file.
2. Else `SERE_PROJECT_ROOT` if that tree has a `sere.toml`.
3. Else `sere.toml` from the current working directory.
4. Stdlib: activated `SERE_STDLIB` only when `SERE_ACTIVE` is set **and**
   `SERE_PROJECT_ROOT` matches this project; else the project `stdlib` path;
   else `SERE_STDLIB`; else the compiler's own stdlib.

Import search then appends the project's `libs/`, `src/`, and root
(`appendLanguageContextDirs`). See [language.md](language.md#modules-and-imports).

## What `.slib` contains

`Library.cpp` writes `SERELIB/2` (zlib members when that shrinks the payload;
`SERELIB/1` is still readable). Pack **typechecks the entry first**. It keeps:

- the entry file
- local `.sere` modules that import walk actually loaded (not the rest of the
  tree, not stdlib, not already-extracted libraries)
- compiled native objects (`.lib` / `.a` / `.dll` / `.so` / `.dylib`) next to
  those modules, plus `native/` and `libs/native/`
- loose `.c` / `.cpp` / `CMakeLists.txt` only when no native binary is present

Consumers extract next to the `.slib` under `.sere-lib/<stem>/` when the
archive is newer than `.extracted`. `import name` then loads that tree.
`.sere` wins if both `name.sere` and `name.slib` exist.

Folder libraries (no pack): `libs/mylib/lib.sere` or `libs/mylib/mylib.sere`
plus optional C sources.

## Activate vs `./bin`

| Mechanism | Purpose |
| --- | --- |
| `. scripts/activate.ps1` | This terminal: `SERE_ACTIVE`, `SERE_PROJECT_ROOT`, project `PATH` |
| `sere shell` | Same env in a nested host |
| `.\bin\sere-path.ps1` | Put the **compiler** on `PATH` (optional `-Persistent`) |
| `sere refresh-bin` | Replace workspace `./bin/sere` from the running binary when the LSP has the old exe locked |

The language server prefers the CMake build-tree compiler so it does not lock
`./bin/sere`. After a rebuild, refresh `./bin` and restart the server
([lsp.md](lsp.md)).

## Pitfalls

- **Forgot the leading `.` on activate** — you get a nested shell, not this
  terminal. `deactivate` inside that shell leaves the nest; the outer prompt
  is unchanged.
- **`SERE_STDLIB` from an old activate** — ignored unless `SERE_ACTIVE` and
  `SERE_PROJECT_ROOT` still match the `sere.toml` being analyzed.
- **Pack looks empty** — unused sibling `.sere` files are not packed. Import
  them from the entry (or a reachable local module).
- **Native symbols missing at link** — set `native = true`, or put objects
  next to the module / in `native/`, and rebuild `sere_rt` when you add
  runtime C. The `.slib` must contain the compiled object, not only the
  `extern "C"` declaration.
- **`sere build` in a lib project writes a `.slib`**, not an exe. Use
  `kind = "app"` for a program.
- **`env.ps1` is for compiling sere itself**, not for `sere build` of a user
  project. Users need the pinned clang next to the compiler
  ([backend.md](backend.md)).
