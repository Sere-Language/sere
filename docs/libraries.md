# Making Sere libraries

A Sere **library** is a project that other projects import: Sere modules, and
optionally native C/C++ and command-line tools, packed into one `.slib` file.
A consumer drops that file into its `libs/` directory and imports it — no flags,
include paths, or `PATH` edits.

This page is the author's guide: scaffold, public API, packing, native code,
shipping tools, and publishing. For the manifest keys see
[sere-toml.md](sere-toml.md); for the archive format and native linking see
[slib.md](slib.md).

## Create a library

```powershell
sere init-lib mathlib
cd mathlib
```

`init-lib` writes:

| Path | Purpose |
| --- | --- |
| `sere.toml` | `kind = "lib"`, entry `src/lib.sere`, output `dist/mathlib.slib` |
| `src/lib.sere` | The module consumers import |
| `libs/` | Extra Sere modules and `libs/native/` C/C++ |
| `libs/native/` | Example native source, `CMakeLists.txt`, and `libs/native.sere` |
| `dist/` | Packed `.slib` output |
| `README.md`, `.gitignore`, `.gitattributes` | Project docs and ignore rules |

`init-lib` does **not** create `bin/`. Create it yourself if you ship tools (see
[Shipping custom tools](#shipping-custom-tools)).

## Write the public API

The entry file is the module a consumer's `import <name>` resolves to. A library
has no `main`; the functions and types you declare at module scope are its API.

```sere
# src/lib.sere
"""Small math helpers."""


def add(left: i32, right: i32) -> i32:
    """Return the sum of two integers."""
    return left + right
```

Consumers then write:

```sere
import mathlib

def main() -> i32:
    print(mathlib.add(20, 22))   # 42
    return 0
```

`from mathlib import add` also works. See
[modules and imports](language.md#modules-and-imports) for `__exports__`, and
note that an unpacked folder library (`libs/mathlib/mathlib.sere`,
`libs/mathlib/lib.sere`, or `libs/mathlib/__init__.sere`) is importable too.

## Build and pack

```powershell
sere pack                       # dist/mathlib.slib
sere pack -o dist/other.slib    # a different output path
sere build                      # a library project: `build` packs
```

`sere build` on a `kind = "lib"` project is the same as `sere pack`. Packing
keeps the entry module and the local modules it actually imports (plus native
content); unused neighboring files are excluded.

Pack one module without a project at all:

```powershell
sere pack helpers.sere -o helpers.slib
```

Inspect what you produced before shipping it:

```powershell
sere slib-info dist/mathlib.slib
sere slib-verify dist/mathlib.slib
```

`slib-info` lists the entry, the Sere modules, and the native members;
`slib-verify` re-reads the archive the way a consumer will and exits non-zero on
damage.

## Native code

Put C/C++ in one of the layouts the build knows:

| Layout | Meaning |
| --- | --- |
| `native/` | Beside `sere.toml` |
| `libs/native/` | The scaffold layout |
| `<module>.lib` / `.a` / `.dll` / `.so` / `.dylib` | Beside a module file |

Native code is built by `sere pack` (always) and by `sere build` on an app when
`native = true` is set. A directory with a `CMakeLists.txt` is configured into
`build/`; otherwise the loose `.c`, `.cc`, `.cpp`, and `.cxx` files are compiled
directly. Both strategies may coexist.

Connect it to Sere with a bodyless `extern` declaration, then wrap it:

```sere
extern "C" "native_add"
def _native_add(left: i32, right: i32) -> i32

def add(left: i32, right: i32) -> i32:
    return _native_add(left, right)
```

```c
/* libs/native/example.cpp */
#include <stdint.h>

extern "C" int32_t native_add(int32_t left, int32_t right) {
    return left + right;
}
```

Packing reports what it carried:

```text
sere pack mathlib 0.1.0
  Sere modules:     1
  Native archives:  1
  Native runtimes:  0
  Native sources:   0
  Native headers:   0
```

When a package ships **sources** instead of a binary, the consumer compiles them
with its own toolchain, so one `.slib` works on every platform. When it ships a
binary, it is linked into the consumer's program automatically, including
whole-archive linking for libraries that register functions with
`Sere_DefineFunction`. [slib.md](slib.md) covers both in detail.

> **Check the pack summary.** A native build that fails is reported as a *note*
> and packing continues, producing a package with no native artifact — which
> then fails at the consumer's link step. Make sure `Native archives:` (or
> `Native sources:`) is non-zero for a library that uses native code.

### System libraries

If your native code links against system libraries that are not part of the C
and C++ runtimes, name them so the consumer's build links them too:

```toml
[build]
system_libs = ["opengl32", "gdi32"]
```

The names are recorded in the archive and resolved on the **consumer's**
machine. If one cannot be found there, the consumer's build fails with a message
naming the package and the library — so list only what your code really needs,
and prefer libraries that are present in a standard SDK.

### The scaffolded CMake gotcha

The `libs/native/CMakeLists.txt` that `init-lib` writes points its include path
at `venv/include`, which exists in an **app** project (created by `sere init`)
but not in a fresh library project. With the header missing, the CMake build
fails on `#include "sere/api/sere_mod.h"`, and because that is only a note the
package quietly ends up without its native archive.

Either:

* replace the `#include "sere/api/sere_mod.h"` line with `#include <stdint.h>` when
  your code does not use `Sere_DefineFunction` (the example only needs the fixed
  width types), or
* make the headers available, for example by copying the compiler's
  `include/sere/api` into `venv/include/sere/api` before packing.

## Shipping custom tools

A library can ship ready-to-run programs (a CLI, a helper, a generator) in
addition to its modules. Declare them under `executables` (the alias `scripts`
is accepted):

```toml
[build]
executables = ["hello-tool"]
```

The rules are simple:

1. **The tools must exist in `bin/` when you pack.** `sere pack` copies each one
   from `bin/` into the archive under `bin/` and records it in the package
   metadata. A missing tool is reported and skipped rather than packed.
2. **The consumer's build installs them into its own `bin/`**, next to the
   program it just built.

### Building a Sere tool

Write the tool as an ordinary Sere program and compile it to `bin/`:

```powershell
New-Item -ItemType Directory -Force bin | Out-Null
sere tools/hello.sere -o bin/hello-tool.exe
sere pack
```

A native tool works the same way: add it to `libs/native/CMakeLists.txt` with its
output directory pointed at the project's `bin/` (`sere pack` builds native
code first, so the binary is in place before the packer looks for it):

```cmake
add_executable(mycli cli.cpp)
set_target_properties(mycli PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin")
```

### What the consumer sees

After the consumer imports the library and builds, the tools are in the
consumer's `bin/`:

```text
sere build consumer -> …\consumer\bin/consumer.exe
… \consumer\bin\consumer.exe
… \consumer\bin\hello-tool.exe     <- installed from the library
```

Run them from `bin/` (`.\bin\hello-tool.exe`), or add `bin/` to your `PATH` if
you want them available by name.

Two notes:

* `slib-info` lists Sere modules and native members; shipped tools show up as
  extra files, not under `Native:`.
* `sere pack` still prints `no native content was packed` when only a tool was
  packed. That is expected — compare the archive's file count instead.

Do not commit built tools: the generated `.gitignore` ignores `*.exe`.

## Custom tool settings

Any key or section the compiler does not own is ignored, so use your own tables
for your own tooling. `sere update` rewrites only `[toolchain].sere` and leaves
them alone.

```toml
[tool.fmt]
line_width = 100

[tool.pack]
extra_files = ["LICENSE"]
```

Only keys in a section the compiler owns (or at the root) are read; `name` in a
custom table does not override `[project].name`. See
[sere-toml.md](sere-toml.md#unknown-keys-and-custom-tables).

## Consuming a library

```powershell
Copy-Item ..\mathlib\dist\mathlib.slib libs\
sere build
```

```sere
import mathlib
```

Repack and rebuild the consumer after changing compiled library code: the
consumer links against the archive, and the extracted copy under
`libs/.sere-lib/` is refreshed only when the `.slib` is newer. `sere clean`
removes that cache.

## Publishing

```powershell
sere login <token>      # once; sere login with no token shows status
sere publish            # packs this library project and uploads it
sere add mathlib@1.0.0  # in another project: install into ./libs
```

`sere publish` packs the project first, so the version in `[project].version` is
what consumers see. `--dry-run` describes the work without sending or writing.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `import mathlib` cannot be found | The `.slib` is in the consuming project's `libs/` and `[paths].entry` names the module |
| `undefined reference to native_add` | `sere pack` reported `Native archives: 0`; fix the native build, then repack |
| The package shipped no native binary | A native build failure is a note — read the pack output |
| `Native archives: 0` and no note | Set `native = true`, or put the sources under `native/` / `libs/native/` |
| `a package needs the system library 'x'` | Remove `x` from `system_libs` if it is not needed, or install the SDK that provides it |
| A shipped tool is missing for the consumer | It was not in `bin/` at pack time; build it first, then `sere pack` |
| A consumer still runs old code | Repack, replace the `.slib` in `libs/`, and rebuild; `sere clean` clears the extraction cache |

## Related

* [sere-toml.md](sere-toml.md) — every manifest key, including `executables` and `system_libs`
* [slib.md](slib.md) — archive format, native strategies, whole-archive linking
* [projects.md](projects.md) — applications, build/run/clean
* [modules and imports](language.md#modules-and-imports) — exports and resolution
