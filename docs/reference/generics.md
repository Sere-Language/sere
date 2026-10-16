# Generics

A generic declaration takes type parameters in `[ ]` after its name.

```sere
def identity[T](value: T) -> T:
    return value

class Box[T]:
    value: T

struct Pair[K, V]:
    key: K
    val: V

enum Option[T]:
    Some(T)
    None
```

Type arguments are inferred at the call site, or given explicitly:

```sere
n = identity(3)          # T = i32
s = identity[str]("hi")  # T = str
b = Box[i32](1)
```

A generic class or struct requires explicit constructor type arguments
(`Box[i32](1)`, not `Box(1)`); functions and enum payloads are generally
inferred.

Function inference examines supplied positional and named arguments recursively,
including nested lists and tuples. Optional parameters may be omitted: defaults
do not prevent inference from the other arguments. Explicit type arguments take
precedence, and the arguments still have to be assignable to the selected types.

```sere
def pair[A, B](a: A, b: B) -> (A, B):
    return (a, b)

number, text = pair(42, "hello")  # i32, str

def create[T]() -> list[T]:
    return []

names: list[str] = create()      # T comes from the annotated result
```

A result annotation can supply a type parameter that arguments leave undetermined.
Without that context, `create()` reports that `T` cannot be inferred; use an
annotation or `create[str]()`. Conflicting argument types are an error, rather
than an arbitrary choice of specialization. Calls with the same concrete types
reuse a specialization.

See [enumerate.sere](../../examples/enumerate.sere) for a complete example
combining inference, generic defaults, tuple returns, and loop destructuring.

## Constraints

Add `: Type` (or `: A | B`) after the parameter name to restrict the allowed
type arguments.

```sere
def identity[T: i32 | f64](value: T) -> T:
    return value

class Box[T: i32 | str]:
    value: T

enum Value[T: i32 | str]:
    Item(T)
```

- The `|` lists **alternative** type arguments — it does not make `T` a
  union-valued variable. Each specialization picks exactly one.
- Constraints are enforced for explicit arguments (`identity[str]("no")` →
  `TypeError`) and for inferred ones (`identity("no")` → `TypeError`).
- Matching is exact after resolving aliases. `i32 | f64` rejects an inferred
  `i64`, and a constraint naming a class does not also admit its subclasses.
- Concrete collections are valid: `T: list[i32] | str`.
- Constraints must name concrete types — `Any`, `void`, and other generic
  parameters are not allowed inside a constraint.
- Parameters are independent, and unrestricted parameters may sit beside
  constrained ones: `class Pair[K: i32 | str, V]`.
- Omitting the constraint (`[T]`) keeps the parameter unrestricted.

A constrained record still needs its type arguments everywhere it is named, so
`Box` alone does not silently become `Box[Any]`.

An executable example covering functions, classes, methods, and enum payloads is
[generic_constraints.sere](../../examples/generic_constraints.sere).

## Generic methods

A method may declare its own parameters, independent of the class:

```sere
class Stack[T]:
    items: list[T]

    def push(self, value: T) -> void:
        self.items.append(value)

    def map[U](f: Callable[[T], U]) -> list[U]:
        result: list[U] = []
        for item in self.items:
            result.append(f(item))
        return result
```

## Type parameters as annotations only

`Any` remains the escape hatch for genuinely mixed data. Prefer a type parameter
when the type is fixed per call, and `Any` only when it truly varies at runtime —
see [gradual-typing.md](../gradual-typing.md) for the conversion rules between
`Any` and concrete types.

## See also

- [functions.md](functions.md) — callable types and signatures.
- [classes.md](classes.md) — methods and operator overloading on generic types.
- [enums.md](enums.md) — payload variants holding `T`.
