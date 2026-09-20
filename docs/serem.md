# Serem: the target-independent SSA IR

Serem is Sere's own SSA intermediate representation. It sits between the typed
AST and LLVM IR, it is fully inspectable as text, and it is defined by Sere's
own model — not by an LLVM version. `--emit-serem` shows it, `--backend=serem`
compiles through it, and the optimizer can rewrite it before either backend
runs.

```
.sere source
    │
    ▼
 TypeChecker (typed AST)
    │
    ├─ IRGenerator ─────────────────► llvm::Module ─┐
    │                                              │
    └─ SeremGenerator ──► Serem IR ─┬─ SeremTransform (opt passes)
                    (--emit-serem)  │
                                    └─ SeremLLVMBackend ──► llvm::Module ─┤
                                                                         ▼
                                                opt pipeline ──► .ll / .s / exe
```

Everything below the typed AST goes through the same optimization pipeline, so
`--emit-serem`, `--emit-llvm`, `--emit-asm`, and a linked executable all agree
about which switches are on.

## Why a second IR

| Reason | Consequence |
| --- | --- |
| Every rewrite pass reads and writes a Sere-owned data structure | A pass cannot break on an LLVM upgrade, and value types carry Sere meaning (`str`, `Any`, boxed records) instead of opaque pointers |
| The module is printable text | `--emit-serem` is a debugging tool that needs no LLVM knowledge, and the printed text is what the passes produced |
| The optimizer runs before any target decision | `--no-runtime-checks`, `--cse`, and `--strength-reduce` change the visible IR rather than a hidden internal state |
| The backend is replaceable | `SeremLLVMBackend` is one implementation of an interface, not the IR itself |

## Text format

The printed form is stable and line oriented. A module is a header, then its
type definitions, globals, and functions, in that order.

```text
module @test
type @Exception = %Exception [class, field=message:1]
global @str.0 = str "Hello world!"
func @main() -> i32 {
entry:
  runtime.print @str.0
  return 0
}
```

| Element | Syntax | Notes |
| --- | --- | --- |
| Module header | `module @<name>` | The name is the source file stem |
| Type definition | `type @<name> = <type> [<attr>, …]` | Attributes like `class` and `field=message:1` mark records and their field slots |
| Global | `global @<name> = <type> <value>` | String literals become `str "..."` globals so a rewrite can see them |
| Function | `[extern ][async ][generator ]func @<name>(%arg0: <type>, …) -> <type> [<attr> = <value>, …]` | An `extern` function prints on one line and has no body |
| Block | `<label>:` followed by two-space-indented operations | The entry block is always labelled `entry` |
| Operation | `[%<result> = ]<opcode>[ <type>][ <operand>, …][ {<attr> = <value>, …}]` | A `void` result prints no type; attributes are space separated inside braces |

Value references:

| Value | Prints as |
| --- | --- |
| Integer constant | `42` (wrapped to the result width when folded) |
| Float constant | The value at 17 significant digits |
| String constant | `"..."`, or `@global` when the literal has a named global |
| Argument | `%arg0` |
| Function reference | `@name` |
| Operation result | `%name` |

## Type model

`IRType` is a value type — cheap to copy and compare — with these kinds:

| Kind | Prints as | Meaning |
| --- | --- | --- |
| `Void` | `void` | No value |
| `Bool` | `i1` | A boolean |
| `I8`, `I16`, `I32`, `I64` | `i8`, `i16`, `i32`, `i64` | Signed integers |
| `U8`, `U16`, `U32`, `U64` | `u8`, `u16`, `u32`, `u64` | Unsigned integers; the strength-reduction pass uses the distinction |
| `F32`, `F64` | `f32`, `f64` | Floating point |
| `Ptr` | `Ptr[T]` | Pointer to `T`; the pointer value *is* the address |
| `Array` | `[N x T]` | Fixed-length aggregate |
| `Function` | `(T, U) -> R` | A callable signature |
| `Struct` | `%Name` | A named record |
| `Label` | `label` | A branch target |
| `String` | `str` | A Sere string (pointer plus length at the LLVM level) |

Two helper contracts live next to the types because both the generator and the
backend must agree on them without exchanging data out of band:

