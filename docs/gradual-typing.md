# Gradual typing and inference

`Any` accepts values of every type, including records, strings, collections,
functions, enums, and `None`. It preserves the complete value and its concrete
type when passed to a function, returned, or stored in a collection.

```python
def identity(value: Any) -> Any:
    return value

text: str = identity("hello")
number: i32 = identity(42)
```

Converting an `Any` to a concrete type checks the stored type at runtime. A
mismatch terminates with `Any type mismatch: expected ...`, rather than reading
the value using the wrong memory layout. The check requires the concrete type
to match exactly: unbox to the original numeric type before converting its
width, and to the original record type before converting to a base class.
`isinstance(value, T)` checks that concrete type for `Any` values, and
`typeof(value)` returns its name. A default-initialized `Any` represents `None`.

This is gradual typing at typed boundaries. Arbitrary member access, calls,
or arithmetic directly on `Any` still require converting to an appropriate
concrete type first.

Lists with mixed element types infer `list[Any]`; dictionaries with mixed
value types infer `dict[K, Any]`. Numeric collections infer a common numeric
type before considering assignment compatibility, so a narrow integer first
does not force wider subsequent integers to truncate.

Collection annotations provide context for literals in variable declarations,
assignments, function arguments, and return statements, including nested
literals. For example, a parameter declared `list[Any]` accepts both `[]` and
`[1, "two"]`. Empty literals without type context still need an annotation.

Existing mutable collections require matching element types. Assigning an
existing `list[i32]` to `list[Any]`, or changing a dictionary's key/value types
through assignment, is rejected: those aliases would allow incompatible writes
and have incompatible element layouts. Construct a new collection with the
desired annotation instead. Concrete type mismatches and literal range checks
remain compile-time errors.

Callable annotations must also preserve whether each parameter and the return
value use `Any`. Use a wrapper function to box or unbox at those boundaries;
changing a callback annotation alone cannot change its calling convention.

The representation of `Any` has changed. Rebuild compiled Sere libraries and
programs together; old object files containing `Any` are not ABI compatible.

See [the executable example](../examples/any.sere).
