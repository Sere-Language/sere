# Named tuples

Named tuples are immutable values with named elements. Both declarations below
have the same structural type:

```python
type Size: (width: u32, height: u32)
type Resolution = (width: u32, height: u32)

size = Size(640, 480)
other = Resolution(height=1080, width=1920)
compatible: Resolution = size
```

Construct values with positional arguments, keyword arguments, or a mixture.
Every element is required. Each argument is checked against its element type;
names, missing arguments, duplicate arguments, and out-of-range literals are
checked at compile time.

## Reading and unpacking

```python
assert size.width == size.first
assert size.height == size.second
assert size.width == size[0]
width, height = size
```

Indexes must be integer literals within bounds. `first` and `second` remain
positional aliases. A declared name may use these spellings only at the matching
position, so `(first: i32, second: i32)` is valid but `(x: i32, first: i32)` is not.
Names must be unique; all elements in one tuple type must have names.

## Compatibility and immutability

Two named tuple types are compatible when their element names, order, and types
match. Their alias names do not affect compatibility. Renaming or reordering an
element, or changing its type, creates a different type. Ordinary unnamed tuple
types are distinct from named tuples.

Elements cannot be assigned, incremented, or accessed through writable pointers.
This applies to named, positional, and indexed access. A variable holding a named
tuple can be reassigned to another value of its type. Immutability applies to the
element slots; a mutable object held in an element retains its own behavior.

## Generic and nested types

```python
type Pair[T]: (left: T, right: T)
type WindowInfo = (size: Size, title: str)
type Identifier: (value: i32)

pair = Pair[str]("left", "right")
window = WindowInfo(Size(800, 600), "Editor")
identifier = Identifier(7)
```

A single named element is a tuple even without a trailing comma. Named tuple
types can appear inside other type expressions and generic aliases. `late`,
`auto`, access modifiers, defaults, and member methods are not supported in
named tuple element declarations.

LLVM and Serem support construction, access, structural compatibility, and
unpacking. The language server provides named member completion, type hover,
constructor signature help, and readonly semantic highlighting. The VS Code
extension includes named tuple declaration highlighting and snippets.