- `ListElementKind` — the numeric tag stored with a list so the backend knows
  its element layout and the runtime formatter knows how to print an element.
- `recordTypeId(name)` — FNV-1a hash of a record name, stored in the first word
  of every class value so a union can answer `x is Mayor` for a member it does
  not list.

## Instruction set

Serem opcodes are strings, so a new operation does not need a new enum value.
`IRBuilder` covers the common shapes; the generator may also emit a
dialect-specific opcode directly through `operation()`.

### Arithmetic, logic, and values

| Opcode | Result | Notes |
| --- | --- | --- |
| `add`, `sub`, `mul`, `div`, `rem` | Integer | `div`/`rem` are signed unless the type is unsigned |
| `fadd`, `fsub`, `fmul`, `fdiv` | Float | |
| `and`, `or`, `xor` | Integer | Bitwise |
| `shl`, `shr` | Integer | `shr` is arithmetic at the LLVM level, which is why the strength-reduction pass only rewrites *unsigned* division into a shift |
| `neg` | Integer/float | Unary minus |
| `not`, `invert` | Integer | Logical/binary negation |
| `cmp.<pred>` | `i1` | `<pred>` names the comparison (`eq`, `ne`, `lt`, `slt`, `ult`, …) |
| `cast.<kind>` | Any | `<kind>` names the conversion |
| `select` | Any | `select <cond>, <a>, <b>` |
| `phi` | Any | `phi <type> <incoming>, …`; incoming pairs carry their predecessor in attributes |
| `alloca`, `load`, `store` | Ptr/Void | Stack slot, load, store |
| `get_element`, `extract`, `insert` | Any | Aggregate access |
| `deref`, `address.of`, `store.indirect` | Ptr/Void | Pointer vocabulary |
| `pointer.null`, `pointer.is_null` | Ptr/`i1` | Null literal and null test |

### Aggregates, strings, and runtime calls

| Opcode | Notes |
| --- | --- |
| `aggregate.list`, `aggregate.array`, `aggregate.range` | Build a list, an array, or a range; the element kind travels in an attribute |
| `string.concat` | Concatenate two Sere strings |
| `contains` | Membership test |
| `runtime.print` | Write a value to stdout |
| `runtime.input` | Read a line |
| `runtime.len` | Length of a collection or string |
| `value.repr` | Render a value the way `repr()` prints it; the element kind travels in an attribute |
| `builtin.method` | A lowered built-in call such as `list.append`; the lowered name is an attribute |
| `construct`, `member.get`, `member.set` | Record construction and field access |
| `enum.tag`, `enum.payload` | Enum tag and payload access |
| `union.pack`, `union.extract`, `union.is`, `object.isa` | Union and dynamic-type tests |
| `iter.begin`, `iter.has_next`, `iter.next` | Iterator protocol |
| `assert` | Compiler-generated assertion |
| `sere.expression`, `sere.statement` | Escape hatches that carry a source expression or statement the Serem lowering does not model |
| `binary.dynamic`, `unary.dynamic`, `assign.dynamic`, `decorated.call`, `destroy`, `throw`, `index.address`, `index.set`, `shared.new` | Dynamic dispatch, decorators, destruction, exceptions, indexing, and shared boxes |

### Control flow

| Opcode | Notes |
| --- | --- |
| `branch` | Unconditional; the target travels in the `target` attribute |
| `cond_branch` | Conditional; `true` and `false` attributes name the two successors |
| `return` | `return` or `return <value>` |
| `unreachable` | Cannot be reached; usually the arm of a check that was eliminated |

### Exceptions

| Opcode | Notes |
| --- | --- |
| `error.enter` | Save the pending error so a `finally` body starts clean |
| `error.leave` | Restore or release the pending error; `restore` is an attribute |
| `error.isa` | Test the pending error against a type named in the `type` attribute |
| `error.bind` | Bind the caught value out of the pending error; the field slot is an attribute |
| `throw` | Raise |

`--no-runtime-checks` removes `error.enter`, `error.leave`, and `error.bind`,
and rewrites every `error.isa` to `false`. The handler blocks then become
unreachable and the unreachable-block pass deletes them, so a release build
carries no exception machinery at all.

