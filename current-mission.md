# Sere 0.3.0 --- Language Extensions and New Features

**Release:** 0.3.0\
**Previous stable release:** 0.2.2\
**Status:** Proposed / In Development

------------------------------------------------------------------------

## Introduction

After some time away from active development, I'm preparing to resume
work on Sere.

Sere 0.2.2 is currently stable, but I've decided that it's time to
introduce several substantial language extensions. Rather than
incrementally adding these features through smaller releases, I've
decided to move directly toward **Sere 0.3.0**.

This release will focus primarily on expanding Sere's type system,
declaration semantics, and object-oriented capabilities. The goal is to
introduce more expressive language constructs while maintaining the
readability and simplicity that Sere aims to provide.

The following features are currently planned for 0.3.0.

------------------------------------------------------------------------

# 1. Custom Access Modifiers

One of the primary additions to Sere 0.3.0 will be the ability to define
custom access modifiers.

Currently, Sere provides built-in modifiers such as:

-   `@private`
-   `@public`
-   `@frozen`

With 0.3.0, developers will be able to define their own modifiers by
combining existing access modifiers and specifying custom getter and
setter behavior.

### 1.1 Defining custom modifiers

Custom modifiers will be declared using the `type` keyword:

``` python
type @internal:
    get = @public
    set = @private
```

In this example, we're defining a new modifier called `@internal`.

When applied to a member, it allows public access when reading the
member while restricting write access to private contexts.

For example:

``` python
struct Object:
    @internal x: str
```

The `@internal` modifier will apply the access rules defined in its
declaration.

### 1.2 Combining multiple access modifiers

Custom modifiers will also support lists of access modifiers.

``` python
type @internal:
    get = [@private, @frozen, ...]
    set = [@private, @frozen, ...]
```

A list of modifiers represents a combination of restrictions. **Every
modifier in the list must be satisfied.**

In other words, modifier composition uses logical AND semantics rather
than allowing any individual modifier to independently grant access.

This allows developers to combine existing modifier behavior when
defining more specialized access policies.

### 1.3 The `@param` placeholder

Custom modifier definitions will introduce a special placeholder called
`@param`.

`@param` is not an actual access modifier and cannot be used
independently. It exists exclusively within custom modifier definitions.

For example:

``` python
type @internal:
    get = @param
    set = @param
```

When `@internal` is applied, the developer can provide custom getter and
setter parameters:

``` python
struct Object:
    @internal[get = self._get_x, set = self._set_x] x: str

    @internal[get = self._get_y,
              set = self._set_y] y: str
```

The supplied parameters replace the corresponding `@param` placeholders
in the modifier definition.

The `@param` placeholder will also support fallback behavior through the
`or` operator.

For example:

``` python
type @internal:
    get = @param or @public
    set = @param or @private
```

This means that if no custom parameter is provided, the specified
default modifier is used.

A list can also be used as a fallback:

``` python
type @internal:
    get = @param or [@private, @frozen]
    set = @param or @private
```

This provides flexibility without requiring developers to define
separate modifiers for every possible configuration.

### 1.4 Reusable modifier definitions with `using`

Sere 0.3.0 will also introduce a new use for the `using` keyword.

Developers will be able to create reusable modifier configurations and
assign them aliases.

For example:

``` python
struct Object:
    using @my_internal as @internal[
        get = self._get_$
        set = self._set_$
    ]

    @my_internal x: str
```

Here, `@my_internal` is an alias for a configured version of
`@internal`.

An additional feature of this system is the introduction of the `$`
substitution character.

The `$` character automatically matches the name of the member to which
the modifier is applied.

In the previous example:

``` python
@my_internal x: str
```

The getter and setter references expand to:

``` python
self._get_x
self._set_x
```

The substitution character can also appear anywhere within a function or
method reference.

For example:

``` python
self.set$var
```

When applied to a member named `x`, this expands to:

``` python
self.setxvar
```

This feature is intended to make reusable modifier configurations
considerably more convenient, particularly when implementing properties
and custom access policies across multiple members.

------------------------------------------------------------------------

# 2. Late Initialization

Another addition to Sere 0.3.0 will be the `late` keyword.

The purpose of `late` is to allow members to be declared without
requiring them to be initialized during object construction.

This is particularly useful when a member cannot or should not be
initialized immediately.

### 2.1 Syntax and usage

Late initialization will be declared as part of a member's type
expression:

``` python
struct Object:
    my_var: late str
```

Unlike an optional or nullable member, a late member retains its
original declared type.

For example:

``` python
my_var: late str
```

does not mean:

``` python
my_var: str | None
```

Instead, it means that `my_var` is a non-nullable string member whose
initialization is deferred.

