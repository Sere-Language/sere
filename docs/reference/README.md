# Sere reference

Deep dives for one subject at a time. Start with
**[../language.md](../language.md)** for the whole language at a glance; these
pages go into the details, edge cases, and the exact methods that exist.

Everything here reflects what the compiler in this repository actually accepts —
if a page and the compiler disagree, the compiler is right and the page is a bug.

| Topic | Page |
| --- | --- |
| Text: literals, indexing, slicing, methods, formatting-free basics | **[strings.md](strings.md)** |
| Ordered collections: literals, mutation, repetition, comprehensions | **[lists.md](lists.md)** |
| Keyed collections: literals, lookup, iteration, ordering | **[dicts.md](dicts.md)** |
| Integers, floats, literals, arithmetic, casts | **[numbers.md](numbers.md)** |
| Bytes and buffers | **[bytes.md](bytes.md)** |
| `f"..."` interpolation and the format mini-language | **[formatting.md](formatting.md)** |
| Arithmetic, comparison, logical, bitwise, `is`, `in` | **[operators.md](operators.md)** |
| `if` / `while` / `for` / `match` / `with` / `defer` | **[control-flow.md](control-flow.md)** |
| Classes, structs, methods, properties, dunders, operator overloading | **[classes.md](classes.md)** |
| Enums, payloads, flags | **[enums.md](enums.md)** |
| `def`, parameters, defaults, varargs, lambdas, decorators | **[functions.md](functions.md)** |
| Type parameters, constraints, unions, `Any` | **[generics.md](generics.md)** |
| `raise`, `try`/`except`/`finally`, custom exceptions | **[exceptions.md](exceptions.md)** |
| `import`, `from ... import`, `__exports__`, project `libs/` | **[modules.md](modules.md)** |
| `Ptr` / `Unique` / `Shared`, `alloc`, `&`, `*`, GC | **[memory.md](memory.md)** |
| `async` / `await`, generators, `yield` | **[async.md](async.md)** |
| Macro definitions and expansion | **[macros.md](macros.md)** |

## Conventions used in these pages

- Examples are complete `main`-based programs unless trimmed with `...`.
- `T` is a placeholder for a concrete type.
- "Method" means a builtin member reached with `.`; user-defined member
  functions are described in [classes.md](classes.md).
- Arity errors are compile-time: the compiler knows each builtin's signature and
  rejects a wrong argument count or type.