### Coroutines and async

| Opcode | Notes |
| --- | --- |
| `coro.begin` | Start a generator or async function; the element type is the result |
| `coro.suspend`, `coro.end`, `yield` | Suspend, finish, and produce a value |
| `await` | Suspend until a task completes |
| `async.create`, `async.resume`, `async.destroy` | Task creation and driving |
| `coro.resume`, `coro.done`, `coro.promise`, `coro.destroy` | Coroutine handle operations |

## Optimization passes

`SeremTransform.h` defines `TransformPass`: a named rewrite that reports whether
it changed the module. The driver iterates the selected passes to a fixed point
(bounded by eight rounds, since every pass is monotone).

| Pass | Runs when | What it does |
| --- | --- | --- |
| `constant-fold` | Always | Folds arithmetic, comparisons, casts, `select`, and branches whose operands are all constants; a constant `cond_branch` becomes a `branch` and the dead edge disappears |
| `dead-code` | Always | Keeps only what the entry point can reach, following call operands *and* attribute references, because a callee can be named in an attribute |
| `unreachable-blocks` | Always | Drops blocks no branch can reach |
| `unused-globals` | Always | Drops string literals nothing references any more |
| `strength-reduce` | `--peephole` or `--strength-reduce` | `x+0`, `x*1`, `x|0`, `x^0`, `x&-1`, `x-0` become `x`; `x*0`, `x-x`, `x^x` become `0`; `x*8` becomes `x<<3`; unsigned `x/8` becomes `x>>3`; unsigned `x%16` becomes `x&15` |
| `cse` | `--cse` | Replaces a repeated pure computation in the same block with the first result; arithmetic, compares, casts, `select`, and aggregate reads are candidates |
| `runtime-checks` | `--no-runtime-checks` | Removes the pending-error bookkeeping and turns the handler dispatch into `false` |

Every pass is independent: none of them knows about the others, and adding one
means writing a class and putting it in `transformPasses()` (or in
`defaultTransformPasses()` if it should always run).

## Backend

`SeremLLVMBackend` lowers a Serem module to `llvm::Module`:

| Serem | LLVM |
| --- | --- |
| `add`, `sub`, `mul`, `and`, `or`, `xor`, `shl` | The matching `IRBuilder` instruction |
| `div`, `rem` | Signed or unsigned division depending on the integer type |
| `shr` | `ashr` |
| `cmp.<pred>` | `icmp`/`fcmp` |
| `neg`, `not`, `invert` | `CreateNeg`, `CreateNot` |
| `alloca`, `load`, `store`, `deref` | `alloca`, `load`, `store` |
| `branch`, `cond_branch`, `return`, `unreachable` | The matching terminator |
| `call`, `invoke` | `call`; the callee is a `FunctionRef` |
| `coro.*`, `await`, `yield` | The LLVM coroutine intrinsics, split by `coro-early,coro-split,coro-cleanup` |
| everything else | A runtime call or an inline expansion, exactly as the LLVM generator would do |

The lowered module then enters the same optimization pipeline as a module
produced by `IRGenerator`, so `--backend=serem` and the default backend share
every optimization switch.

## Using it

| Goal | Command |
| --- | --- |
| Print the Serem IR | `sere --emit-serem app.sere -o app.serem` |
| Print the stable bytecode text | `sere --emit-serem-bytecode app.sere` |
| Compile through Serem to an executable | `sere --backend=serem app.sere -o app.exe` |
| See the IR after the release rewrites | `sere --emit-serem -O3 --release app.sere` |
| Skip the passes entirely | `sere --emit-serem --no-transformers app.sere` |

`--no-transformers` prints what the generator produced, before any rewrite, which
is the right starting point when a pass looks wrong.

## Safety notes

- Values are shared pointers. Replacing an operation means rewriting every
  operand that points at it (that is what `applyReplacements` does) and then
  removing it from its block; editing a block in place without doing both leaves
  the printer and the backend disagreeing.
- A block is a straight-line list of operations plus a terminator flag. Passes
  that change control flow must keep `isTerminated()` accurate.
- `removeFunction` and `removeBlock` must not run while a walk over the module
  is in progress; the dead-code pass collects names first and removes second.
