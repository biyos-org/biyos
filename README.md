# biyos

An optimization solver for problems of the form

$$
\Large
\begin{aligned}
\min_{x \in \mathbb{R}^n} \quad & f(x) \\
\text{s.t.} \quad & l \le g(x) \le u \\
& x_I \in \mathcal{K} \\
& x_J \in \mathbb{Z}^{|J|}
\end{aligned}
$$

where $`g : \mathbb{R}^n \to \mathbb{R}^m`$, the bounds $`l`$ and $`u`$ may be infinite, $`x_I`$ is the subvector of $`x`$ indexed by $`I`$, $`J`$ indexes the integer variables, and $`\mathcal{K} = \mathcal{K}_1 \times \cdots \times \mathcal{K}_p`$ is a product of cones such as the nonnegative orthant and the second-order, exponential, power and semidefinite cones.

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
