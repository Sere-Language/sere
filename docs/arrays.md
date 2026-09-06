# Numeric arrays

`import arrays` provides numeric operations on packed `list[f64]` values.
These functions use ordinary Sere lists. The language's `array[T]` collection
is a separate builtin.

## Create and transform values

```sere
import arrays

def main() -> i32:
    values: list[f64] = [1.0, 2.0, 3.0]
    scaled: list[f64] = arrays.scale(values, 2.0)
    assert values[0] == 1.0
    assert scaled[0] == 2.0
    assert arrays.sum(scaled) == 12.0
    assert arrays.mean(values) == 2.0
    return 0
```

Transformations return new lists. Use `arrays.copy(values)` when you explicitly
need an independent copy. Keep inputs typed as `list[f64]`; an existing
`list[i32]` does not become `list[f64]` through assignment.

| Function | Result |
| --- | --- |
| `zeros(length)`, `ones(length)` | List filled with zero or one |
| `full(length, fill)` | List filled with the given `f64` |
| `linspace(start, stop, count)` | Evenly spaced values, including both endpoints when `count > 1` |
| `copy(values)`, `reverse(values)` | Copied values in original or reversed order |
| `concat(left, right)` | Both lists joined in order |
| `add`, `sub`, `mul`, `div` | Elementwise operation on two lists |
| `scale(values, factor)` | Each value multiplied by a scalar |
| `abs(values)` | Absolute values |
| `clip(values, low, high)` | Values clamped to the bounds |
| `normalize(values)` | Values divided by their Euclidean length; near-zero vectors are copied |

Lengths and counts use `i64`; scalar values use `f64`. All these functions return
`list[f64]`. `linspace` returns an empty list for a nonpositive count and just
`start` for a count of one.

Elementwise binary functions operate up to the shorter input length. They do
not broadcast or reject unequal lengths. If your algorithm requires equal
lengths, check `assert len(left) == len(right)` before calling them.

## Statistics and vector products

| Function | Result |
| --- | --- |
| `sum(values)`, `mean(values)` | Total or arithmetic mean (`f64`) |
| `var(values)`, `std(values)` | Population variance or its square root (`f64`) |
| `vmin(values)`, `vmax(values)` | Smallest or largest value (`f64`) |
| `argmin(values)`, `argmax(values)` | Index of the first smallest or largest value (`i64`) |
| `dot(left, right)` | Sum of pairwise products up to the shorter length (`f64`) |
| `outer(left, right)` | Flattened outer product (`list[f64]`) |
| `cross(left, right)` | Three-component cross product (`list[f64]`) |

Variance divides by the number of values, not by `n - 1`. Empty-list statistics
return zero, including `argmin` and `argmax`; that index does not identify an
element in an empty list. Check the length before indexing with it. Use
three-element inputs for `cross`.

## Matrices use flat row-major storage

For `rows` by `cols`, element `(row, col)` is stored at `row * cols + col`.
`zeros2(rows, cols)` and `ones2(rows, cols)` allocate flat lists with
`rows * cols` elements. `eye(n)` creates a flat `n` by `n` identity matrix.
Dimensions are supplied separately; lists do not carry a matrix shape.

```sere
import arrays

def main() -> i32:
    left: list[f64] = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]
    right: list[f64] = [1.0, 0.0, 0.0, 1.0, 1.0, 1.0]
    product = arrays.matmul(left, 2, 3, right, 3, 2)
    assert len(product) == 4
    assert product[0] == 4.0
    assert product[1] == 5.0
    assert product[2] == 10.0
    assert product[3] == 11.0
    transposed = arrays.transpose(left, 2, 3)
    assert transposed[1] == 4.0
    return 0
```

`matmul(left, left_rows, left_cols, right, right_rows, right_cols)` produces
`left_rows * right_cols` values. Supply positive dimensions, matching inner
dimensions, and lists whose lengths match their declared shapes. The runtime
returns an empty list for mismatched inner dimensions; it does not provide
comprehensive shape validation. `transpose(values, rows, cols)` returns a flat
matrix with `cols` rows and `rows` columns.

For fixed-size matrix objects and graphics transforms, see
[`matrix.sere`](../stdlib/matrix.sere). The
[numeric example](../examples/numeric.sere) combines arrays, matrices, vectors,
and byte buffers. API declarations live in [`arrays.sere`](../stdlib/arrays.sere),
with native implementations in [`sere_stdlib.c`](../runtime/sere_stdlib.c).
