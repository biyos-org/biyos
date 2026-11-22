#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::picked;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();

biyos::Constraint bounded(biyos::Constraint constraint,
                          std::size_t k,
                          const double *lower,
                          const double *upper)
{
    if (lower != nullptr)
    {
        constraint.lower = lower[k];
    }
    if (upper != nullptr)
    {
        constraint.upper = upper[k];
    }
    return constraint;
}

void add_constraints(BIYOS_model &model,
                     int n,
                     const int *functions,
                     const double *lower,
                     const double *upper,
                     int *start)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(functions, "funidx");
    }
    std::vector<biyos::Constraint> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        const biyos::Constraint free{
            .function = functions[k],
            .lower = -kInfinity,
            .upper = kInfinity,
        };
        added.push_back(bounded(free, k, lower, upper));
    }
    const biyos::Index index = core.n_constraints();
    core.add_constraints(added);
    if (start != nullptr)
    {
        *start = index;
    }
}

void set_bounds(BIYOS_model &model,
                int n,
                const int *indices,
                const double *lower,
                const double *upper)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> changed = listed(n, indices);
    std::vector<biyos::Constraint> constraints;
    constraints.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        constraints.push_back(bounded(core.constraint(changed[k]), k, lower, upper));
    }
    core.set_constraints(changed, constraints);
}

void n_constraints(const BIYOS_model &model, int *n)
{
    *require(n, "n") = model.core.n_constraints();
}

void copy_out(const biyos::Constraint &constraint,
              std::size_t k,
              int *functions,
              double *lower,
              double *upper)
{
    if (functions != nullptr)
    {
        functions[k] = constraint.function;
    }
    if (lower != nullptr)
    {
        lower[k] = constraint.lower;
    }
    if (upper != nullptr)
    {
        upper[k] = constraint.upper;
    }
}

void get_constraints(const BIYOS_model &model,
                     int start,
                     int size,
                     int *functions,
                     double *lower,
                     double *upper)
{
    const std::span<const biyos::Constraint> found = model.core.constraints(start, size);
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        copy_out(found[k], k, functions, lower, upper);
    }
}

void get_bounds(const BIYOS_model &model, int n, const int *indices, double *lower, double *upper)
{
    const std::vector<biyos::Constraint> constraints =
        picked(model.core, &biyos::Model::constraint, listed(n, indices));
    for (std::size_t k = 0; k < constraints.size(); ++k)
    {
        copy_out(constraints[k], k, nullptr, lower, upper);
    }
}

}

BIYOS_status BIYOS_addcon(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_index *funidx,
                          const BIYOS_real *lb,
                          const BIYOS_real *ub,
                          BIYOS_index *pos)
{
    return guarded<add_constraints>(mdl, n, funidx, lb, ub, pos);
}

BIYOS_status BIYOS_setconbndsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_bounds>(mdl, n, idx, lb, ub);
}

BIYOS_status BIYOS_getconbndsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_bounds>(mdl, n, idx, lb, ub);
}

BIYOS_status BIYOS_getncon(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_constraints>(mdl, n);
}

BIYOS_status BIYOS_getconrg(const BIYOS_model *mdl,
                            BIYOS_index pos,
                            BIYOS_index n,
                            BIYOS_index *funidx,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    return guarded<get_constraints>(mdl, pos, n, funidx, lb, ub);
}
