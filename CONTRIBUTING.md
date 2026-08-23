# Contributing to Sere

Thank you for working on the compiler. Read [docs/README.md](docs/README.md)
first. The extension recipes in [docs/extending.md](docs/extending.md) are the
source of truth for “where do I put this?”

## Build

```powershell
.\scripts\bootstrap.ps1
. .\scripts\env.ps1
cmake --preset windows-clang-cl-relwithdebinfo
cmake --build --preset windows-clang-cl-relwithdebinfo
ctest --preset windows-clang-cl-relwithdebinfo --output-on-failure
```

`env.ps1` must be **dot-sourced**. Set `SERE_LLVM_DIR` with forward slashes if
CMake complains about `\U` in `CMAKE_RANLIB`:

```powershell
$env:SERE_LLVM_DIR = "C:/Users/<you>/AppData/Local/sere/toolchains/llvm-22.1.8"
```

## Patch shape

- One concern per change. A new keyword is not the time to reformat IRGenerator.
- Match the surrounding C++: C++20, file `@file` docstring, `sere` namespace,
  `[[nodiscard]]` on pure queries, no C++ exceptions for user diagnostics.
- Public API goes in `include/sere/<area>/`. Implementation in `lib/<area>/`.
- Do not introduce circular library dependencies (`lib/CMakeLists.txt` order
  is the allowed graph).
- Language rules belong in parse / sema / codegen. Editor UX belongs in
  `lib/lsp` and `editors/vscode`.
- Prefer a stdlib `extern "C"` wrapper over a new compiler intrinsic when the
  operation is a runtime call.

## Tests

A compiler change without a test will regress. See [docs/testing.md](docs/testing.md).

At minimum: a unit test that fails before your patch, or an `examples/*.sere`
`--emit-llvm` test for user-facing syntax.

## Editor

If you change the compiler binary, restart the language server. Stdlib,
`sere.toml`, and project `src/` / `libs/` edits refresh automatically. The
workspace compiler path is `build/windows-clang-cl-relwithdebinfo/bin/sere.exe`.
Copying to `bin/` can fail while the LSP still has `bin/sere.exe` open; that is
expected.

## Docs

User-facing syntax: [docs/language.md](docs/language.md) (keep the README
language table in sync). Memory vocabulary also lives in `stdlib/memory.sere`.
Internals: update the matching file under `docs/`. Installer / packaging:
[docs/packaging.md](docs/packaging.md).