The `late` keyword is only valid within class or structure definitions.
It is not intended to be used for ordinary variable declarations.

A normal member declaration without an assigned value remains distinct
from a late member declaration.

### 2.2 Initialization behavior

Late members are not automatically initialized during object
construction.

Instead, they must be initialized manually:

``` python
struct Object:
    my_var: late str

    def initialize(self):
        self.my_var = "Hello, Sere!"
```

Once assigned, the member behaves like an ordinary member.

### 2.3 Accessing uninitialized members

Accessing a late member before it has been initialized will produce an
error.

For example:

``` python
struct Object:
    my_var: late str

    def print(self):
        print(self.my_var)
```

If `print()` is called before `my_var` is initialized, an error occurs.

Importantly, accessing a late member always assumes that it has been
initialized. An uninitialized late member cannot be treated as a
nullable value or accessed as though it contained `None`.

This distinguishes late initialization from nullable members:

-   A nullable member can be accessed even when its value is `None`.
-   A late member cannot be accessed at all until it has been
    initialized.

### 2.4 Behavior after initialization

The `late` keyword is an initialization-state marker, not an access
modifier or mutability restriction.

Once a late member has been initialized, it behaves exactly like an
ordinary member.

This means:

-   Reassignment is permitted.
-   The member retains its original declared type.
-   No additional restrictions are imposed by `late`.
-   Any applicable access modifiers continue to determine who can read
    or modify the member.

For example:

``` python
struct Object:
    my_var: late str

    def initialize(self):
        self.my_var = "First value"

    def update(self):
        self.my_var = "Second value"
```

Both assignments are valid.

The `late` marker does not imply immutability or single-assignment
semantics.

------------------------------------------------------------------------

# 3. Automatic Member Assignment (`auto`)

Sere 0.3.0 will introduce the `auto` keyword for constructor parameters.

When applied to a parameter in an `__init__` method, `auto` instructs
the compiler to automatically assign the parameter's value to a
corresponding class member.

This eliminates repetitive constructor assignments.

### 3.1 Syntax and usage

Consider the following class:

``` python
class Object:
    x: str

    def __init__(self, auto x: str) -> None:
        ...
```

Normally, this would require an explicit assignment:

``` python
class Object:
    x: str

    def __init__(self, x: str) -> None:
        self.x = x
```

With `auto`, the compiler generates the corresponding assignment
automatically.

Conceptually, the original example behaves as though the following
assignment were written inside the constructor:

``` python
self.x = x
```

### 3.2 Restrictions

The `auto` keyword has several restrictions:

-   It can only be used in parameter lists.
-   It can only be used within `__init__` methods.
-   The corresponding class member must exist.
-   The parameter and member must have identical names.
-   The parameter and member must have matching types.

The compiler will report an error if these requirements are not
satisfied.

### 3.3 Access modifier compatibility

Automatically generated assignments must respect the access modifiers
applied to the corresponding member.

However, because the assignment occurs within the class's own
initialization context, private members can still be initialized using
`auto`.

For example:

``` python
class Object:
    @private x: str

    def __init__(self, auto x: str) -> None:
        ...
```

This is valid.

The compiler-generated assignment follows the same access-control rules
as an explicitly written assignment within the constructor.

### 3.4 Compatibility with late initialization

The `auto` keyword can also be used with late members.

``` python
class Object:
    handle: late gl.Window

    def __init__(self, auto handle: gl.Window) -> None:
        ...
```

Although `handle` is declared as late, the automatic constructor
assignment initializes it normally.

The `late` marker does not prohibit initialization during construction.
It simply indicates that initialization is not automatically required as
part of the member's ordinary construction process.

------------------------------------------------------------------------

# 4. Named Tuples

Another major addition to Sere 0.3.0 will be support for named tuples.

Sere already supports ordinary tuple type declarations.

For example:

``` python
type myTuple = (u32, u32)
```

Alternatively:

``` python
type myTuple = tuple[u32, u32]
```

Ordinary tuples expose their elements through positional names such as
`.first` and `.second`, as well as their existing positional access
mechanisms.

With named tuples, developers will be able to assign meaningful names to
individual tuple elements.

### 4.1 Syntax and usage

A named tuple can be declared by providing names alongside its element
types:

``` python
type named_tuple = (a: u32, b: u32)
```

The elements can then be accessed using their assigned names:

``` python
named_tuple.a
named_tuple.b
```

This makes tuple declarations considerably more descriptive without
requiring a full class or structure definition.

### 4.2 Named tuple type declarations

Named tuples will also support direct type declarations using the `type`
keyword.

For example, consider a `Size` type.

