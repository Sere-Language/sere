# Testing

Two styles:

1. **C++ unit tests** in `tests/*.cpp` — lexer, parser, sema, macros, LSP, CLI.
2. **Example emit tests** — `sere --emit-llvm examples/foo.sere` must succeed.

CMake wires them in `tests/CMakeLists.txt`. `SERE_STDLIB` is set to the repo
`stdlib/` so prelude resolution does not depend on install layout.

## Run

```powershell
. .\scripts\env.ps1
cmake --build --preset windows-clang-cl-relwithdebinfo
ctest --preset windows-clang-cl-relwithdebinfo --output-on-failure
```

Single test:

```powershell
ctest --preset windows-clang-cl-relwithdebinfo -R sere.test.sema --output-on-failure
```

## Where to put a new test

| You changed… | Add… |
| --- | --- |
| Lexer / keywords | `tests/lex_basic.cpp` |
| Grammar | `tests/parse_fn.cpp` or `tests/parse_lang.cpp` |
| Types / pointers / inference | `tests/sema_types.cpp` or `tests/sema_control.cpp` |
| Macros | `tests/macro_tt.cpp`, `macro_expand.cpp`, `macro_errors.cpp` |
| `# type: ignore` | `tests/type_ignore.cpp` |
| LSP hover / tokens | `tests/macro_lsp.cpp` or `tests/lsp_semantic.cpp` |
| `sere init/build` | `tests/project_cli.cpp` |
| `.slib` pack / import | `tests/library_pack.cpp` |
| `@private` exports | `tests/import_private.cpp` |
| An optimization switch or an IR rewrite | `tests/opt_options.cpp` |
| An optimization switch end to end | `add_test(NAME sere.opt.your …)` with `--emit-llvm` and the flags |
| A Serem pass | `tests/smoke_serem.cpp` or a new case in `tests/opt_options.cpp` |
| End-to-end language feature | `examples/your.sere` + `add_test(NAME sere.example.your …)` |

## Optimization tests

`tests/opt_options.cpp` links `sere_codegen` and exercises both halves of the
optimizer:

- flag parsing: levels, per-pass switches, the `--no-…` inverses, `--release`,
  `--debug`, and the clang flags derived from them;
- the IR rewrites: null-check folding, panic-block removal, stack promotion,
  free elision, plus `llvm::verifyModule` after each rewrite so a pass that
  produces invalid IR fails the test;
- the pipelines themselves: `runOptPipeline` must parse and run for a level, a
  flag-composed pipeline, and the LTO pipeline. A pass name that LLVM does not
  know fails here rather than in a user's build.

Sere's own flags are also covered end to end in `tests/CMakeLists.txt`
(`sere.opt.release`, `sere.opt.pass_flags`, `sere.opt.custom_pipeline`,
`sere.opt.checks_off`, `sere.opt.size`), which is what catches a switch that
parses but breaks emission.

Example tests only require `--emit-llvm` success (typecheck + IR). That is
enough to catch most frontend/backend mismatches without running the `.exe`.

When the feature is runtime-visible (GC, files, Qt), add a small `.sere`
example and document how to run it; keep CI on `--emit-llvm` unless the test
harness can invoke the binary reliably.

## Style

- One focused assertion path; `fail("why")` on error.
- Use `Frontend::analyze` when you need prelude, imports, and macros together.
- Direct `Parser` + `TypeChecker` is finer-grained when you are isolating sema.
- Do not spawn `sere --lsp` in unit tests.
