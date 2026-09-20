# Backend: codegen, optimize, link, runtime

## IR generator

`IRGenerator` (`include/sere/codegen/IRGenerator.h`) takes a typed `Module` plus
imported modules and builds an `llvm::Module`.

Typical responsibilities:

- Lower `Type*` to `llvm::Type*` (`lower`).
- Declare functions, including `extern "C"` names and generic instantiations.
- Emit module init for globals.
- Lower statements and expressions.
- Call into the C runtime (`sere_alloc`, `sere_print_str`, `sere_list_push`, …).
- Wrap user `main` as C `main`.
- Drop unique pointers at end of scope (`emitDrops`).

Pointer lowering:

- `emitAddress` for `&` and assignment targets.
- Unary `Deref` loads; the pointer value *is* the address of the pointee.
- `Unique` / `Shared` / `Ptr` share a pointer representation at the LLVM level
  with different drop/retain rules.

Do **not** put new language diagnostics in codegen. If a program can reach
codegen, it should already be well-typed. Use `DiagnosticEngine` only for
internal “this should be unreachable” failures.

## Optimization

Sere composes its own LLVM pipeline rather than calling LLVM's level defaults,
so every switch in `OptimizationOptions` selects a real pass.

| File | Responsibility |
| --- | --- |
| `include/sere/codegen/OptPipeline.h` | `OptimizationOptions`, the level presets, the flag parser, `runOptPipeline`, `clangCodegenFlags` |
| `lib/codegen/OptPipeline.cpp` | The composed pipeline, the presets, the report |
| `include/sere/codegen/OptPasses.h` | The rewrites over the generated module |
| `lib/codegen/OptPasses.cpp` | Runtime annotations, null-check folding, check-block and error-state removal, stack promotion, free elision, fast-math and tail-call attributes |
| `lib/codegen/SeremTransform.cpp` | The same switches applied to the Serem IR |

`runOptPipeline(module, options, error)` runs, in order:

1. `runPrePipelinePasses` — the Sere IR rewrites (`OptPasses.cpp`).
2. The coroutine pipeline, `coro-early,coro-split,coro-cleanup`.
3. The composed module pipeline, repeated function rounds included.

The default level is `O0`, so a plain `sere build` stays a debugging build until
the project manifest sets `opt = "O2"` or the command line passes `-O2`,
`--release`, or an individual switch.

[docs/optimization.md](optimization.md) is the full reference: every flag, what
it adds to the pipeline, what it changes in the emitted IR, and what it passes to
`clang`. [docs/serem.md](serem.md) covers the Serem side.

## Serem

Serem is the target-independent SSA IR behind `--emit-serem`, `--emit-serem-bytecode`,
and `--backend=serem`. Its model is in `include/sere/codegen/Serem.h`: every
constant, argument, function reference, and instruction is a `Value`, `IRBuilder`
owns insertion into function blocks, and `IRModule` owns types, globals, and
functions. The generic `operation()` API accepts dialect-specific opcodes, which
is how the generator emits language features the core hierarchy does not name.

The pipeline is:

```
typed Module ─► SeremGenerator ─► Serem IR ─► SeremTransform ─┬─► .serem text
                                                              └─► SeremLLVMBackend ─► llvm::Module
```

`SeremTransform.h` exposes `TransformPass`; the passes are constant folding,
dead code, unreachable blocks, unused globals, strength reduction, common
subexpression elimination, and runtime-check removal. The driver runs the set
the optimization switches select, iterating to a fixed point.

The LLVM path and the Serem path meet at the same `llvm::Module` optimization
pipeline, so `--backend=serem` honors every switch `--emit-llvm` does.

[docs/serem.md](serem.md) documents the text format, the type model, and the full
instruction set.

## Linking

`compileInput` writes a temp `.ll`, then invokes the pinned `clang` with
`lld` (`-fuse-ld=lld`) and `sere_rt.lib`.

On Windows the runtime also links `user32`, `gdi32`, `opengl32`, `shell32`,
`advapi32`. Importing `qt6` adds `sere_qt6` when CMake found Qt6.

`--link extra.lib` appends extra native libraries. Use this for custom GC
implementations and C extension modules. Importing a `.slib` or a folder
library also links native objects next to it (and compiles loose `.c` /
`.cpp` when needed).

Clang and `sere_rt` are found next to the compiler (see `Toolchain.h` /
`findClang`, `findRuntimeLibrary`). Users compiling Sere programs do **not**
need `scripts/env.ps1`. That script is only for **building sere itself**.

## C runtime

`runtime/` is C (plus optional `sere_qt6.cpp`). Keep the ABI in headers:

| Header | Role |
| --- | --- |
| `runtime/sere_rt.h` | Strings, lists, dicts, alloc, print, sys helpers |
| `include/sere/api/sere_mod.h` | Boxed objects and `Sere_DefineFunction` |
| `include/sere/api/sere_gc.h` | Pluggable collector vtable |

Object files:

| File | Typical contents |
| --- | --- |
| `sere_rt.c` | Core heap, strings, lists |
| `sere_gc.c` | Builtin collectors (`none`, `mark_sweep`, `arena`) |
| `sere_mod.c` | Native module registry |
| `sere_stdlib.c` | Extra stdlib C helpers |
| `sere_sys.c` | Process / env |
| `sere_re.c` | Regex |
| `sere_win.c` / `sere_gl.c` | Platform / OpenGL 2.1+ (WGL, shaders, buffers, textures, FBO) |
| `sere_qt6.cpp` or `sere_qt6_stub.c` | Qt widgets or a stub |

New runtime functions: declare in `sere_rt.h` (or a focused header), implement
in the matching `.c`, then bind from Sere with:

```sere
extern "C" "sere_io_read_line"
def read_line() -> str
```

## Garbage collection

Default collector name is `"none"`: `alloc` is tracked malloc; you `free`.

```sere
import gc
gc.use("mark_sweep")   # or "arena"
p = alloc[i32]()
gc.add_root(p as Ptr[i8])
gc.collect()
```

Custom collector: implement `SereGcVTable`, call `sere_gc_install` from
`sere_mod_init`, link with `--link`. Install **before** the program allocates.
Arenas and pools for explicit regions live in `import heap`.

## Native modules

`include/sere/api/sere_mod.h`:

```c
static Sere_Object* add(Sere_Object* const* args, int32_t nargs) { ... }

extern "C" void sere_mod_init(void) {
  Sere_DefineFunction("add", add, 2);
}
```

```text
sere src/main.sere --link libs/native.lib
```

The simpler path for typed C functions is `extern "C" "symbol"` as in
`examples/native_add.sere`, which calls the symbol directly instead of the
boxed `Sere_Object` API.
