# Macros

Macros run **after parsing, before type checking**. They rewrite syntax, and
they are hygienic by default: names introduced inside a `quote` do not capture
names at the call site. Expansion depth is capped and reports
`RecursionError`.

## Invocation forms

| Form | Use |
| --- | --- |
| `name!(...)` | expression or statement |
| `name!{...}` / `name![...]` | token-tree argument |
| `name:` + indented body | statement macro, or initializer after `=` |

```sere
total: i32 = twice!(n)

node: Html = html:
    <div>{title}</div>
```

Do not write `if left < right:` as an indent macro — the `ident:` newline after a
comparison is read as the suite colon.

## Quote and splice

```sere
macro twice(x):
    quote:
        ($x) + ($x)

total: i32 = twice!(n)      # (n) + (n)
```

- `$name` splices an argument.
- `$($x),*` repeats a splice inside a `quote` (the separator after `,*` is
  emitted between expansions).
- `$type` is available when the macro declares `typed: true`; it is the type of
  the first argument as a token.

## Token-tree matching

```sere
macro vec:
    match:
        ($($x:expr),*) => quote:
            [$($x),*]

xs: list[i32] = vec!(1, 2, 3)   # [1, 2, 3]
```

Each `$name:spec` captures one token tree. Supported specs: `expr`, `ident`,
`literal`. Arms are tried in order, and a `match` macro that matches nothing
reports a diagnostic.

## Statement and pipeline forms

```sere
macro html:
    syntax: raw          # raw | tokens | pipeline | (default: sere)
    interpolate: brace   # brace | dollar
    wrapper: Html        # constructor wrapped around the result
    typed: true

n: i32 = pipeline:
    1
    |> add2
    |> wrap(4)
```

| Property | Values | Effect |
| --- | --- | --- |
| `syntax` | `raw`, `tokens`, `pipeline` | how the body is tokenized before expansion |
| `interpolate` | `brace`, `dollar` | which interpolation form the body understands |
| `wrapper` | a type name | wraps the expansion in that constructor |
| `typed` | `true` / `false` | exposes `$type` |

The indent form (`name:` with an indented body) is available for statement
position and as an initializer after `=`:

```sere
node: Html = html:
    <div>{title}</div>
```

The pipeline form collects a value and `|>` steps:

```sere
n: i32 = pipeline:
    1
    |> add2
    |> wrap(4)
```

## Importing macros

A macro is an ordinary name, so it imports with the module:

```sere
from html_lang import html, Html
```

## Prelude macros

| Macro | Effect |
| --- | --- |
| `cfg!(windows)` / `cfg!(linux)` / `cfg!(macos)` / `cfg!(unix)` | host OS flag |
| `cfg!(x86_64)` / `cfg!(arm64)` / `cfg!(debug)` | architecture / build flag |
| `todo!("msg")` | raises `NotImplementedError` |
| `unreachable!()` | raises if reached |
| `dbg!(value)` | prints the value and yields it |

## See also

- [modules.md](modules.md) — import search paths and `__exports__`.
- [../decorators.md](../decorators.md) — runtime decorators (related but
  different: decorators wrap values, macros rewrite syntax).
- [../architecture.md](../architecture.md) — where macro expansion sits in the
  pipeline.
