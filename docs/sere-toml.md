# `sere.toml` reference

Every Sere project and library is described by one `sere.toml` at its root. The
compiler reads it to find the entry file, the import roots, the standard
library, the output path, and how to build native code and ship tools.

This page is the complete reference: every section, every key, the defaults, the
value syntax the reader accepts, and the keys it deliberately ignores.
See [projects.md](projects.md) for the everyday workflow and
[libraries.md](libraries.md) for authoring a library.

```toml
[project]
name = "hello"
version = "0.1.0"
kind = "app"                 # app or lib

[toolchain]
sere = "pre-0.2.1"           # compiler/stdlib pin

[paths]
src = "src"
entry = "src/main.sere"
libs = "libs"
stdlib = "venv/stdlib"

[build]
output = "bin/hello.exe"     # omit for the platform default
opt = "O0"                   # O0 O1 O2 O3 Os Oz
native = false               # build libs/native on sere build
system_libs = []             # for a library: -l names its consumer must link
executables = []             # for a library: tools to ship in bin/

# Anything else is reserved for your own tooling and ignored by Sere.
[tool.mytool]
profile = "release"
```

## How the manifest is found

The compiler walks **up** from the file being compiled (or from the working
directory) until it finds a `sere.toml`. That directory is the project root, and
it decides:

| Effect | Detail |
| --- | --- |
| Entry file | `[paths].entry`, resolved against the root |
| Import roots | `<root>/libs`, `<root>/src`, then `<root>` |
| Standard library | `[paths].stdlib` when it holds `prelude.sere` |
| Output path | `[build].output`, or the default for the kind |

`SERE_PROJECT_ROOT` names a project to use when the walk finds none, so you can
compile a file from outside its tree.

The standard library is chosen in this order: the stdlib of an **activated**
project environment (anything that exports `SERE_ACTIVE` together with a matching
`SERE_PROJECT_ROOT`), then the project's `[paths].stdlib`, then the copy shipped
next to the compiler — `SERE_STDLIB` is only a fallback for layouts with no
sibling stdlib. `sere --print-env` shows what was chosen. `bin/sere-path` only
puts a compiler on `PATH`; it does not activate the project.

The manifest is optional. Compiling a loose `file.sere` without any
`sere.toml` in scope works exactly as before.

## Sections and keys

Keys are matched **by name**, and only in the section that owns them. Writing
`native` under `[project]` is ignored — it is not an error. At the root (before
any `[section]` line) every recognised key is accepted, which is what keeps
legacy manifests working.

### `[project]`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | the project directory name | Package name: output file, `.slib` name, `import` name |
| `version` | string | `"0.1.0"` | Package version recorded in a `.slib` |
| `kind` | string | `"app"` | `"lib"`/`"library"` builds a library; anything else is an app |

### `[toolchain]`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `sere` | string | unset | The compiler/stdlib version the project was created with. Information for tooling: `sere update` rewrites it in place without touching your other settings |

### `[paths]`

All four are resolved relative to the directory that holds `sere.toml`; an
absolute path is used as-is.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `src` | string | `"src"` | Source directory, added to the import search path |
| `entry` | string | `"src/main.sere"` | The file `sere build` compiles. `sere init-lib` writes `"src/lib.sere"` explicitly |
| `libs` | string | `"libs"` | Drop-in libraries: `.slib` files, folder libraries, native deps |
| `stdlib` | string | `"venv/stdlib"` | Project copy of the standard library; regenerated when missing |

### `[build]`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `output` | string | app: `bin/<name>[.exe]`; lib: `dist/<name>.slib` | Written artifact |
| `opt` | string | `"O0"` | Optimization level: `O0`–`O3`, `Os`, `Oz` (also `-O2`, or a bare `2`) |
| `native` | bool | `false` | Build native C/C++ before compiling (see below) |
| `system_libs` | list | empty | Packaged project: system library names its consumers must link |
| `system-libs` | list | — | Accepted alias of `system_libs` |
| `executables` | list | empty | Packaged project: tools to pack and install into the consumer's `bin/` |
| `scripts` | list | — | Accepted alias of `executables` |

`opt` is a **default**, not a lock: a command-line level always wins, so
`sere build -O3` or `sere build --release` overrides `opt` for that build. An
unparseable `opt` value is ignored and `O0` stays in effect. See
[optimization.md](optimization.md) for every switch.

#### `native`

`native = true` compiles the project's C/C++ before the Sere sources:

* `sere build` (an app) builds **`libs/native/`**.
* `sere pack` (a library) builds **`native/`**, **`libs/native/`**, and the
  library root, and packs whatever those produced.

