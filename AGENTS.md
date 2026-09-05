# Repository Guidelines

## Project Structure & Module Organization

Sere is a compiled typed Python superset compiler with an LLVM 22 backend, implemented in C++20. The compiler and language server (`sere --lsp`) share a unified frontend analysis pipeline:

- `.\include\sere\`: Public API headers organized by component (`ast`, `types`, `sema`, `macro`, `codegen`, `lsp`, `driver`).
- `.\lib\`: Core compiler implementations following a strict linear dependency chain (`diag` → `source` → `lex` → `types` → `ast` → `parse` → `macro` → `sema` → `codegen` → `driver`).
- `.\runtime\`: Runtime support library (`sere_rt`), garbage collectors (mark-sweep, arena), and native integration.
- `.\stdlib\`: Standard library modules and `.\stdlib\prelude.sere`.
- `.\tools\sere\`: Main executable entry point and CLI driver.
- `.\tests\`: Unit tests and example emit tests.
- `.\editors\vscode\`: VS Code / Cursor language extension and LSP client.

## Build, Test, and Development Commands

Requires CMake 3.28+, Ninja 1.11+, MSVC x64, and LLVM 22.1.8.

- Bootstrap toolchain: `.\scripts\bootstrap.ps1`
- Load environment (must dot-source in PowerShell): `. .\scripts\env.ps1`
- Configure preset: `cmake --preset windows-clang-cl-relwithdebinfo`
- Build compiler: `cmake --build --preset windows-clang-cl-relwithdebinfo`
- Run test suite: `ctest --preset windows-clang-cl-relwithdebinfo --output-on-failure`
- Run a single test: `ctest --preset windows-clang-cl-relwithdebinfo -R sere.test.sema --output-on-failure`
- Package VS Code extension: `.\scripts\package-vsix.ps1`
- Package release: `.\releases\stage.ps1`

## Coding Style & Naming Conventions

- **Standards:** C++20 (`.cpp`/`.h`), C17 (`.c`/`.h`), 2-space indentation (4 spaces for `*.sere`), LF line endings (`.\.editorconfig`).
- **Formatting:** LLVM style via `clang-format` (`.\.clang-format`), 100 column limit, left pointer alignment.
- **Naming (`.\.clang-tidy`):** `CamelCase` for classes, structs, enums, type aliases, and constants; `camelBack` for functions, methods, members, and variables; `lower_case` for `sere` namespaces.
- **Rules:** Mark pure query methods `[[nodiscard]]`. Never use C++ exceptions for user diagnostics; use `DiagnosticEngine` with appropriate `DiagnosticCode` identifiers (`NameError`, `TypeError`, etc.).

## Testing Guidelines

- Test framework: `CTest` managing custom C++ executables and CLI emit validations (`.\tests\CMakeLists.txt`).
- Unit tests (`.\tests\*.cpp`) isolate parser, sema, macro, and driver behaviors.
- Integration tests verify `sere --emit-llvm examples/<name>.sere`.
- `SERE_STDLIB` is automatically forwarded to `.\stdlib` during test execution.

## Commit & Pull Request Guidelines

- One distinct concern per patch or pull request.
- Use concise, imperative commit messages summarizing changes (e.g., `Fixed strings, and made types objects`, `Add member completion tests`).
- Keep public interfaces in `.\include\sere\` synced with implementations in `.\lib\`.
