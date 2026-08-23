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
| `windows`, `gl`, `qt6` | Native UI / graphics (see [OpenGL](#opengl-gl) below) |
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

## OpenGL (`gl`)

`stdlib/gl.sere` is the typed wrapper. C lives in `runtime/sere_gl.c`
(WGL + OpenGL 2.1+). Windows hosts get a real context. Elsewhere
`gl.available()` is `False` and window/GL calls fail closed (`0` / `""` /
`False`). The linker always pulls `opengl32` on Windows
([backend.md](backend.md)).

Prefer the classes and helpers. The `_window_*` / raw `gl*` `extern "C"`
names are the ABI; they are not the intended app API.

| Surface | Role |
| --- | --- |
| `available()` | Host has a usable WGL/GL context |
| `Window(title, w, h, flags=0)` | Native window + context. Loop: `poll` / `should_close` / `swap` |
| `Color` / `rgb` / `rgba` / `color_u32` | Clear and immediate-mode colors (`apply`, `lerp`, `to_u32`) |
| `program(vert, frag)` | Compile/link a `Program` from GLSL strings (`ok()`, `log()`, `use()`, `set_mat4`) |
| `Mesh(vertices, floats_per_vertex)` | VBO/VAO. `layout(index, size, offset_floats)`, `draw(Primitive)` |
| `Texture` / `Framebuffer` / `Renderbuffer` | Offscreen color/depth targets |
| `GpuBuffer` / `VertexArray` / `Shader` | Lower-level objects if you skip `Mesh` |
| Enums | `Primitive`, `Key`, `Mouse`, `WindowFlag`, `ShaderKind`, `Capability`, … |

Window extras that landed with the expanded bindings: `should_close` /
`close`, `key_pressed`, `set_vsync`, `set_cursor` / `CursorMode`,
`fit_viewport`, `dt` / `time`, clipboard, fullscreen, and `WindowFlag`
(`Visible`, `Resizable`, `Decorated`, `Maximized`, `Floating`, `Focused`,
`Fullscreen`, `Vsync`). `default_window_flags()` is visible + resizable +
decorated + focused + vsync.

Shaders in the samples are **GLSL 1.20** (`#version 120`, `attribute` /
`varying`). `Mesh` vertices are `list[f64]`; `layout` offsets are in
floats, not bytes. Pair cameras with `import matrix` (row-major
`list[f64]` of 16). Examples: `examples/gl_info.sere` (enums / color
math, no window) and `examples/gl_triangle.sere` (window + mesh).

Always `destroy()` GPU objects and the window before `return`. Gate the
host with `if not gl.available(): return 0`.

## Project layout vs stdlib

`sere init` creates a project with its own `src/` and `libs/`. User modules
resolve relative to the importing file. Publish reusable code with
`sere init-lib` + `sere pack` as a single `.slib` (reachable sources plus
compiled native objects). Drop that file into a project's `libs/` and
`import` it. A folder `libs/mylib/` with `lib.sere` or `mylib.sere` (and
optional C sources) is the same import without packing. Loose `.sere` files
on the import path still work. Neither belongs in `stdlib/` unless it is
part of the language distribution. Commands and `sere.toml`:
[projects.md](projects.md).
