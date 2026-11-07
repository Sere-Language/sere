# Custom access modifiers

Define independent rules for reading and writing a member:

```sere
type @access:
    get = @param or @public
    set = @param or @private

class Counter:
    @access[get = self.read, set = self.write] value: i32 = 0

    def read(self) -> i32:
        return self.value

    def write(self, value: i32) -> void:
        self.value = value
```

`@param` is valid only inside modifier definitions. A bare `@param` requires
the corresponding accessor argument. `@param or @public` (or a list such as
`@param or [@private, @frozen]`) uses its fallback when that argument is omitted.
Every restriction in a list applies.

Accessors are synchronous instance methods: getters take `self` and return the
field's type; setters take `self` and a value of that type and return `void` or
`None`. Within an accessor, accessing its associated field uses backing storage.
Constructors also initialize backing storage directly. Outside those contexts,
reads and writes invoke the configured methods.

Functions without a return annotation return `void`; a setter may therefore be
written as `def write(self, value: i32):`. Custom modifiers can also be placed
on functions or methods. Their `get` and `set` slots do not apply in that case:
no `@param` argument is required, and any supplied accessor references are ignored.

Aliases configure accessors once for multiple members:

```sere
class Counter:
    using @tracked as @access[
        get = self.read_$
        set = self.write_$
    ]
    @tracked value: i32

    def read_value(self) -> i32:
        return self.value

    def write_value(self, value: i32) -> void:
        self.value = value
```

Each `$` expands to the field name; it may appear in the middle of a name.
Alias arguments may be separated by commas or newlines. Explicit arguments on
an alias application override its configured arguments.

The language server provides modifier and alias completion, definition-body
and application-key completion, hover, navigation from applications, and
semantic highlighting. VS Code snippets are available under `modifier`,
`accessor`, and `usingmodifier`.
