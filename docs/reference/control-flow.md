# Control flow

## `if` / `elif` / `else`

```sere
if n < 0:
    print("negative")
elif n == 0:
    print("zero")
else:
    print("positive")
```

The condition must be `bool`. A `str` or a number is not a `bool`, so `if s:`
is a type error — write `if len(s) > 0:` or `if s != "":`. A type with `__bool__`
may be used directly.

The ternary form mirrors Python:

```sere
label = "big" if n > 100 else "small"
```

## `while`

```sere
i = 0
while i < 10:
    i += 1
```

`while` condition is re-evaluated before each iteration.

## `for`

Iterate any iterable: a `list`, `str`, `dict`, `set`, a `range()`, or a type with
`__iter__`.

```sere
for item in items:
    print(item)

for ch in "abc":
    print(ch)                # a b c

for key in table:
    print(key, table[key])
```

Iterating a `dict` yields keys. There is no builtin `enumerate`; track the index
yourself when you need it:

```sere
index = 0
for value in items:
    print(index, value)
    index += 1
```

`range` is typed by its bounds — `range(0, n)` where `n: i64` yields `i64`, and
`range(3)` yields `i32`. This matters when the loop variable feeds an `i64`
parameter:

```sere
def total(n: i64) -> i64:
    sum = 0i64
    for i in range(0, n):    # i is i64
        sum += i
    return sum
```

## `break` / `continue`

```sere
for v in values:
    if v < 0:
        continue
    if v > 100:
        break
    print(v)
```

Both work in `while` and `for`.

## `match`

```sere
match command:
    case "stop":
        running = False
    case "go":
        running = True
    case _:
        print("unknown")
```

The subject may be any type. Literals, enum variants, and payload patterns are
supported:

```sere
match shape:
    case Circle(r):
        area = 3.14159 * r * r
    case Rect(w, h):
        area = w * h
    case Point:
        area = 0.0
```

- `case _:` is the wildcard and must come last.
- `case Message.Move(x, y):` binds payload fields to new names.
- `case A | B:` matches either variant.
- `case pat if cond:` adds a guard.
- For enum subjects the `match` must be **exhaustive** — see
  [enums.md](enums.md).

See [Pattern matching](../language.md#pattern-matching) for the grammar.

## `with`

```sere
with open("data.txt") as f:
    text = f.read()
```

The expression's type must define `__enter__` and `__exit__`; `__exit__` runs on
normal exit and on `return`, `break`, or `continue` leaving the block. `as name`
is optional. Exactly one context object is evaluated — nest `with` statements for
more than one resource:

```sere
with open("in.txt") as src:
    with open("out.txt") as dst:
        dst.write(src.read())
```

The `as` cast operator is disabled inside the `with` expression, so bind first if
you need a cast:

```sere
resource = make_resource() as Handle
with resource as handle:
    handle.use()
```

## `defer`

`defer` runs a statement on scope exit, in reverse order of registration.

```sere
def process(path: str) -> void:
    f = open(path)
    defer f.close()
    if not f.valid():
        return               # f.close() still runs
    f.write("done")
```

## `del`

```sere
del items[2]
del table["key"]
```

`del` needs a subscript target. Deleting a bare name is not implemented.

## `assert`

```sere
assert len(items) > 0
assert n >= 0, "n must be non-negative"
```

`assert` always runs. The optional second argument is the message shown on
failure, and the raised `AssertionError` is catchable — see
[exceptions.md](exceptions.md).

## `pass`

`pass` is the empty statement, used where a block is required.

```sere
class Marker:
    pass
```

## Scope

`global` and `nonlocal` do not exist. A nested `def` captures locals of the
enclosing function and is the supported way to build closures; a lambda cannot
capture. Declare a variable before a block that writes to it, and give loop
locals a type when the inferred type would be `Any`.

## See also

- [functions.md](functions.md) — parameters, defaults, `*args`.
- [exceptions.md](exceptions.md) — `try` / `except` / `finally`.
- [async.md](async.md) — `await` and `async for`.
