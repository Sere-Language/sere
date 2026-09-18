# Formatting: f-strings

String interpolation uses `f"..."` with `{expression}` holes. A hole may carry a
format spec after `:`:

```
f"{expression:spec}"
```

```sere
n = 255
print(f"value is {n}")        # value is 255
print(f"{n} in hex is {n:#x}") # 255 in hex is 0xff
```

`{{` and `}}` produce literal braces. An expression may contain parentheses,
brackets, or string literals — a `:` inside those does not start the spec:

```sere
print(f"{items[1:3]}")    # the colon is a slice, not a spec
```

Everything interpolated is converted with the same rules as [strings.md](strings.md);
**any** printable value works, including lists, dicts, enums, and records.

## Format spec grammar

```
[[fill]align][sign][#][0][width][,][.precision][type]
```

| Part | Values | Meaning |
| --- | --- | --- |
| `fill` | any character | padding character (with `align`) |
| `align` | `<` `>` `^` `=` | left, right, centre, after the sign |
| `sign` | `+` `-` ` ` | always show sign / space for positives |
| `#` | flag | add the `0x` / `0o` / `0b` prefix |
| `0` | flag | zero-pad up to `width` |
| `width` | integer | minimum field width |
| `,` | flag | thousands separators (decimal only) |
| `.precision` | integer | decimals for floats, max characters for strings |
| `type` | see below | how to render the value |

### Types

| Type | Applies to | Result |
| --- | --- | --- |
| *(none)* | anything | default text form |
| `d` | integers | decimal |
| `b` `o` `x` `X` | integers | binary, octal, lowercase/uppercase hex |
| `f` `F` | numbers | fixed point |
| `e` `E` | numbers | scientific |
| `g` `G` | numbers | shortest of fixed/scientific |
| `%` | numbers | percentage (value × 100, `%` appended) |
| `s` | strings | the string (default) |

Integers also accept `f`, `e`, `g`, and `%`, which convert them to floating
point first.

## Examples

```sere
v: i32 = 255
b: u8 = 7
big: i64 = 48879
ratio: f64 = 0.1234

print(f"{v:08x}")       # 000000ff
print(f"{v:#08x}")      # 0x0000ff
print(f"{v:X}")         # FF
print(f"{v:>8x}|")      #       ff|
print(f"{v:08b}")       # 11111111
print(f"{b:#04x}")      # 0x07
print(f"{big:#x}")      # 0xbeef
print(f"{ratio:.2f}")   # 0.12
print(f"{ratio:.1%}")   # 12.3%
print(f"{1234567:,d}")  # 1,234,567
print(f"{v:*^8}|")      # **255***|
print(f"{'ab':>5}|")    #     ab|
print(f"{'abcdef':.3}") # abc
```

Zero padding counts the `#` prefix in the width: `#08x` of `255` is
`0x0000ff` (8 characters).

## Empty spec

`f"{n:}"` is valid and means "no spec" — identical to `f"{n}"`. Write the spec
only when you want a non-default rendering.

## Caveats

- `.precision` is ignored for integers; it applies to floats and strings only.
- `c` is accepted by the type checker but not implemented by the runtime; it
  prints the decimal value rather than a character. Avoid it.
- An invalid spec is a compile error, e.g. `f"{n:#ff}"` reports
  `'#' needs a 'b', 'o', 'x', or 'X' format type` because `f` is a float code.
- The spec is a **compile-time literal**. A spec cannot be built at runtime, so
  `f"{v:{spec}}"` is not supported.

## Runtime-built formatting

Because specs are static, use the prelude `hex()` helper when the base or width
is only known at runtime:

```sere
import bytes

print(hex(255))            # 0xff
print(hex(255, True))      # 0XFF
print(hex(7, False, 4))    # 0x0007
print(hex(255, False, -1, ""))  # ff
```

````
def hex(value: i64, uppercase: bool = False, digits: i32 = -1, prefix: str = "0x") -> str
````

For byte buffers, `bytes.hex(buffer)` renders a whole buffer.

## Related

- [strings.md](strings.md) — what `+` and `str()` produce.
- [numbers.md](numbers.md) — integer widths and casts.
