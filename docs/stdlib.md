# Standard library

`stdlib/` is ordinary Sere. The compiler injects `prelude.sere` into every
program. Everything else is opt-in:

```sere
import io
import gc
from math import sqrt
```

Search path (`ImportPath`): directory of the importing file, then the stdlib
directory next to `sere` (or `SERE_STDLIB` in tests).

## Prelude

`stdlib/prelude.sere` is always loaded and marked `fromPrelude()`. Keep it
small: names everyone needs, plus macros such as `dbg!`. Memory vocabulary is
documented in `stdlib/memory.sere` (comments; the types themselves are
compiler generics).

`print` is both a prelude-friendly name and a compiler intrinsic. Prefer
calling the existing intrinsic rather than reimplementing I/O in Sere.

## Modules

| Module | Role |
| --- | --- |
| `io` | Extra I/O (`read_line`, `eprint`) |
| `fs`, `path`, `os`, `env`, `sys` | Filesystem and process |
| `string`, `bytes`, `encoding`, `regex` | Text and binary |
| `math`, `vec`, `matrix`, `ml`, `arrays` | Numeric |
| `hash`, `random`, `time`, `log`, `bit` | Utilities |
| `gc`, `heap`, `memory` | Collectors, arenas, pointer docs |
| `inspect` | Runtime inspection helpers |
| `html_lang` | Indent-body HTML macro support |
| `windows`, `gl`, `qt6` | Native UI / graphics (GL: window close/state, shaders, mesh, FBO) |
| `requests` | HTTP client (`get` / `post` / `put` / `delete`) |
| `wsgi` | Blocking HTTP server; subclass `Handler` and implement `handle` |

Bindings that need C use:

```sere
extern "C" "sere_gc_collect"
def collect() -> void
```

The string must match a symbol in `sere_rt` (or a library passed with `--link`).

## Native stdlib surface

If a module needs new C:

1. Add the C function to `runtime/` and declare it in `sere_rt.h` (or the
   matching public API header).
2. Rebuild `sere_rt`.
3. Declare `extern "C"` in `stdlib/yourmod.sere`.
4. Add `examples/…` and a `sere.example.*` test that `--emit-llvm`s it.

Optional heavy deps (Qt6) are behind CMake `find_package`. When Qt is missing,
`sere_qt6_stub.c` still links so `import qt6` typechecks; runtime calls fail
closed. Do not assume Qt is present in tests that only emit LLVM.

## Project layout vs stdlib

`sere init` creates a project with its own `src/` and `libs/`. User modules
resolve relative to the importing file. Publish reusable code with
`sere init-lib` + `sere pack` as a single `.slib` (reachable sources plus
compiled native objects). Drop that file into a project's `libs/` and
`import` it. A folder `libs/mylib/` with `lib.sere` or `mylib.sere` (and
optional C sources) is the same import without packing. Loose `.sere` files
on the import path still work. Neither belongs in `stdlib/` unless it is
part of the language distribution.
