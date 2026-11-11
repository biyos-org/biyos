# biyos

An optimization solver for problems of the form

```text
minimize    f(x)
subject to  l <= g(x) <= u
            h(x) in K
            x[J] integer
```

where the bounds `l` and `u` may be infinite, `K` is a product of cones such as the nonnegative orthant and the second-order, exponential, power and semidefinite cones, and `J` is the set of integer variables.

The project is at its start and solves nothing yet.

## Licence

Copyright 2026 BIYOS Authors. Licensed under the [Apache License 2.0](LICENSE).
