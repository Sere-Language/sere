# Bytes and buffers

`byte` is a builtin alias of `u8`. A byte buffer is a `list[byte]`, and the
`bytes` module provides allocation, endian conversion, and a `Buffer` wrapper.

```sere
import bytes
```

## Creating buffers

```sere
import bytes

raw = bytes.zeros(16)          # list[byte], 16 zero bytes
buf = bytes.Buffer(1024)       # object with .data: list[byte]
```

`bytes.bytearray(n)` is an alias of `bytes.zeros(n)`.

## Reading and writing

```sere
b = bytes.zeros(4)
bytes.set(b, 0, 0x2A)          # b[0] = 42
v = bytes.get(b, 0)            # 42, a byte

buf = bytes.Buffer(8)
buf.set(3, 0xFF)               # the same as bytes.set(buf.data, 3, 0xFF)
buf.fill(0x00)                 # overwrite every byte
print(len(buf))                # 8
print(buf.get(3))              # 255
print(buf[3])                  # 255 — __getitem__
```

A `Buffer` also supports `push(value)` to append, `copy()`, `hex()`,
`to_str()`, and `__str__` (which renders up to the first 128 bytes). Its
underlying storage is always reachable as `buf.data`.

## Free functions

| Function | Returns | Description |
| --- | --- | --- |
| `zeros(n)` / `bytearray(n)` | `list[byte]` | `n` zero bytes |
| `copy(buf)` | `list[byte]` | duplicate |
| `concat(a, b)` | `list[byte]` | join two buffers |
| `fill(buf, value)` | `void` | set every byte |
| `set(buf, index, value)` | `void` | write one byte |
| `get(buf, index)` | `byte` | read one byte |
| `find(hay, needle)` | `i64` | first offset of a run, or `-1` |
| `eq(a, b)` | `bool` | content comparison |
| `from_str(text)` | `list[byte]` | UTF-8 bytes of a string |
| `to_str(buf)` | `str` | decode as UTF-8 |
| `hex(buf)` | `str` | hexadecimal rendering |
| `from_hex(text)` | `list[byte]` | parse a hex string |
| `xor` / `bitand` / `bitor` | `list[byte]` | element-wise bitwise ops |
| `bitnot(buf)` | `list[byte]` | invert every byte |

Content comparison needs `bytes.eq`; `==` on two buffers compares identity (see
[lists.md](lists.md)).

## Endian conversion

Readers and writers exist for 16-, 32-, and 64-bit integers in both byte orders,
plus `f32`/`f64`:

```sere
import bytes

b = bytes.zeros(8)
bytes.write_u32_le(b, 0, 0x12345678)
print(bytes.read_u32_le(b, 0))   # 305419896
print(bytes.read_u32_be(b, 0))   # byte-swapped view

bytes.write_u16_be(b, 4, 0x0102)
print(bytes.hex(b))              # includes 01 02 at offset 4
```

| Group | Functions |
| --- | --- |
| read integers | `read_u16_le/be`, `read_u32_le/be`, `read_u64_le/be` |
| write integers | `write_u16_le/be`, `write_u32_le/be`, `write_u64_le/be` |
| read floats | `read_f32_le`, `read_f64_le` |
| write floats | `write_f32_le`, `write_f64_le` |

Reads return **unsigned** integers; cast with `as i32` when you want a signed
interpretation. Reads do not bounds-check beyond the buffer's own runtime
bounds checking on `get`.

## Example: a memory dump

```sere
import bytes

def dump(buf: bytes.Buffer, count: i64) -> void:
    i: i64 = 0
    while i < count:
        print(f"{i:#06x}: {buf.get(i):#04x}")
        i += 1

def main() -> i32:
    mem = bytes.Buffer(16)
    mem.fill(0)
    bytes.write_u16_le(mem.data, 0, 0xBEEF)
    dump(mem, 4)
    return 0
```

See [formatting.md](formatting.md) for the `#06x` spec and [lists.md](lists.md)
for the operations inherited from `list[byte]`.
