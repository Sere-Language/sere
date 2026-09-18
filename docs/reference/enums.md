# Enums

An enum is a closed set of named variants.

```sere
enum Color:
    Red
    Green = 2
    Blue
```

Variants are constructed as `Color.Green`. Values are assigned in declaration
order, or set explicitly with `= N`; a following variant continues from there
(so `Color.Blue` is `3`).

## Unit variants

| Expression | Result | Type |
| --- | --- | --- |
| `Color.Green` | the variant | `Color` |
| `Color.Green.name` | `"Green"` | `str` |
| `Color.Green.value` | `2` | `i32` |
| `i32(Color.Green)` | `2` | `i32` |
| `Color.variants()` | `["Red", "Green", "Blue"]` | `list[str]` |
| `tone is Color.Green` | identity test | `bool` |
| `tone == Color.Green` | equality | `bool` |

An enum with no payloads behaves as an integer wherever an integer is expected —
pass it to an `i32` parameter, assign it to an `i32`, compare it with numbers —
without writing `.value` or an explicit cast.

```sere
enum Level:
    Low
    High

def label(l: Level) -> str:
    if l == Level.High:
        return "high"
    return "low"

n: i32 = Level.Low          # 0
print(n + 1)                # 1
```

`switch`-style integer tests are cheap: unit enums are plain integers at
runtime.

## Payload variants

A variant may carry data, declared either as a type or as named fields.

```sere
enum Message:
    Quit
    Move(x: i32, y: i32)
    Write(str)
```

```sere
m = Message.Move(1, 2)
w = Message.Write("hello")
q = Message.Quit
```

Match to get at the payload:

```sere
match m:
    case Message.Move(x, y):
        print(x, y)          # 1 2
    case Message.Write(text):
        print(text)
    case Message.Quit:
        pass
```

Payloads are read with the pattern, not with field access.

## Methods

Enums can declare methods and use `self` like a class.

```sere
enum Token:
    Number(i64)
    Plus
    Minus

    def is_operator(self) -> bool:
        match self:
            case Token.Plus | Token.Minus:
                return True
            case _:
                return False

    def __str__(self) -> str:
        return "Token." + self.name
```

## Flags

`@flags` marks an enum as a bit set. Use `in` to test membership and the
bitwise operators to combine.

```sere
@flags
enum Perm:
    Read = 1
    Write = 2
    Exec = 4

p = Perm.Read | Perm.Write

print(Perm.Read in p)        # True
print(Perm.Exec in p)        # False
print(i32(p))                # 3
```

## Exhaustiveness

When the `match` subject is an enum, every variant must be covered or the last
arm must be `case _`:

```sere
match tone:
    case Color.Red:
        pass
    case Color.Green:
        pass
    case Color.Blue:
        pass
```

Omitting one is a compile error:

```
error[ExhaustivenessError]: match is not exhaustive; missing Color.Blue
```

Non-enum subjects have no exhaustiveness requirement. Arms are tried in order,
the first match wins, and `case pat if expr` adds a guard.

```sere
match tone:
    case Color.Red if count > 0:
        pass
    case _:
        pass
```

## Generic payloads

Payload variants may use the enum's type parameters — see
[generics.md](generics.md).

```sere
enum Result[T, E]:
    Ok(T)
    Err(E)
```

## See also

- [control-flow.md](control-flow.md) — the full `match` grammar.
- [classes.md](classes.md) — records, methods, and dunder methods.
