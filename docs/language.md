# Sere language reference

This is the user-facing description of Sere **as the compiler implements it today**.
It is not a roadmap. Features that are tokenized but not implemented are called
out at the end.

Sere is a **statically typed Python-superset**. Indentation is significant.
Programs compile to native code through LLVM 22. `print` is a language intrinsic
(also used from the prelude); it is not a statement.

A minimal program:

```sere
def main() -> i32:
    print("hello, sere")
    return 0
```

Compile:

```powershell
sere examples\hello.sere -o hello.exe
.\hello.exe
```

The process exit code is `main`'s `i32` return value.

---

## Contents

1. [Programs](#programs)
2. [Lexical structure](#lexical-structure)
3. [Types](#types)
4. [Names and bindings](#names-and-bindings)
5. [Expressions](#expressions)
6. [Statements](#statements)
7. [Functions](#functions)
8. [Decorators](#decorators)
9. [Classes, structs, and enums](#classes-structs-and-enums)
10. [Modules and imports](#modules-and-imports)
11. [Memory and pointers](#memory-and-pointers)
12. [Collections and strings](#collections-and-strings)
13. [Pattern matching](#pattern-matching)
14. [Errors](#errors)
15. [Macros](#macros)
16. [Introspection and platform](#introspection-and-platform)
17. [Intrinsics](#intrinsics)
18. [Standard library](#standard-library)
19. [Native interop](#native-interop)
20. [Diagnostics](#diagnostics)
21. [Reserved, not implemented](#reserved-not-implemented)
22. [Examples](#examples)

---

## Programs

A linked executable needs `main`. Return `i32` (used as the process exit code)
or `void`:

```sere
def main() -> i32:
    return 0
```

or with arguments:

```sere
def main(argv: list[str]) -> i32:
    return len(argv)
```

A file without `main` still typechecks and can emit LLVM; it does not produce a
C `main`. Top-level statements in the entry file run as module initialization
before `main`.

`prelude.sere` is injected automatically. Other stdlib modules are opt-in
(`import math`).

A typed binding may omit an initializer; the slot is default-initialized.
`=` always requires an expression.

```sere
ptr: Unique[i32]          # ok, default
n: i32 = 0                # ok
# n: i32 =                # error
```

Indent with **spaces only**. Tabs are a diagnostic.

---

## Lexical structure

### Comments

`#` to end of line. `# type: ignore` and `# type[NameError]: ignore` suppress
diagnostics (see [Diagnostics](#diagnostics)).

### Names

Identifiers: ASCII letters, digits, and `_`. Keywords are reserved.

### Keywords

```
False  None  True
and  as  assert  async  await  break  case  class  const  continue
def  defer  del  elif  else  enum  except  extern  finally
for  from  if  import  in  is  lambda  macro  match
not  or  pass  raise  return  static  struct  super
try  type  while  with
```

`const` binds a readonly name. `lambda` is an anonymous function. `with`
requires `__enter__` / `__exit__` on the context type. `async` and `await`
are reserved but not ready to use yet — see
[Reserved, not implemented](#reserved-not-implemented).

### Literals

| Kind | Forms |
| --- | --- |
| Integer | decimal `42`, hex `0xFF`, binary `0b1010`, octal `0o755`, `_` separators `1_000` |
| Float | `1.0`, `3e2`, `1.0f`, `_` allowed |
| Bool | `True`, `False` |
| None | `None` |
| String | `"..."`, multi-character `'...'`, `"""..."""` (multiline) |
| Byte | one-character `'A'` / `'\n'` — type `i8`, assignable to `byte` (`u8`) |
| F-string | `f"hi {x}"` with `{expr}` holes |
| Regex | backtick `` `\d+` ``, type `regex` |

### Operators

Arithmetic: `+ - * / // % **`
Bitwise: `& | ^ ~ << >>`
Comparison: `== != < <= > >=` `is` `in`
Boolean: `and` `or` `not`
Assignment: `=` `+=` `-=` `*=` `/=` `//=` `%=` `**=` `&=` `|=` `^=` `<<=` `>>=`
Inc/dec: `++n` `n++` `--n` `n--`
Pointers: `&x` `*p`
Cast: `value as T`
Call/index: `f(x)` `xs[i]` `xs[a:b]`
Walrus: `name := expr`
Other: `.` `,` `:` `->` `=>` `!` `$` `@` `...` `|` (unions and bitwise or)

`/` is true division. `//` is floor division.

---

## Types

### Primitives

| Type | Meaning |
| --- | --- |
| `void` | No value (function returns) |
| `None` | Python-style spelling of `void`; also the no-value literal |
| `bool` | `True` / `False` |
| `i8` `i16` `i32` `i64` | Signed integers |
| `u8` `u16` `u32` `u64` | Unsigned integers |
| `f32` `f64` | IEEE floats |
| `str` | String |
| `byte` | Alias of `u8` |
| `regex` | Compiled pattern (backtick literal) |

Prelude aliases (unions):

```sere
type Int = i8 | i16 | i32 | i64 | u8 | u16 | u32 | u64
type Float = f32 | f64
```

### Pointers

| Type | Meaning |
| --- | --- |
| `Unique[T]` | Exclusive heap pointer; dropped at end of scope |
| `Shared[T]` | Reference-counted heap pointer |
| `Ptr[T]` | Raw pointer; caller `free`s |

### Collections

| Type | Meaning |
| --- | --- |
| `list[T]` | Runtime list |
| `list[T, N]` | List of `T` with a fixed length `N` |
| `array[T]` | Fixed array from `array[T](...)` |
| `dict[K, V]` | Map |

### Callables

Functions, lambdas, classes, structs, and bound instance methods (`self.method`) can be passed as values. `self` is already applied, so `def handler(self, x: i32)` matches `Callable[[i32], R]`.

| Type | Meaning |
| --- | --- |
| `Callable` | Any callable; argument count and return type are unchecked |
| `Callable[R]` | Any callable that returns `R` |
| `Callable[[P...], R]` | Callable with those parameter types and return `R` |
| `Callable[[P..., ...], R]` | Prefix parameters must match; extra arguments are allowed |
| `Function` / `Function[R]` / `Function[[P...], R]` | Same shapes, but only functions and lambdas (not classes) |
| `Class` | Any class or struct type object |
| `Class[T]` | The type object for `T` (same as `type[T]`) |

```sere
def add(a: i32, b: i32) -> i32:
    return a + b

def apply(cb: Callable[[i32, i32], i32], x: i32, y: i32) -> i32:
    return cb(x, y)

def twice(cb: Callable[i32], n: i32) -> i32:
    return cb(n) + cb(n)

def call_any(cb: Callable) -> void:
    cb(1, 2)

n = apply(add, 1, 2)
m = twice(lambda (x: i32) -> i32: x + 1, 3)
call_any(add)

class Point:
    x: i32
    y: i32
    def __init__(self, x: i32, y: i32) -> void:
        self.x = x
        self.y = y

def make(cls: Class[Point], x: i32, y: i32) -> Point:
    return cls(x, y)

def construct(cb: Callable[[i32, i32], Point], x: i32, y: i32) -> Point:
    return cb(x, y)

p = make(Point, 1, 2)
q = construct(Point, 3, 4)
```

`Callable[[i32, ...], i32]` accepts `def f(a: i32) -> i32` and `def g(a: i32, b: i32) -> i32`.
A bare `Callable` result is `Any`; annotate `Callable[R]` when the return value is used as a typed result.

### User types

- `class` — identity (reference)
- `struct` — copy-by-value
- `enum` — discriminant; variants `Color.Green`
- `type Name = ...` — alias or union

### Unions

```sere
n: i32 | f32 = 3
type Number = i32 | i64
wide: i64 = 10
total: i64 = small + wide    # integer widths mix in arithmetic
```

Cast with `as` or a constructor: `n as i32`, `i32(tone)`, `T(value)`,
`Unique[T](pointer)`.

---

## Names and bindings

```sere
n: i32 = 0
static module_count: i32 = 0

def bump() -> i32:
    static n: i32 = 0
    n = n + 1
    return n
```

- Local: `name: Type` or `name: Type = expr`
- Module-level `static` and function-level `static` persist
- Functions and types can be aliased: `donut = print`

A declaration may be preceded by `@` decorator lines. Reserved modifier
decorators (`@public`, `@private`, `@static`, `@abstract`, `@override`,
`@frozen`, `@flags`) and user-defined runtime decorators are covered in
[Decorators](#decorators).

---

## Expressions

Precedence, high to low (roughly): postfix → unary → `as` → range →
`* / // % **` → `+ -` → shifts → `&` → `^` → `|` → comparisons / `in` / `is` →
`and` → `or` → ternary `a if c else b`.

```sere
xs: list[i32] = [1, 2, 3]
ages: dict[str, i32] = {"ada": 36}
empty: dict[str, i32] = dict[str, i32]()
arr: array[i32] = array[i32](1, 2, 3)
comp: list[i32] = [x for x in range(0, ..., 3)]
label: str = f"n={n}"
ok: bool = "ell" in hello and n > 0 and not False
```

`range` is an intrinsic that yields `list[i32]`:

| Call | Meaning |
| --- | --- |
| `range(stop)` | `0, 1, …, stop-1` |
| `range(start, stop)` | `start … stop-1` |
| `range(start, stop, step)` | stepped |

`start ... stop` desugars to `range(start, stop)`. A bare `...` in a call
argument list is skipped, so `range(0, ..., 3)` is `range(0, 3)`.

Ternary is Python-style: `x if cond else y`.

### Calls

Positional and keyword arguments may mix; keyword arguments must name a
parameter: `scale(factor=3, value=1)`. Keyword arguments are **not** supported
on indirect calls through a `Callable` or a function-typed value — use
positional form there.

### Comprehensions

`[expr for name in iterable]` builds a `list`. The iterable may be a `range`,
`list`, or `str`:

```sere
squares: list[i32] = [x * x for x in range(0, 5)]
```

### Truthiness and equality

`bool`, integers, and floats test directly in `if` / `while`. `is` compares
identity (enum variants, `None`); `==` compares value equality and works on
strings, numbers, lists, dicts, and user types with `__eq__`-style arithmetic
or comparison dunders.

### Walrus

`name := expr` assigns and yields the value. If `name` already exists it must
be assignable (and not `const`); otherwise it is declared with the inferred
type:

```sere
if (n := next()) > 0:
    print(n)
```

---

## Statements

```sere
pass
assert cond
assert cond, "failed"
return expr
break
continue
n = n + 1
n += 2
++n
n++
```

### `if` / `while` / `for`

```sere
if n == 1:
    n = n + 2
elif n == 0:
    pass
else:
    n = -n

while n < 4:
    n = n + 1

for n in range(0, 4):
    total += n
for item in xs:
    total += item
for ch in hello:
    pass
```

`for` iterates `range(...)`, `str` (one-character strings), and `list[T]`.

### `defer` / `del`

```sere
defer free(raw)
defer:
    print("done")
del xs[i]
del table[key]
```

`defer` queues its body and runs it in reverse order on every `return` (and
on the implicit return at the end of the function). `del` removes a list index
or dict key. `del name` is a diagnostic.

### `with` / `const` / `lambda` / tuples

```sere
const limit = 4
pair = (1, 2)
a, b = pair
add1 = lambda (x: i32) -> i32: x + 1
if (n := 3) > 0:
    print(n)
with Guard() as value:
    print(value)
```

Untyped `lambda x: ...` parameters are `Any`. Lambdas do not capture enclosing
locals; pass values as parameters, or use a nested `def`, which can capture.
`with` calls `__enter__` and `__exit__` on one evaluated context object.

---

## Functions

```sere
def scale(value: i32, factor: i32 = 2) -> i32:
    return value * factor

def identity[T](value: T) -> T:
    return value
```

- Return type after `->` may be omitted: `main` infers `i32`, `__init__`
  infers `void`, other functions infer `Any`
- Parameter types may be omitted (`Any`)
- Default arguments are allowed
- Keyword arguments at call sites: `scale(factor=3, value=1)`
- Varargs and kwargs: `def log(prefix: str, *parts: list[str], **opts: dict[str, str]) -> void`
- `print(..., sep=" ", end="\n")` — `end=""` suppresses the trailing newline
- Generic type parameters: `[T]` on `def` or `class`
- Methods take `self` as the first parameter
- Nested `def` is allowed and may capture enclosing locals — useful for
decorator wrappers and closures (lambdas still cannot capture)

Native:

```sere
extern "C" "native_add"
def add(left: i32, right: i32) -> i32
```

The string is the link symbol. The `def` has no body.

### Constrained generic parameters

Add `: Type` or `: Type1 | Type2` after a generic parameter name to restrict its
allowed types:

```sere
def identity[T: i32 | f64](value: T) -> T:
    return value

class Box[T: i32 | str]:
    value: T

enum Value[T: i32 | str]:
    Item(T)

def main() -> i32:
    number: i32 = identity[i32](3)
    real: f64 = identity(2.5)
    box = Box[str]("hello")
    value = Value.Item("text")
    return 0
```

The same syntax works on structs and generic methods. Each parameter has its
own constraint; unrestricted parameters can appear alongside constrained ones:
`class Pair[K: i32 | str, V]`.

Constraints are checked at compile time for explicit type arguments and for
arguments inferred from function calls or enum payloads. `identity[str]("no")`
and `identity("no")` both report `TypeError`. A constrained class or struct still
requires explicit constructor type arguments, such as `Box[i32](3)`.

Allowed type arguments match exactly after resolving aliases. A constraint of
`i32 | f64` rejects an inferred `i64` or `Any`; a constraint naming a class does
not also admit its subclasses. Convert the value first, or choose an allowed
explicit type argument and use the usual argument-conversion rules.
Concrete collection types are also valid,
for example `T: list[i32] | str`. Constraints must name concrete types; `Any`,
`void`, and other generic parameters are not allowed in the constraint itself.
Omitting a constraint (`[T]`) retains unrestricted generic behavior.

The `|` in a constraint lists alternative type arguments. It does not change
`T` into a union-valued variable: each specialization still has one chosen type.
An annotation on a constrained record must supply its type arguments, too;
bare `Box` cannot silently select `Any`.

See [generic_constraints.sere](../examples/generic_constraints.sere) for an
executable example with functions, classes, methods, and enum payloads.

---

## Decorators

Sere has two kinds of decorator:

- **Reserved modifiers**, handled at compile time: `@public`, `@private`,
  `@static`, `@abstract`, `@override`, `@frozen`, `@flags`.
- **Runtime decorators**: any user callable applied with `@name`,
  `@name(args)`, or `@Class.method`.

Both kinds may appear in the same stack. Decorators run **bottom-up** — the
one closest to the declaration runs first, so `@a` above `@b` on `f` means
`f = a(b(f))`.

### Reserved decorators

| Decorator | On | Effect |
| --- | --- | --- |
| `@public` / `@private` | field, `def`, property accessor, `class`, `struct`, `enum`, `type`, `macro`, module binding | `@private` is not exported: `import` / `from` and `module.name` cannot see it (`PermissionError`). Private methods and setters are only usable inside the owning class. |
| `@static` | field | Same as `static name: T`: one shared class variable |
| `@abstract` | method | Empty/`pass` body must be overridden; a real body is a default hook |
| `@override` | method | Marks an override |
| `@frozen` | class | Fields are not assignable after init |
| `@flags` | enum | Variants are bit flags; `Flag.A in mask` is a bitwise test |

Reserved decorator names cannot be redefined and never produce a runtime
wrapper.

### Runtime decorators

A runtime decorator is a callable that receives the decorated object and
returns its replacement:

```sere
def identity(fn: Callable) -> Callable:
    return fn

def logged(fn: Function[[i32], i32]) -> Function[[i32], i32]:
    def wrapper(n: i32) -> i32:
        print("logged", n)
        return fn(n)
    return wrapper

@identity
@logged
def bump(n: i32) -> i32:
    return n + 1
```

- `@name(args)` is a **factory**: the call is evaluated and its result is the
decorator.
- `@Class.method` uses a class method as the decorator.
- Decorating a `class` / `struct` passes the **type object** to the decorator.

Runtime decorators run **once, at module initialization**, before `main`. The
wrapped value is stored and every later call to the name goes through it. A
decorator whose static signature is unknown (it returns a bare `Callable`)
leaves the original signature intact, Python-style. Nested `def` wrappers may
capture the decorated function.

The full reference — type-checking rules, factory and class-method patterns,
class decoration, and diagnostics — is in **[decorators.md](decorators.md)**.

---

## Classes, structs, and enums

### Class (identity)

```sere
class Pet:
    name: str

    def __init__(self, name: str) -> void:
        self.name = name

    def id(self) -> i32:
        return 1

class Cat(Pet):
    def __init__(self, name: str) -> void:
        super().__init__(name)

class Box[T]:
    value: T

    def get(self) -> T:
        return self.value

class Animal:
    @abstract
    def speak(self) -> i32:
        pass
```

Construct with `Pet("z")` or `Box[i32](4)`. Multiple bases are allowed
(`class Dog(Animal, Named)`). `import pets` then `class Cat(pets.Pet)` is the
same base as `from pets import Pet` then `class Cat(Pet)`.

`static` fields are one shared value for the class (not per instance).
Read and write them as `MyClass.x` or `self.x`:

```sere
class Counter:
    @public static total: i32 = 0

    def __init__(self) -> void:
        Counter.total = Counter.total + 1
```

`@static` on the field is the same as the `static` keyword.

### Properties

`name.get` and `name.set` are accessors for `obj.name` / `obj.name = value`.
The backing field may reuse the same name. Inside the accessor (and in
`__init__` when a stored field exists), `self.name` is the field. Everywhere
else it goes through the getter or setter.

Visibility is per accessor: a public getter with a private setter is
read-only from outside the class.

```sere
class Vec2:
    @private x: i32
    @private y: i32

    def __init__(self, x: i32, y: i32) -> void:
        self.x = x
        self.y = y

    @public x.get:
        return self.x

    @public y.get:
        return self.y

    @public x.set(value: i32) -> void:
        self.x = value

    # equivalent: x.set(self, value: i32) -> void

v.x        # getter
v.x = 10   # setter; the setter always receives self
```

A getter may omit `()` and `-> T` (the field type is used). A setter takes
one value besides `self` and returns `void`. A getter without a setter is
read-only. A computed property may omit the field and only declare accessors.

### Struct (value)

```sere
struct Point:
    x: i32
    y: i32

    def length_sq(self) -> i32:
        return self.x * self.x + self.y * self.y

p: Point = Point(1, 2)
q: Point = p     # copy
q.x = 9          # p.x stays 1
```

Structs cannot inherit.

### Enum

```sere
enum Color:
    Red
    Green = 2
    Blue

enum Message:
    Quit
    Move(x: i32, y: i32)
    Write(str)

    def is_quit(self) -> bool:
        match self:
            case Message.Quit:
                return True
            case _:
                return False
```

- Unit variants: `Color.Green`
- Payload variants: `Message.Move(1, 2)`
- `.name` → `str`, `.value` → discriminant, `i32(tone)` → tag
- Unit / `@flags` enums (no payload) are integers in context: pass to `i32`
  parameters, assign to `i32`, compare with ints, and use `| & ^` without `.value`
- `Color.variants()` → `list[str]`
- `tone is Color.Green` compares identity of the variant
- `@flags` on an enum marks it as a flag set; `Flag.A in mask` is a bitwise test

### Dunder methods

If a type defines these, the corresponding syntax uses them:

| Method | Syntax |
| --- | --- |
| `__init__` | `T(...)` |
| `__len__` | `len(x)` |
| `__getitem__` / `__setitem__` | `x[i]` / `x[i] = v` |
| `__contains__` | `v in x` |
| `__enter__` / `__exit__` | `with x as name:` |
| `__add__` / `__radd__` and other arithmetic | `+ - * / // % **` and comparisons |

---

## Modules and imports

```sere
import util
import util as u
from util import double
from html_lang import html, Html
from math import sqrt
from window import Window as BaseWindow
from string import *
```

`import gl` binds only the module name. A local `class Window` is a different
type from `gl.Window`.

`from SomeClass import someMethod` in the same file hoists a class method to
module scope so it can be exported. `from module import Name as Alias` binds
the export under `Alias`.

Public top-level names are exported by default. `__exports__` can also list
names brought in with `from other import Name` (or `as Alias`) so a barrel
file can re-export them:

```sere
def version() -> str:
    return "1"

from Greeter import hello
from window import Window as BaseWindow

__exports__ += [hello, BaseWindow]   # keep version, also re-export these
# __exports__ = [hello]              # export only hello
```

Search order: directory of the importing file (and `libs/` next to it), the
current working directory, then the stdlib next to `sere` (or `SERE_STDLIB`
in tests). `util.sere` and `util.slib` both provide module `util`. `.sere`
wins when both exist. A folder named `util` with `util/util.sere` or
`util/lib.sere` is also `import util` (native `.c` / `.lib` in that folder or
`native/` are compiled and linked). The compiler extracts `.slib` files next
to themselves under `.sere-lib/` and links any native objects they contain.
`.slib` packs only the entry and the local modules it actually imports, plus
compiled native objects — not the rest of the tree. The language server uses
the same search path, so drop-in `.slib` files and folder libraries complete
and hover like ordinary modules.

Module globals (always in scope):

| Name | Meaning |
| --- | --- |
| `__name__` | `"__main__"` for the entry file, otherwise the module stem |
| `__file__` | Source path |
| `__package__` | Package string |
| `__doc__` | Leading docstring if present |
| `__debug__` | True in a debug-oriented build flag |
| `__sere_version__` | Compiler version string |

Compile-time host flags (bool). A false `if` branch is not typechecked:

| Flag | Meaning |
| --- | --- |
| `__windows__` `__linux__` `__macos__` `__unix__` | OS |
| `__x86_64__` `__arm64__` | Architecture |
| `__platform__` | `"windows"` / `"linux"` / `"macos"` |
| `__arch__` | `"x86_64"` / `"arm64"` / `"unknown"` |

```sere
if __windows__:
    windows.message_box("hi")
if cfg!(linux):
    pass
```

`cfg!(windows)` (and `linux`, `macos`, `unix`, `x86_64`, `arm64`, `debug`) is a
prelude macro that expands to the matching dunder.

---

## Memory and pointers

```sere
owned: Unique[i32] = unique[i32](42)
*owned = 43
value: i32 = load(owned)

raw: Ptr[i32] = alloc[i32]()
store(raw, 7)
*raw = 8
free(raw)

local: i32 = 10
stack: Ptr[i32] = &local
*stack = *owned
```

| Intrinsic | Result |
| --- | --- |
| `unique[T](value)` | `Unique[T]` |
| `shared[T](value)` | `Shared[T]` |
| `alloc[T]()` | `Ptr[T]` |
| `load(p)` | `T` |
| `store(p, v)` | `void` |
| `free(p)` | `void` |
| `&x` | `Ptr[T]` for an addressable lvalue |
| `*p` | load `T`; `*p = v` stores |

`alloc` / `free` go through the installed collector (`import gc`). Default
collector is `"none"` (tracked malloc; you free it). Switch with
`gc.use("mark_sweep")` or `gc.use("arena")`. Arenas and pools: `import heap`.
Custom collectors: implement `SereGcVTable` in C, call `sere_gc_install` from
`sere_mod_init`, link with `sere main.sere --link my_gc.lib`.

---

## Collections and strings

```sere
xs: list[i32] = [1, 2, 3]
xs.append(4)
xs[0] = 10
print(len(xs), xs[1], xs[1:], xs[:2], xs[1:3], xs[:])

hello: str = "Hello"
assert hello[0] == "H"
assert hello[-1] == "o"
assert hello[1:4] == "ell"
assert "ell" in hello
assert hello + "!" == "Hello!"
assert hello * 2 == "HelloHello"
```

`len` works on `list`, `array`, `dict`, `str`, and types with `__len__`.
`xs.append(v)` and `append(xs, v)` both add to a list.

Dicts: `ages["ada"] = 37`. Empty: `dict[str, i32]()`.

---

## Pattern matching

```sere
match moved:
    case Message.Move(x, y):
        assert x == 1
    case Message.Quit if n > 0:
        pass
    case _:
        assert False
```

`case _` is the wildcard. Enum payloads bind names in the arm. `case pat if expr`
is a guard.

When the subject is an enum, the `match` must be **exhaustive**: every variant
is covered, or the last arm is `case _`. Missing variants are a compile-time
diagnostic (`match is not exhaustive; missing Color.Blue`). Non-enum subjects
have no exhaustiveness requirement.

Payload bindings are plain names (`case Message.Move(x, y)`); the binding type
is the declared payload type. Arms are checked in order; the first matching arm
runs.

---

## Errors

```sere
try:
    raise TypeError("nope")
except TypeError as e:
    print(e.message)
```

`try` needs `except` and/or `finally`. Optional `else` (no error) and `finally`
(always). `except Type` matches that class and its subclasses. Bare `except:`
catches everything. `as e` binds an instance with `.message`.

```sere
try:
    risky()
except ValueError as e:
    print(e.message)
except RuntimeError:
    print("known failure")
else:
    print("no error")
finally:
    print("always")
```

Multiple `except` clauses are allowed; the first matching handler runs.
Exceptions propagate out of functions and `for` / `while` bodies until a
handler or the top level. `defer` bodies run on the way out, including when an
exception unwinds the function.

Builtin exception classes (all subclass `Exception`):
`SyntaxError`, `IndentationError`, `NameError`, `AttributeError`, `TypeError`,
`IndexError`, `ImportError`, `ValueError`, `AssertionError`, `PermissionError`,
`RuntimeError`, `RecursionError`, `NotImplementedError`.

```sere
class Boom(ValueError):
    pass

raise Boom("bad")
raise "boom"                 # Exception
raise TypeError              # empty message
assert False, "fail"         # AssertionError, catchable
```

`raise` stringifies the first constructor argument (or a bare `str`). Bare
`raise` uses an empty `Exception`. `panic("msg")` still aborts. Prelude macros:

```sere
todo!("not yet")
unreachable!()
dbg!(total)          # prints and yields total
```

---

## Macros

Macros run **after parse, before type checking**. They are hygienic by default
(names introduced in a `quote` do not capture caller names). Expansion depth
is capped (diagnostic `RecursionError`).

### Quote

```sere
macro twice(x):
    quote:
        ($x) + ($x)

total: i32 = twice!(n)
```

Splice with `$x`. Repeats: `$($x),*` inside `quote`. `$type` is available when
the macro sets `typed: true` (the type of the first argument).

### Match (token trees)

```sere
macro vec:
    match:
        ($($x:expr),*) => quote:
            [$($x),*]

xs: list[i32] = vec!(1, 2, 3)
```

Specs include `expr`, `ident`, `literal`.

### Indent / raw / pipeline

Statement form: `name:` plus an indented body. Expression form only after `=`:

```sere
node: Html = html:
    <div>{title}</div>

n: i32 = pipeline:
    1
    |> add2
    |> wrap(4)
```

Do not write `if left < right:` as a macro; `ident:` newline after a comparison
is the suite colon.

Macro properties:

```sere
macro html:
    syntax: raw          # raw | tokens | pipeline | (default sere)
    interpolate: brace   # brace | dollar
    wrapper: Html        # constructor around the result
    typed: true
```

Invocations:

- `name!(...)`  `name!{...}`  `name![...]`
- indent `name:` (statement, or initializer after `=`)

Import macros like any name: `from html_lang import html, Html`.

---

## Introspection and platform

Always in scope:

```sere
typeof(small)              # str
isinstance[i32](small)
isinstance(small, i32)
isinstance(window, gl.Window)
isinstance(xs, list[f32])
typeof(window) is gl.Window
dir(Box)                   # list[str]
dir()
inspect(scale)             # str
sizeof[i32]()              # i64
alignof[i32]()             # i64
x.__name__  x.__type__  x.__module__  x.__qualname__
Type.__name__
```

`from inspect import label` is extra helpers, not the builtins.

---

## Intrinsics

These names are compiler primitives (see `IntrinsicKind`):

`unique` `shared` `alloc` `free` `load` `store` `len` `print` `str` `repr`
`parse` `try_parse` `append` `list` / list construction `array` `dict` `range`
`typeof` `isinstance` `dir` `inspect` `sizeof` `alignof` `panic` `super`

`print` accepts any printable value, including pointers. `str(x)` converts.
`repr(x)` also converts (debug-oriented spelling for a value).

### Conversion intrinsics

| Call | Meaning |
| --- | --- |
| `str(x)` | Convert any value to its `str` form |
| `repr(x)` | Convert any value to its `str` form (debug spelling) |
| `parse[T](text)` | Parse `text` as `T` (`parse[i32]("123")`), `T | None` on failure |
| `try_parse[T](text)` | Parse `text` as `T`, yielding `T | None` instead of a hard failure |

Integers parse from decimal (plus `0x` / `0b` / `0o` prefixes); floats parse
from decimal and exponent forms. `bool` parses `"True"` / `"False"`.

### Input

| Call | Meaning |
| --- | --- |
| `input(prompt: str)` | Print `prompt`, read one line from stdin, return it as `str` (from the prelude) |

---

## Standard library

Injected: `stdlib/prelude.sere` (`abs`, `min`, `max`, `clamp`, `sign`, `input`,
`Int` / `Float`, the `Exception` class hierarchy, macros `dbg!` `todo!`
`unreachable!` `cfg!`).

Import the rest:

| Module | Role |
| --- | --- |
| `io` | `read_line`, `eprint` |
| `fs` `path` `os` `env` `sys` | Files, paths, process, host |
| `string` `bytes` `encoding` `regex` | Text and binary |
| `math` `vec` `matrix` `ml` `arrays` | Numeric / linear algebra |
| `hash` `random` `time` `log` `bit` | Utilities |
| `gc` `heap` `memory` | Collectors, arenas (memory is documentation) |
| `inspect` | Extra labels (`label`, `describe`) |
| `util` | Tiny helpers (`double`); used by import examples |
| `html_lang` | `html:` raw macro + `Html` |
| `windows` | Win32 message box, beep, clipboard, … (stub off Windows) |
| `gl` | OpenGL 2.1+ (WGL window, `should_close`, shaders, VBO/VAO, textures, FBO, input) |
| `qt6` | Qt 6 widgets; linked automatically if the compiler was built with Qt |

Failed C bindings typically return `""` / `0` / `False` rather than throwing.
Gate OS-only code with `if __windows__:`.

More on how stdlib is wired: [stdlib.md](stdlib.md).

---

## Native interop

```sere
extern "C" "sere_gc_collect"
def collect() -> void
```

Link extra object files or libs:

```powershell
sere src\main.sere --link libs\native.lib -o bin\app.exe
```

Packed `.slib` files and folder libraries (`mylib/lib.sere` plus `mylib/*.c`
or `mylib/native/`) compile and link their C automatically. Prefer packing
the compiled `.lib` / `.a` into the `.slib` so consumers stay on one file.

Optional module init: define `void sere_mod_init(void)` in C. The runtime
provides an empty default. A strong definition from `--link` overrides it.
`sere_mod_init` runs from generated `main` before Sere globals.

Headers: `include/sere/api/sere_mod.h`, `sere_gc.h`.

---

## Diagnostics

Reported as `error[NameError]: ...` (and other exception names).

| Exception | Typical cause |
| --- | --- |
| `SyntaxError` | Parse |
| `IndentationError` | Mixed or inconsistent indent |
| `NameError` | Unknown name, type, function, macro, module |
| `AttributeError` | Unknown field or method |
| `TypeError` | Wrong type, operand, or argument |
| `IndexError` | Bad index or slice |
| `ImportError` | Missing module or prelude |
| `ValueError` | Invalid or uninferable value |
| `AssertionError` | Bad `assert` |
| `PermissionError` | Private field |
| `RuntimeError` | Control-flow / compiler internal |
| `RecursionError` | Macro expansion limit |
| `NotImplementedError` | Unsupported or leftover construct |

Suppression:

```sere
# type[NameError]: ignore          # whole file if at the top
def main() -> i32:
    n: i32 = "nope"                # TypeError still reported
    return missing                 # NameError ignored (file rule)

    return missing  # type: ignore              # this line
    # type: ignore
    return missing                              # next statement
```

`# type[Exception]: ignore` hides every diagnostic in scope.
`# type: ignore[NameError]` is accepted. Unknown names in the ignore list
are `ValueError`.

`sere --analyze file.sere` prints JSON diagnostics. The editor uses `sere --lsp`.

---

## Reserved, not implemented

Sere is a **typed Python superset**, not CPython. These remain out of scope or
incomplete. They diagnose instead of generating silent wrong code:

- keyword-only parameters (after `*args`), `global` / `nonlocal`
- `yield` / generator functions (a nested `def` is fine — see
  [Functions](#functions))
- Unmodified CPython stdlib (use Sere modules such as `requests` and `wsgi`)
- Lambda capture of enclosing locals (pass parameters instead, or use a nested
  `def`, which can capture)
- `del name` (only `del xs[i]` / `del d[k]`)
- `print x` as a statement (`print` is a call)

If a construct parses but lowering is incomplete, you get
`NotImplementedError` rather than silent wrong code.

### Async / await status

`async def`, `await`, and the `Task[T]` / `Future[T]` types parse and
type-check. Calling an `async def` yields a `Task[T]`; `await` unwraps it back
to `T` and may only appear inside an `async def`:

```sere
async def number() -> i32:
    return 42

async def combine() -> i32:
    a = await number()
    b = await number()
    return a + b

async def main(argv: list[str]) -> i32:
    print(await combine())
    return 0
```

The runtime lowering (LLVM switched-resume coroutines and the `coro-*`
splitting passes) is under active development and is **not currently
reliable** — compiled async programs are not guaranteed to build or run
correctly. Treat `async` / `await` as a preview feature until this note is
removed.

---

## Examples

Under `examples/`:

| File | Shows |
| --- | --- |
| `hello.sere` | `print`, f-strings, prelude math |
| `control.sere` | `if` / `while` / `break` / `continue` |
| `lang.sere` | `for`, defaults, enums |
| `features.sere` | unions, comprehensions, default constructors |
| `types.sere` | `Int` / `Float` aliases (no `main`) |
| `collections.sere` | list, array, dict, `@public` / `@private`, `static` |
| `strings.sere` | quotes, index, slice, `in`, `+`, `*` |
| `structs.sere` | value types |
| `enums.sere` | payloads, `match`, `.name` / `.value` |
| `enum_print.sere` | `print` an enum, `main -> void` |
| `oop.sere` | inheritance, `super`, generics, `++` |
| `point.sere` | class methods returning `self` type |
| `properties.sere` | `x.get` / `x.set` used as `v.x` / `v.x = 10` |
| `dunders.sere` | `__getitem__` / `__len__` / `__contains__` |
| `errors.sere` | `try` / `raise` |
| `memory.sere` | `Unique`, `Ptr`, `&` / `*` |
| `introspect.sere` | `typeof`, `dir`, module dunders |
| `imports.sere` | `import` / `from` |
| `aliases.sere` | function aliases |
| `macros_*.sere` | quote, match, HTML, pipeline, hygiene |
| `native_add.sere` | `extern "C"` |
| `stdlib_mods.sere` / `stdlib_more.sere` | fs, math, string, regex, hash, … |
| `numeric.sere` | vec, matrix, ml, bytes |
| `gc_mem.sere` | collectors, arenas, pools |
| `qt6_app.sere` | Qt widgets |
| `gl_info.sere` / `gl_triangle.sere` / `platform.sere` | GL API surface / triangle / host flags |
| `colors.sere` | unit enum |
| `pythonish.sere` | untyped params, inferred locals, `const`, `//=` `**=` |
| `walrus.sere` / `lambda.sere` / `tuples.sere` | `:=`, lambda, tuple unpack |
| `defer.sere` / `with_ctx.sere` / `del_list.sere` | defer, with, del |
| `flags_enum.sere` | `@flags` and `in` |
| `requests.sere` / `wsgi.sere` | HTTP client and WSGI-shaped server types |

Internals of the compiler: [README.md](README.md) in this folder.
How to add a keyword or module: [extending.md](extending.md).
