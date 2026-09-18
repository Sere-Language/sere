# Strings

The type is `str`: an immutable, length-prefixed UTF-8 byte sequence. Backtick
literals produce `regex` instead, which shares the same layout and methods.

## Literals

```sere
a = "double quoted"
b = 'single quoted'
c = "triple
quoted spans lines"
d = `\d+`            # regex literal
```

Double and single quotes are **interchangeable** — `'a'` is the one-character
string `"a"`, not a byte value. This matters: `s.split('.')` needs a `str`
separator, and treating `'.'` as a byte would make it fail.

Only four escapes are recognised; any other backslash is dropped and the
following character is kept literally (so `\\` is `\` and `\"` is `"`).

| Escape | Meaning |
| --- | --- |
| `\n` | newline |
| `\t` | tab |
| `\r` | carriage return |
| `\0` | NUL |

There is no raw-string prefix and no `\u`/`\x` escape: `"\u{41}"` is the four
characters `u{41}`. Backtick regex literals are different — inside them only
`` \` `` and `\\` are unescaped, so `\d` stays `\d` for the regex engine.

Interpolation:

```sere
msg = f"count={n}"    # see formatting.md
```

## Length, indexing, slicing

`len(s)` returns `i64` — the number of **bytes**, not display columns.

```sere
s = "hello"
n = len(s)          # 5, i64
first = s[0]        # "h"   (a str, not a byte)
last = s[i32(n) - 1]
part = s[1:4]       # "ell" (start inclusive, end exclusive)
head = s[:3]        # "hel"
tail = s[3:]        # "lo"
all  = s[:]         # copy
```

Indexing yields a `str` of one character, so string comparison and
concatenation work directly on the result.

## Operators

| Expression | Result | Notes |
| --- | --- | --- |
| `a + b` | `str` | If one side is `str`, the other is converted to text |
| `s * n`, `n * s` | `str` | repeat; `n` is any integer |
| `a == b`, `a != b` | `bool` | value comparison |
| `a < b`, `a <= b`, `a > b`, `a >= b` | `bool` | byte-wise ordering |
| `x in s`, `x not in s` | `bool` | substring test |

`+` is the string-arithmetic operator: when **either** operand is a `str`, the
other operand is rendered with its normal text form, so no manual conversion is
needed.

```sere
s = "Num: "
s += "Hello"
s = s + "! " + 5          # "Num: Hello! 5"
print(1.5 + ": " + True)  # "1.5: True"
print("Hi" * 3)           # "HiHiHi"
print("ab" * 0)           # ""
```

Anything without a meaningful text form (a type object, a module, a callable)
is rejected: `cannot concatenate T onto a str`.

## Methods

All of these are builtin members; the compiler checks their arguments.

| Method | Returns | Description |
| --- | --- | --- |
| `upper()` / `lower()` | `str` | case conversion |
| `strip()` / `lstrip()` / `rstrip()` | `str` | trim whitespace |
| `capitalize()` / `title()` | `str` | capitalisation |
| `starts_with(t)` | `bool` | prefix test (`startswith` also accepted) |
| `ends_with(t)` | `bool` | suffix test (`endswith` also accepted) |
| `contains(t)` / `has(t)` | `bool` | substring test |
| `find(t)` / `rfind(t)` | `i64` | first/last offset, `-1` when absent |
| `count(t)` | `i64` | occurrences |
| `replace(old, new)` | `str` | every occurrence |
| `split(sep)` | `list[str]` | split on a non-empty separator |
| `join(parts)` | `str` | join a `list[str]` |
| `repeat(n)` | `str` | same as `s * n` |
| `is_empty()` | `bool` | zero length |
| `is_digit()` / `is_alpha()` / `is_space()` | `bool` | character-class test |

```sere
line = "  a,b,c  "
print(line.strip())              # "a,b,c"
print(line.strip().split(','))   # ['a', 'b', 'c']
print(",".join(["x", "y"]))      # "x,y"
print("abc".find("c"))           # 2
print("abc".count("z"))          # 0
```

`split` requires exactly one argument; there is no zero-argument
whitespace-splitting form.

## Iteration

```sere
for ch in "abc":
    print(ch)          # a, then b, then c
```

## Conversion

```sere
print(str(255))            # "255" — any value
print("\"" + str(true))    # "True"
n    = parse[i32]("42")    # i32, raises on bad input
n2   = try_parse[i32]("x") # i32 | None
```

See [formatting.md](formatting.md) for padding, hex, and precision, and
[numbers.md](numbers.md) for parsing options.
