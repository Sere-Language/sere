# Project configuration

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
