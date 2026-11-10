# biyos

An optimization solver for problems of the form

```text
minimize    f(x)
subject to  l <= g(x) <= u
            h(x) in K
            x[J] integer
```

where the bounds `l` and `u` may be infinite, `K` is a product of cones such as the nonnegative orthant and the second-order, exponential, power and semidefinite cones, and `J` is the set of integer variables.

The core is C++20 behind a C API, and every language interface is built on that C API.

The project is at its start and solves nothing yet.

## Build

```sh
cmake -S . -B build -DBIYOS_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Layout

| Path | Contents |
| --- | --- |
| `include/biyos/` | the public C header, the only binary interface |
| `src/<module>/` | one directory per module, private headers beside their sources |
| `src/capi/` | the C API implementation |
| `tests/<module>/` | tests for each module |
| `cmake/` | build helpers |
