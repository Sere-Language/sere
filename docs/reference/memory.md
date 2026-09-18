# Memory and pointers

## Vocabulary

| Type | Meaning | Freed by |
| --- | --- | --- |
| `Unique[T]` | exclusive heap value | automatically at end of scope |
| `Shared[T]` | reference-counted heap value | automatically when the last reference dies |
| `Ptr[T]` | raw pointer | you — via `free()` if you allocated it |
| `list[T]`, `str`, `dict[K, V]`, `class` | runtime-managed values | the runtime |

## Creating and using

```sere
owned: Unique[i32] = unique[i32](42)
*owned = 43
value: i32 = load(owned)

shared_ptr: Shared[str] = shared[str]("hi")

raw: Ptr[i32] = alloc[i32]()
store(raw, 7)
*raw = 8
free(raw)
```

| Intrinsic | Result |
| --- | --- |
| `unique[T](value)` | `Unique[T]` |
| `shared[T](value)` | `Shared[T]` |
| `alloc[T]()` | `Ptr[T]` |
| `load(p)` | `T` |
| `store(p, v)` | `void` |
| `free(p)` | `void` |
| `&x` | `Ptr[T]` for an addressable lvalue |
| `*p` | load `T`; as a target, `*p = v` stores |

Addressing and dereferencing stack storage is ordinary:

```sere
local: i32 = 10
stack: Ptr[i32] = &local
*stack = *owned
```

## Output parameters

Use `Ptr[T]` for a function that writes into a caller's variable. Passing `&x`
is enough — there is no `out` keyword. Declare the variable before the call.

```sere
def get_name(out_name: Ptr[str]) -> void:
    *out_name = "Sere"

def main() -> i32:
    name: str = ""
    get_name(&name)
    print(name)
    return 0
```

The same works for fields, list elements, forwarding to other functions,
callbacks, and native C functions.

`Ptr[str]` points at a Sere string value (data pointer plus length), **not** a
C character buffer. Match the native function's ABI when interoping.

## Pointer rules

- A writable pointer parameter requires the exact pointee type: `Ptr[i32]`
  cannot implicitly become `Ptr[i64]`, `Ptr[Any]`, or a pointer to a base class.
- `Unique[T]` and `Shared[T]` may be borrowed as `Ptr[T]`. Raw pointers never
  implicitly acquire ownership.
- Owning pointers passed as arguments are borrowed for the duration of the call
  and are **not** released on return. Use `Ptr[T]` for functions that only need
  access.
- `free()` is for raw `alloc` allocations. Owning pointers release themselves.
- Taking a mutable address of a constant, or directly returning a local
  variable's address, is rejected at compile time.
- Null dereference through `*`, `load()`, or `store()` raises `RuntimeError`.

Raw pointers still need lifetime discipline. Do **not** keep an address after its
storage goes out of scope, `free()` a borrowed address, or hold an element
pointer across a list resize. These checks are not a borrow checker; explicit
pointer casts remain an escape hatch.

## Garbage collectors

`alloc` / `free` go through the collector installed by `import gc`. The default is
`"none"` — tracked `malloc` that you free yourself.

| Collector | Behaviour |
| --- | --- |
| `"none"` | explicit free; no scanning |
| `"mark_sweep"` | conservative; needs roots |
| `"arena"` | bump allocation; `collect()` resets the heap |

```sere
import gc

gc.use("mark_sweep")
p = alloc[i32]()
gc.add_root(p as Ptr[i8])
gc.collect()
gc.remove_root(p as Ptr[i8])
```

`gc.use(name)` returns `bool`; `gc.name()` reports the installed one and there
are `gc.is_none()` / `gc.is_mark_sweep()` / `gc.is_arena()` predicates.

| Function | Result |
| --- | --- |
| `gc.collect()` | run a collection |
| `gc.add_root(p)` / `gc.remove_root(p)` | root registration for tracing collectors |
| `gc.retain(p)` / `gc.release(p)` | adjust the collector's reference count |
| `gc.bytes_in_use()` / `gc.bytes_allocated()` | counters (`i64`) |
| `gc.live_blocks()` / `gc.collections()` | counters (`i64`) |
| `gc.alloc_bytes(n)` / `gc.free_bytes(p)` | raw byte allocation |

Install the collector **before** the program allocates; switching mid-run can
leak.

### Custom collectors

Implement `SereGcVTable` in C, call `sere_gc_install` from `sere_mod_init`, and
link the object:

```
sere main.sere --link my_gc.lib
```

### Explicit memory

`import heap` provides storage that does not use the active GC: raw byte helpers
(`copy`, `set`, `eq`) and fixed-size pools.

```sere
import heap

arena = heap.arena_new(4096)
block = heap.arena_alloc(arena, 64)
heap.arena_reset(arena)      # bump pointer rewinds, memory reused
heap.arena_destroy(arena)

pool = heap.pool_new(32, 128)   # 128 blocks of 32 bytes
item = heap.pool_alloc(pool)
heap.pool_release(pool, item)
heap.pool_destroy(pool)
```

| Function | Result |
| --- | --- |
| `heap.copy(dest, src, size)` / `heap.set(dest, value, size)` | byte moves |
| `heap.eq(a, b, size)` | byte comparison → `bool` |
| `heap.arena_new(cap)` / `arena_alloc` / `arena_reset` / `arena_destroy` | bump arena |
| `heap.pool_new(block_size, blocks)` / `pool_alloc` / `pool_release` / `pool_destroy` | fixed pool |

## See also

- [bytes.md](bytes.md) — byte buffers and endian readers over `Ptr[i8]`.
- [functions.md](functions.md) — output parameters.
- [../architecture.md](../architecture.md) — the runtime and GC layout.
