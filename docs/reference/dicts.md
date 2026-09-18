# Dicts

`dict[K, V]` is a hash map. Keys may be `str` or an integer type; values may be
any type.

## Literals and types

```sere
ages = {"ann": 31, "bo": 24}      # dict[str, i32]
scores: dict[str, f64] = {}
by_id = {1: "one", 2: "two"}      # dict[i32, str]
```

An empty literal needs an annotation so `K` and `V` can be inferred.

## Lookup and assignment

```sere
ages = {"ann": 31}
a = ages["ann"]        # 31
ages["cy"] = 40        # insert or overwrite
```

Reading a missing key is a **runtime error**, not `None`. Use `get` when a key
may be absent.

## Operators

| Expression | Result | Notes |
| --- | --- | --- |
| `d[k]` | `V` | runtime error when absent |
| `k in d`, `k not in d` | `bool` | key presence |
| `d == e`, `d != e` | `bool` | **identity** — true only for the same dict object |

## Methods

| Method | Returns | Description |
| --- | --- | --- |
| `get(key)` | `V` | value, or the zero value of `V` when absent |
| `set(key, value)` | `void` | same as `d[key] = value` |
| `pop(key)` | `V` | remove and return |
| `remove(key)` / `delete(key)` | `bool` | remove a key |
| `contains(key)` / `has(key)` | `bool` | key presence |
| `keys()` | `list[K]` | every key |
| `values()` | `list[V]` | every value |
| `clear()` | `void` | remove all entries |
| `copy()` / `clone()` | `dict[K, V]` | shallow copy |

```sere
d = {"a": 1}
print(d.get("a"))       # 1
print("zz" in d)        # False
d.set("b", 2)
print("b" in d)         # True
print(d.keys())         # ['a', 'b']
```

`get` does not return an optional: it cannot tell you whether the key was
present. Test with `in` / `contains` before reading when that matters.

`==` compares identity rather than contents:

```sere
print({"k": 1} == {"k": 1})   # False
d = {"k": 1}
print(d == d)                  # True
```

## Iteration

```sere
for key in d:
    print(key)

for key in d.keys():
    print(key + " = " + d[key])
```

Iterating a dict yields its **keys**. Call `values()` when you need the values.

## Element types

`K` is restricted to `str`, `i32`, `u32`, `i64`, or `u64`: those are the key
encodings the runtime supports. Other key types are rejected at compile time.
`V` is unconstrained.
