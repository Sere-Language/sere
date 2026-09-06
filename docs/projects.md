# Project configuration

## Everyday workflow

```powershell
sere init hello
cd hello
. .\scripts\activate.ps1
sere build
sere run
sere run -- first second
deactivate
```

`build` compiles the configured entry; `run` builds and then executes it. Arguments
after `--` go to the program. Dot-source activation to use the current terminal;
`sere shell` opens a nested project shell instead.

| Path | Purpose |
| --- | --- |
| `sere.toml` | Project identity, paths, and build settings |
| `src/main.sere` | Default application entry |
| `libs/` | Drop-in Sere libraries and native dependencies |
| `bin/` | Application output and local compiler helpers |
| `venv/` | Project environment and standard-library copy |
| `scripts/activate.ps1` | PowerShell activation |
| `dist/` | Packed library output |

Start with the [getting-started walkthrough](getting-started.md) for a complete
program and a local module.

## Manifest settings

New applications and libraries use these sections in `sere.toml`:

```toml
[project]
name = "hello"
version = "0.1.0"
kind = "app"                 # app or lib

[toolchain]
sere = "pre-0.1.5"

[paths]
src = "src"
entry = "src/main.sere"
libs = "libs"
stdlib = "venv/stdlib"

[build]
output = "bin/hello.exe"     # omit to select the platform default
opt = "O0"                   # O0, O1, O2, O3, Os, Oz
native = false              # build libs/native

[tool.example]
name = "custom metadata"     # does not override project.name
```

Paths resolve relative to `sere.toml`. Libraries use `kind = "lib"`,
`entry = "src/lib.sere"`, and default to `dist/<name>.slib`.
Unknown keys and custom tables are reserved for extensions and ignored by Sere.
The compiler reads scalar string/boolean settings; it does not interpret custom
arrays or tables. Single-quoted literal paths are useful for Windows paths.
Legacy manifests with the same keys at the root remain supported.
`sere update` edits the toolchain version without replacing custom settings.

Virtual environments contain one stdlib tree at `venv/stdlib`, including real
module subdirectories. Legacy nested copies named `stdlib/stdlib` are excluded
when copying a toolchain. The compiler tools are shared with the installation
recorded in `venv/sere.cfg`, so each project does not duplicate LLVM or the SDK.

## Packing and consuming libraries

From a directory containing your application project `hello`:

```powershell
sere init-lib mathlib
cd mathlib
sere pack
Copy-Item dist/mathlib.slib ../hello/libs/
cd ../hello
sere build
```

A library uses `src/lib.sere` as its default entry. Put its public functions and
types there; a library does not need an application `main`. Consumers use
`import mathlib` or `from mathlib import add` for an exported `add` function.

`sere build` also packs projects with `kind = "lib"`. To package one module
without a manifest, use `sere pack helpers.sere -o helpers.slib`. Packing includes
the entry, the local modules it imports, and compiled native objects. Unused
neighboring source files are excluded.

An unpacked `libs/mathlib/lib.sere` or `libs/mathlib/mathlib.sere` also provides
`import mathlib`. See [modules and imports](language.md#modules-and-imports) for
exports and resolution, and [native interop](language.md#native-interop) for C
bindings. Repack and rebuild consumers after changes to compiled library code.

## Inspecting and refreshing the environment

```powershell
Get-Command sere
sere --version
sere --print-env
```

Use these commands to identify the executable and paths currently in use.
`sere update` copies the compiler you invoked to the per-user installation when
it differs and refreshes the current project's environment. It does not download
a release: invoke it from the compiler version you want to install.
`sere refresh-bin` copies the running compiler, runtime, and stdlib into `./bin`.

`sere clean` removes the project's `bin/`, `dist/`, native build directory under
`libs/native/build/`, and extracted library cache under `libs/.sere-lib/`, then
recreates `bin/`. Keep source and hand-maintained files outside those directories.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| PowerShell cannot find `sere` | Activate the project or invoke the installed compiler by its full path |
| Changes to a different source file do not affect the build | Inspect `[paths].entry` in `sere.toml` |
| An import cannot be found | Check module spelling, the importing file's directory, and the project's `libs/` |
| Terminal and editor disagree | Compare `sere --print-env` with the editor's configured compiler; see [editor setup](../editors/vscode/README.md) |
| An edited library still behaves as before | Repack it, replace the consuming project's `.slib`, and rebuild |
| A custom output path is not cleaned | `clean` removes the standard artifact directories; inspect your `[build].output` path separately |
