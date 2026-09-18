# Classes, structs, and operator overloading

Two user-defined record forms:

| Form | Keyword | Semantics |
| --- | --- | --- |
| Class | `class Name:` | reference type with a runtime type id; supports inheritance |
| Struct | `struct Name:` | value type; copied on assignment |

Both may declare fields and methods.

## Fields

```sere
class Account:
    owner: str              # public
    @private balance: i64   # private to the class
    @frozen id: i32         # assignable only during __init__
    @static count: i32      # one shared slot for the whole type
```

Assigning to a `@frozen` field outside `__init__` is a compile error, as is
touching a `@private` field from outside the class.

## Constructor

```sere
class Account:
    owner: str
    @private balance: i64

    def __init__(self, owner: str, balance: i64 = 0) -> void:
        self.owner = owner
        self.balance = balance

a = Account("ann")          # defaults are filled in
b = Account("bo", 100)
```

`__init__` returns `void` or `None`. Defaults, keyword arguments, `*args` and
`**kwargs` behave as for free functions — see [functions.md](functions.md).

## Methods

```sere
class Account:
    owner: str
    @private balance: i64

    def __init__(self, owner: str, balance: i64 = 0) -> void:
        self.owner = owner
        self.balance = balance

    def deposit(self, amount: i64) -> void:
        self.balance += amount

    def total(self) -> i64:
        return self.balance
```

The first parameter must be named `self`. Methods are called with `.` and are
dispatched virtually, so an override on a subclass runs even through a base-typed
value.

## Properties

A property is a method pair guarded by a backing field of the same name:

```sere
class Temperature:
    celsius: f64

    @property
    def fahrenheit(self) -> f64:
        return self.celsius * 1.8 + 32.0

    @fahrenheit.setter
    def fahrenheit(self, value: f64) -> void:
        self.celsius = (value - 32.0) / 1.8
```

## Inheritance

```sere
class Animal:
    name: str

    def __init__(self, name: str) -> void:
        self.name = name

    def speak(self) -> str:
        return "..."

class Dog(Animal):
    def speak(self) -> str:
        return "woof"

def describe(a: Animal) -> str:
    return super(a).speak() + " (" + a.speak() + ")"
```

`super(value)` gives the base-typed view; `a.speak()` still dispatches to `Dog`.
A method with no body (`pass` or an abstract marker) is abstract and must be
overridden.

## String conversion

| Method | Signature | Used by |
| --- | --- | --- |
| `__str__` | `(self) -> str` | `print`, `str(x)`, `f"{x}"` |
| `__repr__` | `(self) -> str` | `repr`, list/dict rendering |

Without `__str__`, a value renders as `{field: value, ...}`.

## Container protocols

| Method | Signature | Enables |
| --- | --- | --- |
| `__len__` | `(self) -> i64` | `len(x)` |
| `__getitem__` | `(self, index: T) -> U` | `x[i]` |
| `__setitem__` | `(self, index: K, value: V) -> void` | `x[i] = v` |
| `__contains__` | `(self, item: T) -> bool` | `item in x` |
| `__iter__` | `(self) -> Iterator[T]` | `for v in x` |
| `__bool__` | `(self) -> bool` | `not x` |

```sere
class Board:
    cells: list[i32]

    def __init__(self) -> void:
        self.cells = [0, 0, 0]

    def __len__(self) -> i64:
        return len(self.cells)

    def __getitem__(self, index: i32) -> i32:
        return self.cells[index]

    def __setitem__(self, index: i32, value: i32) -> void:
        self.cells[index] = value

    def __contains__(self, value: i32) -> bool:
        return value in self.cells
```

## Operator overloading

Define the method named for the operator. The second parameter's type decides
which right-hand operands are accepted: `obj + "x"` only compiles when `__add__`
takes a `str`.

```sere
class Vec:
    @frozen x: i32
    @frozen y: i32

    def __init__(self, x: i32, y: i32) -> void:
        self.x = x
        self.y = y

    def __add__(self, other: Vec) -> Vec:
        return Vec(self.x + other.x, self.y + other.y)

    def __mul__(self, factor: i32) -> Vec:
        return Vec(self.x * factor, self.y * factor)

    def __eq__(self, other: Vec) -> bool:
        return self.x == other.x and self.y == other.y

    def __lt__(self, other: Vec) -> bool:
        return self.x < other.x

    def __str__(self) -> str:
        return f"Vec({self.x}, {self.y})"

print(Vec(1, 2) + Vec(3, 4))   # Vec(4, 6)
print(Vec(1, 2) * 3)           # Vec(3, 6)
print(Vec(1, 2) == Vec(1, 2))  # True
```

### Method names

| Operator | Method | Reflected fallback |
| --- | --- | --- |
| `+` | `__add__` | `__radd__` |
| `-` | `__sub__` | `__rsub__` |
| `*` | `__mul__` | `__rmul__` |
| `/` | `__truediv__` | `__rtruediv__` |
| `//` | `__floordiv__` | `__rfloordiv__` |
| `%` | `__mod__` | `__rmod__` |
| `**` | `__pow__` | `__rpow__` |
| `&` | `__and__` | `__rand__` |
| `\|` | `__or__` | `__ror__` |
| `^` | `__xor__` | `__rxor__` |
| `<<` | `__lshift__` | `__rlshift__` |
| `>>` | `__rshift__` | `__rrshift__` |
| `==` | `__eq__` | `__eq__` on the right operand |
| `!=` | `__ne__` | falls back to `not (a == b)` |
| `<` | `__lt__` | `__gt__` |
| `<=` | `__le__` | `__ge__` |
| `>` | `__gt__` | `__lt__` |
| `>=` | `__ge__` | `__le__` |
| `~` | `__invert__` | — |

The **reflected** method is used when the left operand does not implement the
operator but the right operand does. Note the swapped comparisons: `a < b`
consults `b.__gt__(a)` when `a` has no `__lt__`.

```sere
class Offset:
    @frozen dx: i32

    def __radd__(self, other: i32) -> i32:
        return other + self.dx

print(5 + Offset(10))   # 15
```

### Rules and caveats

- The method must **return a value**; a `void` binary operator method is rejected.
- The other operand must be assignable to the second parameter, otherwise the
  operator overload is not used and the built-in rules apply (or a type error is
  reported for the operand types involved).
- `!=` without `__ne__` inverts `__eq__`'s result.
- Comparisons (`<`, `<=`, `>`, `>=`) on records that define no matching method
  are accepted between same-typed records but do not compare contents.
- Overloaded operators dispatch on the **static** type of the receiver: a
  subclass override is not reached through a base-typed variable. Wrap the call
  in a normal method if you need virtual dispatch.
- Compound assignment (`obj += x`) uses the same methods.

## See also

- [operators.md](operators.md) — the built-in operator rules.
- [enums.md](enums.md) — `match` on variants.
- [generics.md](generics.md) — generic classes.
