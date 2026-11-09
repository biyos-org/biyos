# biyos

An optimization solver for problems of the form

```text
minimize    f(x)
subject to  l <= g(x) <= u
            x_I in K
            x_J integer
```

where `K` is a product of cones. The core is C++20 behind a C API, and every language interface is built on that C API.

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
