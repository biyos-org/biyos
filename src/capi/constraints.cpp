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

void add_constraints(biyos_model &model,
                     int count,
                     const int *functions,
                     const double *lower,
                     const double *upper,
                     int *first)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(count);
    if (size > 0)
    {
        require(functions, "functions");
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
    const biyos::Index start = core.constraint_count();
    core.add_constraints(added);
    if (first != nullptr)
    {
        *first = start;
    }
}

void set_bounds(biyos_model &model,
                int count,
                const int *indices,
                const double *lower,
                const double *upper)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> changed = listed(count, indices);
    std::vector<biyos::Constraint> constraints;
    constraints.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        constraints.push_back(bounded(core.constraint(changed[k]), k, lower, upper));
    }
    core.set_constraints(changed, constraints);
}

void constraint_count(biyos_model &model, int *count)
{
    *require(count, "count") = model.core.constraint_count();
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

void get_constraints(biyos_model &model,
                     int first,
                     int count,
                     int *functions,
                     double *lower,
                     double *upper)
{
    const std::span<const biyos::Constraint> found = model.core.constraints(first, count);
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        copy_out(found[k], k, functions, lower, upper);
    }
}

}

biyos_status biyos_add_constraints(biyos_model *model,
                                   int count,
                                   const int *functions,
                                   const double *lower,
                                   const double *upper,
                                   int *first)
{
    return guarded<add_constraints>(model, count, functions, lower, upper, first);
}

biyos_status biyos_set_constraint_bounds(biyos_model *model,
                                         int count,
                                         const int *indices,
                                         const double *lower,
                                         const double *upper)
{
    return guarded<set_bounds>(model, count, indices, lower, upper);
}

biyos_status biyos_constraint_count(biyos_model *model, int *count)
{
    return guarded<constraint_count>(model, count);
}

biyos_status biyos_get_constraints(biyos_model *model,
                                   int first,
                                   int count,
                                   int *functions,
                                   double *lower,
                                   double *upper)
{
    return guarded<get_constraints>(model, first, count, functions, lower, upper);
}
