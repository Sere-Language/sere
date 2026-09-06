# Decorators reference

A decorator is a name (or call) written above a declaration with `@`. Sere has
two kinds, and both may appear on the same declaration:

- **Reserved modifiers** — `@public`, `@private`, `@static`, `@abstract`,
  `@override`, `@frozen`, `@flags`. Handled by the type checker; they never
  produce a runtime wrapper.
- **Runtime decorators** — any user callable applied with `@name`,
  `@name(args)`, or `@Class.method`. They wrap the decorated value at module
  initialization.

```sere
@public            # reserved modifier
@logged            # runtime decorator
def boot() -> i32:
    return 0
```

Reserved names cannot be redefined and are not valid runtime decorators.

## Ordering

Decorators run **bottom-up**: the one closest to the declaration runs first.
Writing

```sere
@a
@b
def f(n: i32) -> i32:
    return n + 1
```

means `f = a(b(f))`.

## Reserved modifiers

| Decorator | Applies to | Effect |
| --- | --- | --- |
| `@public` | field, `def`, property accessor, `class`, `struct`, `enum`, `type`, `macro`, module binding | Marks the declaration exported. Public is the default, so `@public` mostly documents intent next to `@private` siblings. |
| `@private` | same as `@public` | Not exported: `import` / `from module import name` and `module.name` fail with `PermissionError`. Private methods and property setters are only callable inside the owning class. |
| `@static` | field | Same as `static name: T` — one value shared by the whole class, read and written as `MyClass.x` or `self.x`. |
| `@abstract` | method | With an empty or `pass` body, subclasses must override it. With a real body it is a default hook that overriding is optional for. |
| `@override` | method | Documents that the method overrides a base method. |
| `@frozen` | class | Fields are not assignable after `__init__` runs. |
| `@flags` | enum | Variants become a bit-flag set: `|`, `&`, `^` combine masks and `Flag.A in mask` is a bitwise membership test. |

### Visibility details

- A `@private` field cannot be read or written outside the class. Expose it
  through a `@public` property accessor (see
  [Properties](language.md#properties)).
- A getter and setter have independent visibility: a public getter with a
  private setter is read-only from outside.
- Re-exporting a private name through `__exports__` does not bypass the rule;
  the binding itself is not visible to importers.

### `@abstract` details

```sere
class Animal:
    @abstract
    def speak(self) -> i32:
        pass              # must be overridden

    @abstract
    def describe(self) -> str:
        return "animal"   # optional override (default hook)
```

`@override` on a method that overrides nothing, or forgetting `@override` on
one that does, is not an error — the decorator is documentation.

### `@flags` details

```sere
@flags
enum Perm:
    Read = 1
    Write = 2
    Exec = 4

mask = Perm.Read | Perm.Exec
if Perm.Read in mask:
    print("readable")
```

Payload variants cannot be flags; a `@flags` enum must have unit variants.

## Runtime decorators

A runtime decorator is any callable that receives the decorated object and
returns its replacement:

```sere
def logged(fn: Function[[i32], i32]) -> Function[[i32], i32]:
    def wrapper(n: i32) -> i32:
        print("logged", n)
        return fn(n)
    return wrapper

@logged
def bump(n: i32) -> i32:
    return n + 1
```

Rules:

- **Plain `@name`** applies the callable named `name`.
- **`@name(args)`** is a *factory*: the call is evaluated once and its result
  is the decorator.
- **`@Class.method`** uses a class method as the decorator.
- Decorating a `class` or `struct` passes the **type object** (`Class[T]`) to
  the decorator.

```sere
def prefix(p: str) -> Function[[str], str]:
    def deco(fn: Function[[str], str]) -> Function[[str], str]:
        def wrapper(s: str) -> str:
            return fn(p + s)
        return wrapper
    return deco

@prefix("user: ")
def name(s: str) -> str:
    return s
```

### When they run

Runtime decorators run **once, at module initialization**, before `main`.
The wrapped value is stored, and every later reference to the name — including
from other modules that imported it — goes through the wrapper.

### Type checking

- Each decorator call is type checked like an ordinary call: the decorated
  value must be assignable to the decorator's parameter type, and the
  decorator's return type becomes the new type of the name.
- A decorator whose signature is unknown (its parameter or result is a bare
  `Callable`) leaves the original signature intact, Python-style. Note that a
  bare `Callable` result is `Any` — annotate `Function[[P...], R]` /
  `Callable[[P...], R]` when you want the wrapped function's calls checked.
- Decorating an untyped function is a diagnostic (`cannot decorate an untyped
  function`); give the function parameter and return annotations first.
- Methods may be decorated too; the wrapper replaces the method on the class.

### Closures in wrappers

Lambdas cannot capture enclosing locals, so wrappers that need the decorated
function should use a nested `def`, which can capture:

```sere
def count_calls(fn: Function[[i32], i32]) -> Function[[i32], i32]:
    total: static i32 = 0        # module-level storage also works
    def wrapper(n: i32) -> i32:
        return fn(n)
    return wrapper
```

## Decorator targets summary

| Target | Runtime decorators receive | Reserved decorators allowed |
| --- | --- | --- |
| module-level `def` | the function value | `@public`, `@private` |
| method | the function value | `@public`, `@private`, `@static`, `@abstract`, `@override` |
| property accessor | the accessor function | `@public`, `@private` |
| `class` / `struct` | the type object | `@public`, `@private`, `@frozen` |
| `enum` | not decorated at runtime | `@public`, `@private`, `@flags` (on the enum), `@abstract` (on methods) |
| field | not decorated at runtime | `@public`, `@private`, `@static` |
| `type` alias / `macro` | not decorated at runtime | `@public`, `@private` |

Executable examples: `examples/oop.sere` and `examples/properties.sere`
show reserved modifiers (`@abstract`, `@public`, `@private`, `@frozen`)
alongside properties and static fields.
