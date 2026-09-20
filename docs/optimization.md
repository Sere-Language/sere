# Optimization

Sere composes its own optimization pipeline instead of calling LLVM's
level defaults. Every switch in the table below selects a real pass, and the
same switches drive three layers:

1. **Sere IR rewrites** over the generated `llvm::Module` — release-mode
   lowering, null-check folding, stack promotion, runtime annotations.
2. **The LLVM pipeline** — composed from the enabled passes, run in rounds.
3. **The native toolchain** — the flags `clang` gets when it compiles the
   emitted `.ll` and links the executable.

Because the rewrites happen before printing, the switches are visible in
`--emit-llvm`, `--emit-serem`, and `--emit-asm`, not only in a linked binary.

## Aggregate levels

| Flag | Meaning |
| --- | --- |
| `-O0`, `--O0`, `--opt=O0` | No middle-end optimization. The generator already constant-folded literals and dropped unreachable blocks |
| `-O1`, `--O1` | Inline, constant fold, dead code, peephole, CSE, strength reduction, LICM, branch opt, tail calls |
| `-O2`, `--O2` | `-O1` plus loop unrolling and vectorization |
| `-O3`, `--O3` | `-O2` plus always-inline and three rounds of the function pipeline |
| `-Os`, `--Os` | `-O2` without loop unrolling or always-inline |
| `-Oz`, `--Oz` | Size first: peephole, constant fold, dead code, branch opt only |
| `-O`, `--O` | Shorthand for `-O2` |

`--opt <level>` and the project manifest key `opt = "O2"` do the same thing. A
level sets a whole switch set, so per-pass switches written *before* a level are
replaced by it and switches written *after* it override it. Order matters:

```bash
sere -O3 --no-inline-all app.sere     # O3 without always-inline
sere --cse -O1 app.sere               # the level wins; --cse is redundant
```

## Bundles

| Flag | Effect |
| --- | --- |
| `--release` | `-O3`, every runtime check off, `--fast-math`, `--stack-alloc` |
| `--debug` | `-O0` with every check on and no allocation rewriting |

`--release` is the switch to reach for when the program is correct and speed or
size matters. `--debug` restores the checked behavior without needing to name
every `--no-…` counterpart.

## Per-pass switches

Every switch has an inverse; the inverse is what to use when a preset enabled
something you do not want.

| Enable | Disable | What it adds to the pipeline |
| --- | --- | --- |
| `--inline`, `--inline-expansion` | `--no-inline` | The module inliner plus `deadargelim` and `function-attrs` |
| `--inline-all` | `--no-inline-all` | Marks every non-entry, non-coroutine function `alwaysinline`, then runs `always-inline` |
| `--const-fold`, `--constant-fold` | `--no-const-fold` | `globalopt`, `ipsccp`, `sccp`, `float2int` |
| `--dead-code`, `--dce` | `--no-dead-code` | `adce`, `dce`, then `globaldce` and `strip-dead-prototypes` |
| `--peephole`, `--instcombine` | `--no-peephole` | `instcombine`, `dse`, `memcpyopt`, `mergeicmps` |
| `--cse`, `--gvn` | `--no-cse` | `early-cse<memssa>`, `gvn`, `gvn-hoist` |
| `--strength-reduce` | `--no-strength-reduce` | `aggressive-instcombine`, `reassociate`, `div-rem-pairs`, `constraint-elimination` |
| `--loop-unroll`, `--unroll-loops` | `--no-loop-unroll` | `loop-unroll` |
| `--loop-invariant-hoist`, `--hoist`, `--licm` | `--no-loop-invariant-hoist` | `loop-simplify`, `lcssa`, `loop-rotate`, `loop-idiom`, `licm`, `loop-deletion`, `loop-instsimplify` |
| `--vectorize` | `--no-vectorize` | `loop-vectorize`, `slp-vectorizer`, `vector-combine` |
| `--branch-opt`, `--branch-optimize` | `--no-branch-opt` | `simplifycfg`, `correlated-propagation`, `jump-threading`, `tailcallelim` |
| `--tailcalls`, `--tail-calls` | `--no-tailcalls` | `tailcallelim` plus `disable-tail-calls=false` on every definition |
| `--fast-math` | `--no-fast-math` | `unsafe-fp-math`, `no-nans-fp-math`, `no-infs-fp-math`, `no-signed-zeros-fp-math`, `fp-contract=fast` on every definition, `fast` on every floating-point instruction, and `-ffast-math -fno-math-errno -fno-trapping-math -fno-signed-zeros -ffp-contract=fast` for `clang` |
| `--lto` | `--no-lto` | LLVM's prelink pipeline and `-flto` for `clang` |

