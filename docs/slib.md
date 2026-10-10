# Packed libraries (`.slib`)

A `.slib` is a Sere library in one file: Sere modules, any native C/C++
components they use, headers, metadata, and any command-line tools the library
installs. A consumer drops it into `libs/` and imports it — the compiler
compiles or links the native part for the consumer's platform without extra
flags, include paths, or copying DLLs by hand.

This page covers the archive format and native linking. For the author's
workflow — scaffold, public API, packing, `system_libs`, shipping tools, and
publishing — see [libraries.md](libraries.md).

```text
mylib/
    sere.toml
    src/
        __init__.sere
    native/
        math.c
```

```toml
[project]
name = "mylib"
version = "1.0.0"
kind = "lib"

[paths]
entry = "src/__init__.sere"

[build]
native = true
```

```sere
# src/__init__.sere
extern "C" "mylib_add"
def _mylib_add(left: i32, right: i32) -> i32

def add(left: i32, right: i32) -> i32:
    return _mylib_add(left, right)
```

```c
/* native/math.c */
#include <stdint.h>

int32_t mylib_add(int32_t left, int32_t right) {
    return left + right;
}
```

```text
$ sere pack
sere pack mylib 1.0.0
  Sere modules:     1
  Native archives:  1
  Native runtimes:  0
  Native sources:   0
  Native headers:   0
  Archive:          dist/mylib.slib (… bytes, … files)
```

The consumer copies `dist/mylib.slib` into its `libs/` directory and imports it:

```sere
import mylib

def main() -> i32:
    print(mylib.add(20, 22)) # 42
    return 0
```

No `-L`, `-l`, `-I`, `PATH`, or `LD_LIBRARY_PATH` changes are needed.

## What a `.slib` contains

An archive is a small, deterministic format — not a ZIP — chosen so the
compiler can read it without a dependency:

```text
SERELIB/3
name=mylib
version=1.0.0
entry=src/__init__.sere
encoding=zlib
hashes=sha256
HASH src/__init__.sere 9f86d0…
HASH native/build/libsere_native.a 4e2b6c…
HASH native/math.c 1c3f9a…

FILE src/__init__.sere <raw> <packed>
<zlib payload>
FILE native/build/libsere_native.a <raw> <packed>
<zlib payload>
```

* `SERELIB/3` is the format version. Versions 1 and 2 (no hashes) still read.
  A newer version than the compiler understands is refused with a message
  naming the format, so an old compiler never mis-reads a new archive.
* `entry=` is the module a consumer's `import` resolves to. `name=` and
  `version=` describe the package.
* `HASH <path> <sha256>` records the digest of every member. Extraction
  verifies each one, so a truncated, edited, or half-written package fails
  loudly instead of linking something wrong.
* Members are written in path order, so packing the same content twice
  produces identical bytes — useful for package hashes and caching.
* Paths are validated on both write and read: `..`, absolute paths, and drive
  letters are rejected, so an archive can never escape its extraction
  directory.

Extracted packages live next to the archive in `.sere-lib/<name>/`, and are
re-extracted only when the archive is newer than the tracked stamp.

## Native content

Native content is packed from every layout the build knows:

| Layout | Meaning |
| --- | --- |
| `native/` | beside `sere.toml` or an entry file |
| `libs/native/` | the project scaffold layout |
| `<module>.lib`, `<module>.a`, `<module>.dll`, `<module>.so` | beside a module file |

Two strategies are supported, and they can coexist in one package across
platforms:

**Prebuilt artifacts.** `sere pack` builds `native/` first — through
`native/CMakeLists.txt` when present, otherwise by compiling loose `.c`,
`.cc`, `.cpp`, and `.cxx` with the configured toolchain — and packs the
resulting `.lib`, `.a`, `.dll`, `.so`, or `.dylib`.

**Shipped source.** When no native binary was packed, the `.c`/`.cpp` sources,
their headers, and `CMakeLists.txt` are packed instead. A consumer on another
platform compiles them with its own toolchain, so a portable package works
everywhere without shipping a binary per target.

Headers are packed alongside sources because the consumer's compiler needs the
function declarations to build them.

## Native registration

A native component that registers itself with `Sere_DefineFunction`
(`sere/api/sere_mod.h`) must be linked as a whole archive: nothing in a Sere
program references its symbols directly, so an archive linked normally would be
discarded and the registration would never run. Archives that come from a
packed library are therefore linked whole:

| Platform | Linker input |
| --- | --- |
| Windows (MSVC/lld) | `-Wl,/WHOLEARCHIVE:<archive>` |
| Linux (GNU/LLD) | `-Wl,--whole-archive <archive> -Wl,--no-whole-archive` |
| macOS | `-Wl,-force_load,<archive>` |

Archives passed by hand with `--link` are linked normally, so a consumer keeps
control when it needs to.

## Runtime libraries

When a package ships a shared library (`.dll`, `.so`, `.dylib`), the compiler
links against the matching import library and copies the shared library next to
the produced executable, so the program runs without environment changes.

## Inspecting a package

```text
$ sere slib-info dist/mylib.slib
Name:        mylib
Version:     1.0.0
Entry:       src/__init__.sere
Files:       3
Format:      3 (sha256 per member, zlib members)

Sere modules:
  src/__init__.sere

Native:
  archive   native/build/libsere_native.a
  source    native/math.c
```

```text
$ sere slib-verify dist/mylib.slib
mylib 1.0.0 verified
  Sere modules:    1
  Native archives: 1
```

`slib-verify` re-reads the archive the way a consumer will: format version,
member sizes, path safety, hashes, and duplicate members. It exits non-zero and
prints the reason when a package is damaged.

## Troubleshooting

**`undefined reference to mylib_add`** — the native implementation was not
packed, or was packed but ignored. Run `sere slib-info <file>.slib` and check
that a `Native archives:` line is present. If it is missing, set
`native = true` in `sere.toml` (or put the sources under `native/` next to the
entry file) and pack again.

**The package has no native artifact for this target** — the package shipped a
prebuilt library for another platform only. Ship portable sources as well, or
build a package for this target.

**A link failure mentions symbols from a package** — the compiler prints the
native inputs it used and the archive it took them from. `sere slib-verify`
confirms whether the archive itself is intact.

## Limits

* Native compilation is not cached across builds beyond what the toolchain
  itself caches: changing a package's native sources rebuilds them for the
  consumer.
* Packages are selected by the importing project; there is no per-package
  version conflict resolution yet.
* macOS framework dependencies are not yet expressible in package metadata.

## See also

* [libraries.md](libraries.md) — making and packing a library, shipping tools (`executables`), `system_libs`
* [sere-toml.md](sere-toml.md#build) — `native`, `system_libs`, and `executables`
* [projects.md](projects.md) — applications and the everyday workflow
