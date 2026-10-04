# Standard library

`stdlib/` is ordinary Sere. The compiler injects `prelude.sere` into every
program. Everything else is opt-in:

```sere
import io
import gc
from math import sqrt
```

Search path (`ImportPath`): directory of the importing file, then the stdlib
directory next to `sere` (or `SERE_STDLIB` in tests).

## Prelude

For a numeric API reference with complete programs, see [Numeric arrays](arrays.md).
For syntax shared by collections, see
[Collections and strings](language.md#collections-and-strings).

`stdlib/prelude.sere` is always loaded and marked `fromPrelude()`. Keep it
small: names everyone needs, plus macros such as `dbg!`. Memory vocabulary is
documented in `stdlib/memory.sere` (comments; the types themselves are
compiler generics).

`print` is both a prelude-friendly name and a compiler intrinsic. Prefer
calling the existing intrinsic rather than reimplementing I/O in Sere.

## Modules

| Module | Role |
| --- | --- |
| `io` | Extra I/O (`read_line`, `eprint`) |
| `fs`, `path`, `os`, `env`, `sys` | Filesystem and process |
| `string`, `bytes`, `encoding`, `regex` | Text and binary |
| `math`, `vec`, `matrix`, `ml`, `arrays` | Numeric |
| `hash`, `random`, `time`, `log`, `bit` | Utilities |
| `gc`, `heap`, `memory` | Collectors, arenas, pointer docs |
| `inspect` | Runtime inspection helpers |
| `html_lang` | Indent-body HTML macro support |
| `windows`, `gl`, `qt6` | Native UI / graphics (GL: window close/state, shaders, mesh, FBO) |
| `requests` | HTTP client (`get` / `post` / `put` / `delete`) |
| `socket` | Low-level TCP/UDP sockets, address resolution, blocking modes, and integer options |
| `wsgi` | Blocking HTTP server; subclass `Handler` and implement `handle` |

Bindings that need C use:

```sere
extern "C" "sere_gc_collect"
def collect() -> void
```

The string must match a symbol in `sere_rt` (or a library passed with `--link`).

### `socket`

`socket` exposes owning native sockets for TCP (`SOCK_STREAM`) and UDP
(`SOCK_DGRAM`) over IPv4 and IPv6. Addresses are `(host, port)` tuples;
`bind` and `connect` resolve hostnames through the system resolver. Payloads
are `list[byte]` values (for example, `bytes.from_str("hello")`). `send` may
write only part of its input; `sendall` loops until all bytes are written.
Blocking is the default. `setblocking(false)` exposes native non-blocking
behavior, while `settimeout(seconds)` configures socket I/O timeouts supported
by the OS. Network and OS errors currently raise `RuntimeError` containing the
operation and native error code. Handles are owning and must be explicitly
closed or used in a `with` scope. Raw struct options, DNS result records,
UNIX-domain addresses, and event polling are not yet exposed.

```sere
import socket
import bytes

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind(("127.0.0.1", 0))
server.listen()
print(server.getsockname())
server.close()

payload: list[byte] = bytes.from_str("hello")
```

### `hash`

`hash` provides 64-bit non-cryptographic digests for hash tables, sharding, and
change detection. Every digest is a pure function of its input, so it is stable
across runs, machines, and both backends: a digest can be persisted or compared
between processes. Nothing here is cryptographic — FNV-1a and SplitMix64 are
public, invertible algorithms with published collisions, so they must not stand
in for a password hash, a signature, or any digest an attacker can steer.

| Call | Digest |
| --- | --- |
| `fnv1a(text)`, `hash_str(text)` | FNV-1a 64 over the text's raw bytes |
| `hash_bytes(data)`, `hash_i8_bytes(data)` | FNV-1a 64 over a byte list |
| `hash_int(value)`, `hash_i32(value)`, `hash_u64(value)` | SplitMix64 over the integer's bits |
| `hash_bool(value)` | SplitMix64, domain separated from the integers 0 and 1 |
| `hash_float(value)`, `hash_float32(value)` | SplitMix64 over the IEEE-754 bits |
| `file(path)` | FNV-1a 64 streamed over a file in bounded memory; raises `FileHashError` |
| `combine(left, right)`, `combine_all(values)` | order-sensitive folding of digests |
| `xor(left, right)` | order-insensitive folding, for set-like digests |
| `finalize(state)` | SplitMix64 avalanche, applied before a modulo |
| `bucket(digest, count)` | avalanched index in `[0, count)` |
| `hex_digest(value)`, `to_hex(value)` | the digest as 16 lowercase hex digits |

`Hasher` folds many values into one digest: `write` (raw text), `write_bytes`,
`write_i8_bytes`, `write_int`, `write_int32`, `write_bool`, `write_float`,
`write_float32`, `write_string`, `write_field`, then `finish()` (the raw stream
digest, equal to `fnv1a` for a text-only stream) or `digest()` (avalanched).
Typed writes fold a type tag and the value's own digest, so no two types share a
stream, and `write_string`/`write_field` fold a length before the bytes, so a
field can never run into the one after it. `copy()` snapshots a shared prefix.

Two properties are worth knowing: `hash_int(0)` and `finalize(0)` are 0, and
`-0.0` hashes exactly like `0.0` (they compare equal) while every NaN hashes
alike. NaNs are therefore unusable as hash-table keys.

```sere
import hash

key: str = "user:42"
shard: i64 = hash.bucket(hash.fnv1a(key), 16)

hasher: hash.Hasher = hash.Hasher()
hasher.write_field("id", 42)
hasher.write_string(user)
hasher.write_float(score)
record: i64 = hasher.digest()

if hash.file_matches("data.bin", saved_digest):
    print("unchanged")
```

## Native stdlib surface

If a module needs new C:

1. Add the C function to `runtime/` and declare it in `sere_rt.h` (or the
   matching public API header).
2. Rebuild `sere_rt`.
3. Declare `extern "C"` in `stdlib/yourmod.sere`.
4. Add `examples/…` and a `sere.example.*` test that `--emit-llvm`s it.

Optional heavy deps (Qt6) are behind CMake `find_package`. When Qt is missing,
`sere_qt6_stub.c` still links so `import qt6` typechecks; runtime calls fail
closed. Do not assume Qt is present in tests that only emit LLVM.

## Project layout vs stdlib

`sere init` creates a project with its own `src/` and `libs/`. User modules
resolve relative to the importing file. Publish reusable code with
`sere init-lib` + `sere pack` as a single `.slib` (reachable sources plus
compiled native objects). Drop that file into a project's `libs/` and
`import` it. A folder `libs/mylib/` with `lib.sere` or `mylib.sere` (and
optional C sources) is the same import without packing. Loose `.sere` files
on the import path still work. Neither belongs in `stdlib/` unless it is
part of the language distribution.
