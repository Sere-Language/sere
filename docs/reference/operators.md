# Operators

Operators are listed below with their exact meaning in this compiler.
Overloading them on classes is covered in [classes.md](classes.md).

## Arithmetic

| Operator | Operands | Result |
| --- | --- | --- |
| `+` | numbers; any operand `str`; two lists | number, `str`, or `list` |
| `-` `*` | numbers | number |
| `*` | `str`/`list` with an integer | repetition |
| `/` `//` `%` `**` | numbers | number |
| unary `-` | number | negation |
| unary `+` | number | identity |
| `~` | integer | bitwise complement |

`+` is special: if either side is a `str`, the other side is converted to text
and the result is a `str` — see [strings.md](strings.md#operators).

```sere
print("n=" + 5)      # n=5   (implicit conversion)
print([1] + [2])     # [1, 2]
print("ab" * 3)      # ababab
print([0] * 3)       # [0, 0, 0]
```

For integer `/` and `//` see [numbers.md](numbers.md#arithmetic).

## Comparison

`==` `!=` `<` `<=` `>` `>=` return `bool`.

| Operand type | `==` compares |
| --- | --- |
| numbers | value |
| `str` / `regex` | contents |
| enum | variant identity |
| class with `__eq__` | whatever `__eq__` returns |
| `list`, `dict`, buffers | **identity**, not contents |

Records (classes) need `__eq__` to support `==` meaningfully; without it the
comparison is false for distinct objects.

## Logical

| Operator | Notes |
| --- | --- |
| `and` | short-circuits; requires `bool` operands |
| `or` | short-circuits; requires `bool` operands |
| `not` | requires `bool`, or a type with `__bool__` |

```sere
if a > 0 and b > 0:
    print("both positive")
```

There is no implicit truthiness for numbers or collections: write the comparison
you mean (`n != 0`, `len(xs) > 0`).

## Bitwise and shifts

`&` `|` `^` `<<` `>>` require integer operands (or integer enums). The result
takes the wider operand's width.

```sere
flags = 0b1010 | 0b0001
masked = flags & 0b1100
shifted = 1 << 3
```

## Identity and membership

| Operator | Meaning |
| --- | --- |
| `a is b` | identity: same enum variant / same object; also `x is None` |
| `a is not b` | negated identity |
| `x in c` | membership: substring for `str`, element for a list, key for a dict |
| `x not in c` | negated membership |

```sere
if ptr is None:
    print("unset")

if 3 in [1, 2, 3]:
    print("found")

if "ell" in "hello":
    print("substring")
```

`in` also uses `__contains__` when the right operand's class defines it.

## Assignment operators

`=` plus the compound forms `+=` `-=` `*=` `/=` `//=` `%=` `**=` `&=` `|=` `^=`
`<<=` `>>=`.

Compound assignment accepts numbers, and additionally:

- `s += x` on a `str` — concatenation with implicit conversion
- `s *= n` on a `str` — repetition
- `xs += ys` on a list — concatenation
- `xs *= n` on a list — repetition
- `obj += x` on a class with `__add__` — dispatches to the method

```sere
s = "Num: "
s += 5              # "Num: 5"
xs = [1]
xs += [2, 3]        # [1, 2, 3]
xs *= 2             # [1, 2, 3, 1, 2, 3]
```

## Precedence

Precedence and associativity follow Python's table; see
[../language.md](../language.md#operators). Parenthesise when in doubt.

## Overloadable operators

These can be defined on a class (see
[classes.md](classes.md#operator-overloading)):

`+` `-` `*` `/` `//` `%` `**` `&` `|` `^` `<<` `>>` `==` `!=` `<` `<=` `>` `>=`

`and`, `or`, `not`, `is`, `in`, and the assignment operators themselves are not
overloadable (a class may still provide `__contains__` for `in`).
