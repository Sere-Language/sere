# Async

`async def`, `await`, and the `Task[T]` / `Future[T]` types parse and type-check.

## Basic shape

```sere
async def number() -> i32:
    return 42

async def combine() -> i32:
    a = await number()
    b = await number()
    return a + b
```

- Calling an `async def` does **not** return its declared type — it returns a
  `Task[T]` where `T` is the declared return type.
- `await` unwraps a `Task[T]` back to `T`.
- `await` may only appear inside an `async def`; using it in a plain function is
  a compile error.
- An `async def` may itself be awaited, or run from an `async main`.

```sere
async def main(argv: list[str]) -> i32:
    print(await combine())
    return 0
```

The result type propagates through `await`, so the usual typing rules apply:

```sere
async def fetch_name() -> str:
    return "sere"

async def greet() -> str:
    name: str = await fetch_name()    # typed str, not Any
    return "hello " + name
```

## Argument typing

Arguments are checked against the declared parameter types, not the task:

```sere
async def scale(value: i32, factor: i32 = 2) -> i32:
    return value * factor

async def use() -> i32:
    return await scale(3, factor=4)    # 12
```

An `async def main` is the entry point for an async program. Error handling is
unchanged — `try` / `except` / `finally` work across `await`, and `defer` bodies
still run while an exception unwinds.

## Not implemented

Some constructs parse but are rejected rather than silently miscompiled:

| Construct | Status |
| --- | --- |
| `yield` / generator functions | not implemented — use a nested `def` or a list |
| keyword-only parameters after `*args` | not implemented |
| `global` / `nonlocal` | not implemented (nested `def` closures work) |
| lambda capture of enclosing locals | not implemented — use a nested `def` |

Incomplete lowering reports `NotImplementedError` with the source location
instead of generating wrong code.

## See also

- [functions.md](functions.md) — `def`, defaults, callable types.
- [exceptions.md](exceptions.md) — error handling.
- [../language.md](../language.md#async--await-status) — reference summary.
