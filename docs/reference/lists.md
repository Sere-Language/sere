# Lists

`list[T]` is a growable, heap-allocated, reference-counted/garbage-collected
sequence. Indexing is bounds-checked at runtime.

## Literals and types

```sere
xs = [1, 2, 3]                 # list[i32]
names: list[str] = ["a", "b"]
empty = []                     # needs an expected type to infer T
nested = [[1], [2, 3]]         # list[list[i32]]
mixed: list[i32 | str] = [1, "two"]
```

An empty literal has no element type of its own — give it an annotation.

## Length, indexing, slicing

```sere
xs = [10, 20, 30, 40]
n = len(xs)        # 4, i64
x = xs[0]          # 10
part = xs[1:3]     # [20, 30]
head = xs[:2]      # [10, 20]
tail = xs[2:]      # [30, 40]
copy = xs[:]       # shallow copy
```

Replacing through an index mutates in place:

```sere
xs[0] = 99
```

Slicing yields a **new** list; assigning to a slice is not allowed.

## Operators

| Expression | Result | Notes |
| --- | --- | --- |
| `a + b` | `list[T]` | concatenation; element types must match |
| `a * n`, `n * a` | `list[T]` | repetition; `n` is any integer |
| `x in a`, `x not in a` | `bool` | membership, compares elements by value |
| `a == b`, `a != b` | `bool` | **identity** — see below |

```sere
print([0] * 10)        # [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
print([1, 2] * 3)      # [1, 2, 1, 2, 1, 2]
print([1] * 0)         # []
print([1] + [2, 3])    # [1, 2, 3]
```

`*` copies elements **by value**. For a list of lists the outer list is new but
the inner lists are shared, exactly like Python's `[inner] * n`. Build distinct
inner values in a loop or comprehension when you need them.

## Comparison

`==` on two lists compares **identity**, not contents. Two separately built
lists with equal elements are not equal:

```sere
print([1, 2] == [1, 2])   # False
a = [1, 2]
print(a == a)             # True
```

Relational operators (`<`, `<=`, `>`, `>=`) are accepted between same-typed
lists but do not compare contents; use `in` or compare element by element.
Compare `str` and numbers by value as usual.

## Methods

| Method | Returns | Description |
| --- | --- | --- |
| `append(value)` | `void` | push onto the end (`push` is an alias) |
| `insert(index, value)` | `void` | insert before `index` |
| `pop()` / `pop(index)` | `T` | remove and return (last by default) |
| `remove(value)` | `bool` | remove the first match |
| `find(value)` / `index(value)` | `i64` | first offset, or `-1` |
| `count(value)` | `i64` | number of matches |
| `contains(value)` / `has(value)` | `bool` | membership |
| `clear()` | `void` | drop every element |
| `reverse()` | `void` | reverse in place |
| `copy()` / `clone()` | `list[T]` | shallow copy |
| `extend(other)` | `void` | append every element of `other` |

```sere
xs = [3, 1]
xs.append(2)          # [3, 1, 2]
xs.insert(0, 9)       # [9, 3, 1, 2]
last = xs.pop()       # 2, xs is [9, 3, 1]
print(xs.find(3))     # 1
xs.extend([7, 8])     # [9, 3, 1, 7, 8]
```

`append`, `insert` and `extend` check the element type, so
`list[i32].append("x")` is a compile error.

## Iteration

```sere
for x in xs:
    print(x)
```

`for` binds a fresh element each pass. Mutating the list while iterating is not
guarded — snapshot with `xs[:]` if you need to.

## Comprehensions

```sere
squares = [x * x for x in range(0, 5)]   # [0, 1, 4, 9, 16]
positives = [x for x in values if x > 0]
```

## Arrays

`array[T]` is the fixed-size sibling of `list[T]`. It supports indexing,
slicing, `len`, and the same repetition rules; see
[../language.md](../language.md#collections) and [../arrays.md](../arrays.md)
for size handling and numeric use.