A directory is built with `CMakeLists.txt` when it has one (into
`build/`), otherwise as loose `.c`, `.cc`, `.cpp`, and `.cxx` files, which are
archived as `sere_native.lib` on Windows or `libsere_native.a` elsewhere. A
native build failure is a note, not an error — the Sere half still compiles.
When no binary was packed, the sources, headers, and `CMakeLists.txt` are packed
instead so a consumer on another platform builds them.

#### `system_libs`

Names recorded in the `.slib` and resolved on the **consumer's** machine at
link time, so importing a binding never requires the consumer to know that, say,
an OpenGL wrapper needs `opengl32`:

```toml
[build]
system_libs = ["opengl32", "gdi32"]
```

A name that cannot be found on the consumer's machine is a hard error naming the
package and the library. This key only affects packing; it does not add `-l`
flags to the library's own build.

#### `executables` (alias `scripts`)

Programs a library ships, in addition to its Sere modules and native code. The
build must have produced each one in `bin/` before `sere pack` runs:

```toml
[build]
executables = ["mycli"]
```

`mycli.exe` (or `mycli`) is packed under `bin/` and listed in the archive's
package metadata. When another project imports the library, the compiler copies
those programs into that project's `bin/`, alongside the program it just built.
A declared executable that is **not** present in `bin/` is reported with a note
and skipped rather than packed. See
[libraries.md](libraries.md#shipping-custom-tools).

### Unknown keys and custom tables

Everything the compiler does not recognise is ignored. That is intentional: use
your own sections for your own tooling, and the compiler will leave them alone
and `sere update` will not drop them.

```toml
[tool.fmt]
line_width = 100

[tool.pack]
extra_files = ["LICENSE"]
```

`name` inside a custom table does **not** override `[project].name`: only keys
in a section the compiler owns (or at the root) are read.

## Value syntax

The reader is line oriented and intentionally small — it is not a full TOML
parser. What it accepts:

| Form | Example | Notes |
| --- | --- | --- |
| Section | `[project]` | A label; keys are matched against it |
| String | `name = "hello"` | `"…"` decodes `\n` `\r` `\t` `\b` `\f`; `'…'` is literal |
| Single-quoted path | `entry = 'C:\work\main.sere'` | Useful on Windows; no escapes |
| Boolean | `native = true` | `true`, `1`, or `yes`; anything else is false |
| List | `executables = ["a", "b"]` | Also `a, b`; either quote strips; commas in quotes do not split |
| Comment | `opt = "O2"  # faster` | `#` to end of line, ignored outside quotes |
| Blank lines | | Ignored |

Notes and limits:

* One `key = value` per line; a value cannot span lines and there are no
  nested tables or arrays of tables.
* Duplicate keys: the last one wins.
* Unknown keys never fail the build; a typo silently keeps the default. When a
  setting seems ignored, check its spelling and its section.
* Paths in `[paths]` and `[build].output` are relative to `sere.toml`.

## Defaults at a glance

| Setting | Application | Library |
| --- | --- | --- |
| `kind` | `app` | `lib` |
| `entry` | `src/main.sere` | `src/main.sere` (written as `src/lib.sere` by `sere init-lib`) |
| `output` | `bin/<name>[.exe]` | `dist/<name>.slib` |
| `native` | off | off |
| Everything else | `src`, `libs`, `venv/stdlib`, `opt = "O0"` | same |

## What `sere` does with each key

| Key | `init` | `build` | `pack` | `run` | `clean` |
| --- | --- | --- | --- | --- | --- |
| `name`, `version` | writes | output name | archive metadata | exe name | report |
| `kind` | writes | app binary / pack | forces packing | refuses a library | — |
| `sere` | writes | — | — | — | — |
| `src`, `libs` | writes | import roots | reachability scan | import roots | `libs/native/build`, `libs/.sere-lib` |
| `entry` | writes | compiled | packed root module | compiled | — |
| `stdlib` | writes | `SERE_STDLIB` | — | `SERE_STDLIB` | — |
| `output` | writes | written | written | executed | `bin/`, `dist/` |
| `opt` | writes | default level | — | default level | — |
| `native` | writes | builds `libs/native` | always builds and packs | builds first | `libs/native/build` |
| `system_libs` | — | — | recorded for consumers | — | — |
| `executables` | — | — | packed from `bin/` | — | `bin/` |
| `[tool.*]` | — | ignored | ignored | ignored | ignored |

## Related

* [projects.md](projects.md) — create, build, run, and clean a project
* [libraries.md](libraries.md) — author, pack, and consume a library
* [slib.md](slib.md) — what a `.slib` contains and how native code links
* [optimization.md](optimization.md) — every `opt` level and switch
