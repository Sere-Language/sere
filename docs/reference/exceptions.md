# Exceptions

## Hierarchy

Every exception derives from `Exception`. The prelude defines:

`Exception`, `SyntaxError`, `IndentationError`, `NameError`, `AttributeError`,
`TypeError`, `IndexError`, `ImportError`, `ValueError`, `AssertionError`,
`PermissionError`, `RuntimeError`, `RecursionError`, `NotImplementedError`.

These are runtime classes you can subclass, `raise`, and catch. The compiler
reuses the same names as diagnostic codes (`error[TypeError]: ...`) for
compile-time problems — a compile-time error is a build failure, not something
`except` can catch.

Defining a subclass is a normal class definition:

```sere
class Boom(ValueError):
    pass

raise Boom("bad")
```

`except T` matches `T` and every subclass, so `except Exception` catches
everything that derives from the base.

## `try` / `except` / `else` / `finally`

```sere
try:
    value = parse(text)
except ValueError as e:
    print("bad input:", e.message)
except RuntimeError:
    print("known failure")
except:
    print("anything else")
else:
    print("no error")
finally:
    print("always runs")
```

- A `try` needs at least one `except` or a `finally`.
- Clauses are checked top to bottom; the first whose type matches runs.
- `as e` binds the instance; `e.message` is the message.
- A bare `except:` catches every exception and must come last.
- `else` runs only when the body completed without an exception.
- `finally` always runs — including on `return`, `break`, `continue`, and while
  an exception is unwinding.

Exceptions propagate out of nested calls and loop bodies until a handler or the
top level. `defer` bodies registered in a function still run while an exception
unwinds it.

## Raising

```sere
raise TypeError("nope")     # with a message
raise Boom("bad")           # a subclass
raise TypeError             # empty message
raise "boom"                # becomes Exception("boom")
raise                       # re-raise the exception being handled
```

The first constructor argument is stringified into the message. A bare `raise`
inside an `except` block re-raises the exception currently being handled.

## Catchable failures

| Source | Behaviour |
| --- | --- |
| `raise` | catchable |
| `assert` failure | `AssertionError`, catchable |
| `todo!()` / `unreachable!()` | `NotImplementedError`, catchable |
| null dereference via `*`, `load`, `store` | `RuntimeError`, catchable |
| `xs[i]` out of range | fatal: prints `error: sequence index out of range` and exits with status 1 |
| missing dict key | fatal, same as an out-of-range index |
| integer division by zero | undefined — LLVM `sdiv` traps; guard the divisor |
| `@private` member reached from outside | compile-time `PermissionError` |
| macro expansion / call depth cap | `RecursionError` |

Only the rows marked catchable can be handled with `except`. The fatal rows are
checked at the point of use, so range- and key-check before indexing when the
value comes from outside the program:

```sere
if i >= 0 and i < len(xs):
    v = xs[i]
else:
    print("out of range")

value = table.get(key, "default")
```

`panic("msg")` still aborts the process — it is not catchable.

## Assertions and prelude helpers

```sere
assert total > 0
assert total > 0, "total must be positive"

todo!("not yet")       # raises NotImplementedError
unreachable!()         # raises if reached
dbg!(total)            # prints the value and yields it
```

`assert` is removed in optimized builds, so it is not for validating user input
that production must handle.

## Custom exception data

Exception classes are ordinary classes, so they can carry fields and methods:

```sere
class HttpError(Exception):
    status: i32

    def __init__(self, status: i32, message: str) -> void:
        self.status = status
        self.message = message
```

## See also

- [control-flow.md](control-flow.md) — `defer`, `assert`.
- [classes.md](classes.md) — inheritance and constructors.
- [modules.md](modules.md) — `PermissionError` from `@private` exports.
