# Running tests

From the repository root:

```powershell
.\tests\run-tests.ps1
```

The runner uses `bin/sere.exe` and the repository's `stdlib`. Build or refresh
the compiler first; the runner does not build it. Windows PowerShell 5.1 or
PowerShell 7 on Windows is supported. Paths are resolved from the script, so
you can invoke it from another directory.

Every immediate subdirectory of `tests/` is a category. The runner recursively
discovers every `.sere` file in each category, sorts them, and compiles **and
executes** each on `--backend=serem` and `--backend=llvm` independently. A failure
on one backend does not skip the other backend or subsequent tests.

```powershell
# One category, or multiple categories (wildcards are accepted)
.\tests\run-tests.ps1 -Category features
.\tests\run-tests.ps1 -Category features,regressions

# Match the path relative to tests/, including nested directories
.\tests\run-tests.ps1 -Filter '*classes*' -ShowOutput

# Use another compiler and optimization level
.\tests\run-tests.ps1 -Compiler .\build\static-field-fix\bin\sere.exe -Optimization O2

# Adjust process timeouts or where reports are saved
.\tests\run-tests.ps1 -CompileTimeoutSeconds 120 -RunTimeoutSeconds 60
.\tests\run-tests.ps1 -OutputDirectory .\build\my-test-results
```

The default optimization is `O0`. Assertions and runtime checks remain enabled.
Each compile has a 90-second timeout; each executable has a 30-second timeout.
Timed-out process trees are terminated. Standard input is closed by default,
so a test cannot wait indefinitely for interactive input.

## Writing tests

Add a standalone executable `.sere` program to a category such as
`tests/features/` or `tests/regressions/`. Use assertions for expected behavior
and return zero on success. Every `.sere` file is treated as an executable test;
keep import-only helpers outside the category tree.

Optional companion files use the same stem:

- `example.stdin`: bytes supplied to the program's standard input.
- `example.stdout`: expected standard output, compared case-sensitively. Only
  CRLF versus LF is normalized; spaces and the final newline matter.

Each backend runs in a separate, fresh artifact directory. Source paths are
absolute, but relative runtime file access uses that artifact directory, not
the source directory. Tests needing other runtime fixtures must locate them
explicitly. The runner currently expects successful compilation and execution;
negative compiler tests need a separate expected-failure convention.

## Results

Terminal output includes per-backend pass/failure status, compilation and run
times, failure diagnostic excerpts, a side-by-side comparison, category/backend
totals, and the five slowest backend runs.

`PASS` means compilation and execution both exited zero and any `.stdout`
expectation matched. Without assertions or an expected-output file, this is
only a successful-execution check. When both backends pass but print different
output, the comparison flags it as informational: nondeterministic programs
may legitimately differ.

Every invocation creates a unique directory under `build/test-results/`:

- `summary.md`: backend comparison and failure summary.
- `results.json`: all results, exact argument arrays, exit codes, timings,
  timeout flags, captured output, and log paths.
- `<category>/<test>.sere/<backend>/`: executable and separate compilation/run
  stdout/stderr logs. Complete diagnostics are retained even when the terminal
  excerpt is shortened.

Artifacts from previous runs are retained. Exit codes are `0` for all passing,
`1` for test failures, and `2` for setup/runner errors or incomplete runs. An
empty selection is an error, not a passing suite. `-TestRoot` can point at a
different directory with the same category layout.