Normally, this could be represented using a structure:

``` python
struct Size:
    width: u32
    height: u32
```

However, if the type only needs to represent a pair of values, a named
tuple may be more appropriate.

With named tuples, the same type can be declared as:

``` python
type Size: (width: u32, height: u32)
```

This provides a lightweight alternative to structures for simple data
representations.

### 4.3 Immutability

Named tuples are immutable.

Once constructed, their individual elements cannot be modified.

This makes them suitable for representing fixed collections of related
values, such as coordinates, dimensions, and other lightweight data
types.

### 4.4 Structural compatibility

Unlike nominally typed structures, named tuples use structural
compatibility.

Two named tuple types with matching structures are considered
compatible, regardless of whether they were declared under different
type names.

For example:

``` python
type Size: (width: u32, height: u32)

type Resolution: (width: u32, height: u32)
```

Although `Size` and `Resolution` are separately declared types, they
share the same structure and are therefore structurally compatible.

Compatibility is determined by the tuple's element names, order, and
corresponding types.

This follows the general principle of structural typing: if two types
have the required structure, they are compatible.

### 4.5 Positional access

Named tuples retain positional access just like ordinary tuples.

For example:

``` python
type Size: (width: u32, height: u32)
```

The following access patterns are supported:

``` python
size.width
size.height

size.first
size.second

size[0]
size[1]
```

Named access does not remove or replace the existing positional tuple
interface.

### 4.6 Parameter unpacking

Named tuples also retain parameter unpacking capabilities.

Their elements can be unpacked into individual variables, just like
ordinary tuples.

For example:

``` python
width, height = size
```

This allows named tuples to remain convenient in situations where
positional tuple operations are preferable to accessing individual named
elements.

### 4.7 Restrictions

Named tuples are not treated as ordinary structure member definitions.

Consequently, they do not support:

-   `late` initialization.
-   `auto` member assignment.
-   Access modifiers such as `@private` and `@public`.

Named tuples are intended to remain lightweight, immutable, structurally
typed collections rather than full class or structure definitions.

------------------------------------------------------------------------

# 5. Feature Summary

The following table summarizes the primary additions planned for Sere
0.3.0.

  -----------------------------------------------------------------------
  Feature                             Description
  ----------------------------------- -----------------------------------
  Custom access modifiers             Allows developers to define
                                      reusable access policies.

  Modifier composition                Allows multiple access modifiers to
                                      be combined, with every restriction
                                      enforced.

  `@param`                            Provides configurable placeholders
                                      within custom modifier definitions.

  `using`                             Introduces reusable modifier
                                      configurations and aliases.

  `$` substitution                    Automatically substitutes member
                                      names into configured accessor
                                      references.

  `late`                              Allows non-nullable members to be
                                      initialized after construction.

  `auto`                              Automatically assigns constructor
                                      parameters to corresponding class
                                      members.

  Named tuples                        Introduces named, immutable,
                                      structurally compatible tuples.

  Positional tuple access             Preserves positional access for
                                      named tuples.

  Tuple unpacking                     Preserves unpacking capabilities
                                      for named tuples.
  -----------------------------------------------------------------------

------------------------------------------------------------------------

# 6. Development Direction

Sere 0.3.0 represents a substantial expansion of the language.

Rather than simply introducing additional syntax, these features are
intended to improve how developers define, initialize, access, and
organize data.

Particular attention will be given to compiler-side validation and
semantic consistency.

The implementation will need to account for:

-   Custom modifier parsing and resolution.
-   Modifier composition and conflict detection.
-   Accessor signature validation.
-   Parameter substitution and reusable modifier configurations.
-   Late member initialization tracking.
-   Automatically generated constructor assignments.
-   Named tuple type construction and structural compatibility.
-   Tuple indexing, positional access, and unpacking.

The implementation order and internal compiler architecture may evolve
during development.

**Sere 0.2.2 will remain the previous stable release while development
progresses toward 0.3.0.**

------------------------------------------------------------------------

# 7. Collaboration and Contributions

I'm also open to collaboration requests.

As development continues, I'm still interested in bringing additional
people onto the project, particularly developers interested in
programming language design, compiler development, type systems, and
language tooling.

If you have ideas, feature requests, or suggestions for Sere, please
reach out to me directly.

I'm completely open to new ideas and collaboration opportunities, and
I'd like to hear what others would like to see in the language.

**If you're interested in contributing to Sere or becoming part of the
development team, feel free to DM me.**

------------------------------------------------------------------------

*Thank you to everyone following Sere's development. I'm looking forward
to getting back into active development and seeing where 0.3.0 takes the
language.*

**--- Sere Development**