A per-pass switch also means "build the pipeline from switches". That is the
only difference from a bare level.

## Release-mode lowering

These switches change what the generated IR *checks*, so they are the ones that
change program behavior. Use them on a program that is already correct.

| Flag | Inverse | What disappears |
| --- | --- | --- |
| `--no-runtime-checks` | `--runtime-checks` | Every guard the compiler inserted: calls to `sere_has_error` fold to `false`, `sere_clear_error`/`sere_release_current_error` calls are erased, the blocks they guarded become unreachable and are deleted, and blocks whose only purpose is to call `sere_panic` are removed |
| `--no-bounds-checks` | `--bounds-checks` | Index checks: `sere_list_item`, `sere_str_index`, `sere_dict_get`, and friends are declared `nounwind` and `willreturn` so the middle end can hoist, CSE, and vectorize them |
| `--no-null-checks` | `--null-checks` | Null tests: allocation results are declared `nonnull`, so `icmp eq ptr %p, null` folds to `false` and the branch disappears. `sere_alloc`, `sere_list_new`, `sere_dict_new`, and the other allocations also get `noalias` and `allocsize(0)` |
| `--stack-alloc` | `--no-stack-alloc` | Non-escaping `sere_alloc` calls with a constant size become an `alloca` in the entry block. A pointer escapes if it is stored into memory, returned, passed to a call, or converted to an integer. `SERE_STACK_ALLOC_MAX` (default 1 MiB) caps the promoted size |
| `--arena-alloc` | `--no-arena-alloc` | Per-block `sere_free`/`sere_gc_free` calls are erased, so memory is reclaimed when the collector runs instead of block by block |

`--no-runtime-checks` implies all three check switches. The effects are visible
immediately: `sere --emit-llvm --no-runtime-checks app.sere` has no `err` blocks
and no `sere_has_error` call.

## How the pipeline is composed

The composed module pipeline is, in order:

```text
globalopt, ipsccp                     (when const-fold)
always-inline                         (when inline-all)
inline, ipsccp, function-attrs,
deadargelim, globaldce,
strip-dead-prototypes                 (when inlining or dead-code)
function(                             one round, repeated
  sroa, mem2reg,                        for O1+, twice more for O3
  sccp,
  instcombine, dse, memcpyopt, mergeicmps,
  early-cse<memssa>,
  aggressive-instcombine, reassociate,
  div-rem-pairs, constraint-elimination,
  gvn, gvn-hoist,
  simplifycfg, correlated-propagation, tailcallelim,
  float2int,
  loop-simplify, lcssa, loop-rotate, loop-idiom, licm,
  loop-deletion, loop-instsimplify,
  loop-unroll,
  loop-vectorize, slp-vectorizer, vector-combine,
  instcombine, tailcallelim, adce, dce)
cgscc(jump-threading, simplifycfg,
      instcombine, adce)              (when branch-opt)
globaldce                             (when dead-code)
```

The coroutine pipeline (`coro-early,coro-split,coro-cleanup`) always runs before
this, because an unsplit coroutine cannot be inlined or cloned.

`--passes=<pipeline>` replaces the composed pipeline with a PassBuilder string.
`--lto` replaces it with LLVM's prelink pipeline, which is what `-flto` expects.

