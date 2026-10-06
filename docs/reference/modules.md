# Modules and imports

## Forms

```sere
import util                     # binds `util`
import util as u                # binds `u`
from util import double         # binds `double`
from util import double as d
from window import Window as BaseWindow
from html_lang import html, Html
from string import *            # binds every export
```

`import util` binds **only** the module name; reach members through it
(`util.double(2)`). A local `class Window` is a different type from
`gl.Window`, so two modules may each define a `Window` without clashing.

`from SomeClass import someMethod` in the same file hoists a class method to
module scope so it can be exported.

## Exports

Public top-level names are exported by default. `@private` excludes a name from
imports and from `module.name` access (attempting either is a `PermissionError`).

`__exports__` re-exports names that were imported, which is how a barrel module
is built:

```sere
def version() -> str:
    return "1"

from Greeter import hello
from window import Window as BaseWindow

__exports__ += [hello, BaseWindow]   # keep `version` and add these
# __exports__ = [hello]              # export only `hello`
```

## Search order

For `import util`, the compiler looks for:

1. the directory of the importing file, and `libs/` next to it;
2. the current working directory;
3. the stdlib next to the `sere` executable (or `SERE_STDLIB`).

A module may be provided by:

| Layout | Module name |
| --- | --- |
| `util.sere` | `util` |
| `util.slib` | `util` |
| `util/__init__.sere`, `util/util.sere` or `util/lib.sere` | `util` |

Resolution within each search directory prefers `util.sere`, then `util.slib`,
then a folder entry. Within a folder, `__init__.sere` wins over `util.sere`,
which wins over `lib.sere`. Existing file modules keep their precedence.

An `__init__.sere` file is an ordinary module containing the package's public
API, not a constructor method. For example:

```text
widgets/
    __init__.sere
    buttons.sere
```

```sere
# widgets/__init__.sere
from widgets.buttons import Button
__exports__ = [Button]
```

Consumers can use `from widgets import Button`, `import widgets` followed by
`widgets.Button`, or `from widgets.buttons import Button`. Submodules are
resolved independently; importing a dotted submodule does not implicitly execute
its parent's initializer. Use explicit imports and `__exports__` to expose names
from the package entry point.

A folder
library may also contain native sources (`.c`, `.lib`) directly in the folder or
in `native/`; those are compiled and linked automatically. `.slib` files are
extracted next to themselves under `.sere-lib/`, and any native objects they
hold are linked. A `.slib` packs only its entry module, the local modules that
entry actually imports, and its compiled native objects — not the whole tree.

The language server uses the same search path, so drop-in `.slib` files and
folder libraries complete and hover exactly like source modules.

See [projects.md](../projects.md) for repository layout and
[packaging.md](../packaging.md) for building a `.slib`.

## Module globals

Always in scope:

| Name | Value |
| --- | --- |
| `__name__` | `"__main__"` in the entry file, otherwise the module stem |
| `__file__` | Source path |
| `__package__` | Package string |
| `__doc__` | Leading docstring, if any |
| `__debug__` | True in debug-oriented builds |
| `__sere_version__` | Compiler version string |

## Platform flags

These are compile-time `bool`s. A branch guarded by a false flag is **not
typechecked**, so platform-specific APIs can be referenced freely inside it:

| Flag | Meaning |
| --- | --- |
| `__windows__` `__linux__` `__macos__` `__unix__` | operating system |
| `__x86_64__` `__arm64__` | architecture |
| `__platform__` | `"windows"` / `"linux"` / `"macos"` |
| `__arch__` | `"x86_64"` / `"arm64"` / `"unknown"` |

```sere
if __windows__:
    windows.message_box("hi")

if cfg!(linux):
    pass
```

`cfg!(...)` is a prelude macro accepting `windows`, `linux`, `macos`, `unix`,
`x86_64`, `arm64`, and `debug`; it expands to the matching dunder.

## Directives

The `ifdef` family is the same compile-time test written in C's `#ifdef` shape:

```sere
ifdef(windows):
    windows.message_box("hi")
elifdef(macos):
    macos.notify("hi")
elsedef:
    print("hi")

ifndef(windows):
    print("not windows")
```

`ifdef(name)` stands for `if __name__:`, `ifndef(name)` for `if not __name__:`,
and `elifdef` / `elifndef` / `elsedef` continue the chain. The flag name may be
written either way (`ifdef(windows)` or `ifdef(__windows__)`); an unknown flag is
reported at the directive. Because the condition is a compile-time flag, the
branch that does not apply is not typechecked and never reaches a backend.

There is no `#endif`: the indented suite closes the directive, as it does for
`if`. `ifdef` stays usable as an ordinary identifier outside statement
position.

## Imports of macros

Macros are imported like any other name — `from html_lang import html, Html` —
and then used as `html:` or `html!(...)`. See [macros.md](macros.md).

## See also

- [macros.md](macros.md) — defining and invoking macros.
- [exceptions.md](exceptions.md) — `ImportError`, `PermissionError`.
