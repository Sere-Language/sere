# Numbers

## Types

| Type | Width | Notes |
| --- | --- | --- |
| `i8` `i16` `i32` `i64` | 8/16/32/64 | signed |
| `u8` `u16` `u32` `u64` | 8/16/32/64 | unsigned; `u8` is also spellable `byte` |
| `f32` `f64` | 32/64 | IEEE-754 |
| `bool` | 1 bit | `True` / `False` |

There is no implicit truthy conversion of numbers to `bool`; conditions must be
`bool` (use `n != 0`).

Aliases for generic parameters live in the prelude: `Int`, `Float`, `SignedInt`,
`UnsignedInt`.

## Literals

```sere
d = 1_000_000        # underscores are ignored
h = 0xFF             # hexadecimal
b = 0b1010           # binary
o = 0o17             # octal
f = 1.5              # f64
g = 1.5f32           # f32 suffix
```

An integer literal takes the type its context requires, so `x: i64 = 1` and
`f(1u8)` both work.

## Arithmetic

| Operator | Meaning |
| --- | --- |
| `+` `-` `*` | add, subtract, multiply |
| `/` | division — see below |
| `//` | division — see below |
| `%` | remainder |
| `**` | power |

```sere
print(7 / 2)      # 3   (integer division)
print(7.0 / 2.0)  # 3.5
print(7 % 3)      # 1
print(2 ** 10)    # 1024
```

For **integer** operands both `/` and `//` perform truncating division — the
fractional part is discarded and the sign follows the dividend:

```sere
print(7 / 2)     # 3
print(-7 / 2)    # -3   (truncated toward zero, not floored)
print(7 // 2)    # 3
```

If you need a fractional result or Python's floor semantics, convert to a float
first (`f64(a) / f64(b)`).

## Mixed types

When the two operands differ, the result is the wider type; any float operand
makes the result floating-point:

```sere
x = 1 + 2.5      # f64
y: i64 = 3
z = y + 1        # i64 (the literal takes i64)
```

## Bitwise and shifts

```sere
print(0b1100 & 0b1010)   # 8
print(0b1100 | 0b1010)   # 14
print(0b1100 ^ 0b1010)   # 6
print(~0)                # -1
print(1 << 4)            # 16
print(256 >> 2)          # 64
```

Bitwise operators require integer operands. `~`, `<<`, `>>` work on any integer
width; the result keeps the operand's width.

## Comparison

`==` `!=` `<` `<=` `>` `>=` compare numbers by value and yield `bool`. Mixing
signed and unsigned operands follows the wider type.

## Casting

```sere
n: i32 = 300
big = i64(n)             # widening
small = n as u8          # truncating, via `as`
f = f64(n)
back = i32(f)            # truncates toward zero
print(i32(len(xs)))      # very common: len() is i64, indexing wants i32
```

`as` and `T(value)` are equivalent. Narrowing keeps the low bits.

## Prelude helpers

```sere
print(abs(-3))           # 3
print(min(1, 2), max(1, 2))
print(clamp(15, 0, 10))  # 10
print(sign(-7))          # -1
```

For hexadecimal and other bases in output, use
[formatting.md](formatting.md) (`f"{n:#x}"`) or the prelude `hex()` helper.