## Runtime annotations

`annotateRuntimeDeclarations` runs on every compile, whatever the level, because
the promises are true of the runtime itself:

- Every `sere_*` declaration is `nounwind` — the runtime is C and cannot unwind
  through a Sere frame.
- Pure readers (`sere_list_len`, `sere_dict_len`, `sere_str_eq`,
  `sere_str_cmp`, …) are `readonly` and `willreturn`, so repeated calls collapse
  into one.
- Fresh allocations are `allocsize(0)`, which lets LLVM fold two allocation
  sizes into one and know how large the block is. With `--no-null-checks` (or
  `--no-runtime-checks`) they additionally return `nonnull`, `noalias`, and
  `noundef` memory, which is what lets the null-check folding fire.
- Accessors whose range check can fire (`sere_list_item`, `sere_str_index`, …)
  are not promised anything while `--bounds-checks` is on.

## Native toolchain flags

The IR pipeline optimizes the module; `clang` still has to turn it into machine
code. Sere passes the level through and adds:

| Flags | When |
| --- | --- |
| `-O0`…`-O3`, `-Os`, `-Oz` | Always, matching the level |
| `-fomit-frame-pointer`, `-fno-asynchronous-unwind-tables`, `-fno-unwind-tables`, `-march=native`, `-mtune=native` | `O1` and above |
| `-ffast-math`, `-fno-math-errno`, `-fno-trapping-math`, `-fno-signed-zeros`, `-ffp-contract=fast` | `--fast-math` |
| `-funroll-loops` | `--loop-unroll` |
| `-fvectorize`, `-fslp-vectorize` | `--vectorize` |
| `-fwrapv` | `--no-runtime-checks` with `--no-bounds-checks` |
| `-flto` | `--lto` |

`-march=native` means a binary is tuned for the machine that built it. That is
the right default for `sere build` on a developer machine and the wrong one for
a binary you intend to ship, where `-O2 -Oz` style settings are safer.

## Seeing what happened

| Tool | Purpose |
| --- | --- |
| `SERE_OPT_REPORT=1` | Prints a one-line report per compile: how many attributes were added, how many null checks, error checks, and panic blocks were removed, how many allocations moved to the stack, how many frees were elided |
| `--dump-llvm-ir-raw` | Writes `sere-raw-before-pipeline.ll`: the module before any rewrite, for comparison |
| `SERE_DUMP_RAW_IR=1` | Writes `sere-coro-before.ll`: the module as the coroutine pipeline sees it |
| `--emit-llvm` | The optimized module, after every rewrite |
| `--emit-serem` | The Serem module, after the Serem passes |
| `--emit-asm` | Machine code, for measuring what the backend produced |

```bash
SERE_OPT_REPORT=1 sere --emit-llvm -O3 --release app.sere -o app.ll
sere --dump-llvm-ir-raw --emit-llvm app.sere -o app.ll
```

## Serem-side passes

`--emit-serem` and `--backend=serem` run the Serem pipeline described in
[serem.md](serem.md). The switches that apply there are the same ones:
`--cse` adds Serem common-subexpression elimination, `--peephole` and
`--strength-reduce` add identity folding and shift/mask rewriting, and
`--no-runtime-checks` strips the `error.*` machinery. `--release` enables all
three.

## Adding a switch

1. Add the field to `OptimizationOptions` in
   `include/sere/codegen/OptPipeline.h`, with a doc comment.
2. Set it in `optimizationPreset` if a level should turn it on.
3. Handle the flag in `parseOptimizationFlag`, including its `--no-` inverse.
4. Use it in `buildFlagPipeline` (the LLVM pipeline) or in
   `SeremTransform.cpp::transformPasses` (the Serem pipeline), or in
   `OptPasses.cpp` if it is a rewrite over the generated module.
5. Add it to `clangCodegenFlags` if the backend needs to know.
6. Cover it in `tests/opt_options.cpp` and add a CLI test in
   `tests/CMakeLists.txt`.
