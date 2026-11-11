<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/logo/lockup-reversed.svg">
    <img src="docs/logo/lockup.svg" alt="biyos" width="280">
  </picture>
</h1>

An optimization solver for problems of the form

```text
minimize    f_1(x), ..., f_p(x) in priority order
subject to  l <= g(x) <= u
            h(x) in K
            x[J] integer
```

where the bounds `l` and `u` may be infinite, `K` is a product of cones such as the nonnegative orthant and the second-order, exponential, power and semidefinite cones, and `J` is the set of integer variables.

The project is at its start and solves nothing yet.

## Licence

Copyright 2026 BIYOS Authors. Licensed under the [Apache License 2.0](LICENSE).
