# Functions

## Declaration

```sere
def scale(value: i32, factor: i32 = 2) -> i32:
    return value * factor
```

The type after `->` may be omitted. The inferred result depends on the name:

| Declaration | Inferred return |
| --- | --- |
| `def main(...)` | `i32` |
| `def __init__(...)` | `void` |
| any other `def` | `Any` |

Parameter types may also be omitted, in which case the parameter is `Any`. Add
`: T` whenever the body needs the parameter typed — an untyped parameter is
`Any` and most operations on it are only checked at runtime.

A function with no `return` and a non-`void` return type is an error, not an
implicit `null`.

## Return

```sere
def classify(n: i32) -> str:
    if n < 0:
        return "negative"
    return "non-negative"
```

`return` with no value is only valid in a `void` function. `(a, b)` returns a
multi-value result; unpack it at the call site with `x, y = pair()`.

## Parameters

| Form | Example | Call site |
| --- | --- | --- |
| Positional | `def f(a: i32, b: i32)` | `f(1, 2)` |
| Default | `def f(a: i32 = 2)` | `f()` |
| Variadic | `def f(*parts: list[str])` | `f("a", "b")` |
| Keyword bag | `def f(**opts: dict[str, str])` | `f(sep=",")` |
| Output pointer | `def f(out: Ptr[i32])` | `f(&local)` |

Defaults must come after required parameters. Keyword arguments may be passed in
any order and may be mixed with positional ones:

```sere
def make(width: i32, height: i32 = 10, filled: bool = False) -> str:
    return f"{width}x{height}"

print(make(1, 2))
print(make(height=4, width=3))
print(make(1, filled=True))
```

`*parts` collects the remaining positional arguments into a `list[T]`; `**opts`
collects keyword arguments into a `dict[str, T]`. Keyword-only parameters
(after `*parts`) are not supported.

`print` itself takes the usual optional arguments:

```sere
print("a", "b", sep="-", end="")   # a-b (no newline)
```

## Lambdas

```sere
add1 = lambda (x: i32) -> i32: x + 1
print(add1(2))                     # 3
```

The signature block and `-> R` are optional; unannotated parameters are `Any`.
A lambda is a single expression and **cannot capture enclosing locals** — pass
values in as parameters instead. Use a nested `def` when you need capture.

## Nested functions and closures

```sere
def counter() -> Callable[i32]:
    def next() -> i32:
        count += 1
        return count
    count: i32 = 0
    return next
```

A nested `def` may read and write locals of the enclosing function. This is the
supported way to build closures and decorator wrappers (`global` / `nonlocal`
do not exist).

## Function values

Functions, lambdas, classes, structs, and bound methods (`self.method`) are
values. See the callable types in [language.md](../language.md#callables):

| Type | Accepts |
| --- | --- |
| `Callable` | anything callable; no argument checking |
| `Callable[R]` | any callable returning `R` |
| `Callable[[A, B], R]` | exact parameter list and return type |
| `Callable[[A, ...], R]` | prefix must match, extra arguments allowed |
| `Function[[A, B], R]` | same, but not classes/structs |
| `Class[T]` | the type object for `T` |

```sere
def apply(cb: Callable[[i32, i32], i32], x: i32, y: i32) -> i32:
    return cb(x, y)

print(apply(scale, 3, 4))           # 12
```

A `Callable` without type arguments returns `Any`; annotate the return type when
you use the result as a typed value.

Calling an unknown callable (a bare `Callable`, a field of callable type, or a
parameter without a signature) is still allowed and checked at runtime — the
compiler emits a dynamic call.

## Generic functions

```sere
def identity[T](value: T) -> T:
    return value

def first[T](items: list[T]) -> T:
    return items[0]
```

Type arguments are inferred from the arguments, or given explicitly:
`identity[i32](3)`. Parameters can be constrained — see
[generics.md](generics.md).

## Native functions

```sere
extern "C" "native_add"
def add(left: i32, right: i32) -> i32
```

The string is the link symbol; the `def` has no body. Native functions follow
the C ABI, so `str` means a Sere string value (data + length), not a
null-terminated buffer.

## Entry point

```sere
def main() -> i32:
    return 0

def main(argv: list[str]) -> i32:
    print(argv)
    return 0
```

Both forms are accepted; `argv[0]` is the program path. The return value becomes
the process exit code. `main` is the only place where the return type is
optional — it defaults to `i32`.

## Decorators

`@public`, `@private`, `@static`, `@abstract`, `@override` are compile-time
modifiers; any other decorator is a runtime callable applied bottom-up. See
[decorators.md](../decorators.md).

## See also

- [classes.md](classes.md) — methods, `self`, operator overloading.
- [control-flow.md](control-flow.md) — `defer`, `assert`.
- [memory.md](memory.md) — output parameters with `Ptr[T]`.
