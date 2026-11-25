#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/model/tolerances.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <vector>

using biyos::Family;
using biyos::Model;
using biyos::Session;
using biyos::capi::delete_tolerances;
using biyos::capi::get_tolerances;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::picked;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::set_tolerances;
using biyos::capi::writable;

namespace
{
biyos::Constraint bounded(biyos::Constraint constraint,
                          std::size_t k,
                          const BIYOS_real *lower,
                          const BIYOS_real *upper)
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

void add_constraints(Session &session,
                     BIYOS_index n,
                     const BIYOS_index *functions,
                     const BIYOS_real *lower,
                     const BIYOS_real *upper,
                     BIYOS_index *start)
{
    Model &model = writable(session);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(functions, "fun");
    }
    std::vector<biyos::Constraint> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        const biyos::Constraint free{
            .function = functions[k],
            .lower = -biyos::kInfinity,
            .upper = biyos::kInfinity,
        };
        added.push_back(bounded(free, k, lower, upper));
    }
    const biyos::Index index = model.n_constraints();
    model.add_constraints(added);
    if (start != nullptr)
    {
        *start = index;
    }
}

template <typename Selection>
void set_bounds(Session &session,
                Selection selection,
                const BIYOS_real *lower,
                const BIYOS_real *upper)
{
    Model &model = writable(session);
    const auto changed = selected(model, Family::Constraint, selection);
    std::vector<biyos::Constraint> constraints;
    constraints.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        constraints.push_back(bounded(model.constraint(changed[k]), k, lower, upper));
    }
    model.set_constraints(changed, constraints);
}

void n_constraints(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_constraints();
}

void copy_out(const biyos::Constraint &constraint,
              std::size_t k,
              BIYOS_index *functions,
              BIYOS_real *lower,
              BIYOS_real *upper)
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

template <typename Selection>
void get_constraints(const Session &session,
                     Selection selection,
                     BIYOS_index *functions,
                     BIYOS_real *lower,
                     BIYOS_real *upper)
{
    const auto chosen = selected(session.model(), Family::Constraint, selection);
    const std::vector<biyos::Constraint> constraints =
        picked(session.model(), &Model::constraint, chosen);
    for (std::size_t k = 0; k < constraints.size(); ++k)
    {
        copy_out(constraints[k], k, functions, lower, upper);
    }
}

template <typename Selection>
void get_bounds(const Session &session, Selection selection, BIYOS_real *lower, BIYOS_real *upper)
{
    get_constraints(session, selection, nullptr, lower, upper);
}

}

BIYOS_status BIYOS_addcon(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_index *fun,
                          const BIYOS_real *lb,
                          const BIYOS_real *ub,
                          BIYOS_index *pos)
{
    return guarded<add_constraints>(mdl, n, fun, lb, ub, pos);
}

BIYOS_status BIYOS_setconbndsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_bounds<List>>(mdl, List{n, idx}, lb, ub);
}

BIYOS_status BIYOS_setconbndrg(BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_bounds<Range>>(mdl, Range{beg, end}, lb, ub);
}

BIYOS_status BIYOS_getconbndsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_bounds<List>>(mdl, List{n, idx}, lb, ub);
}

BIYOS_status BIYOS_getconbndrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_bounds<Range>>(mdl, Range{beg, end}, lb, ub);
}

BIYOS_status BIYOS_setcontolsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *atol,
                               const BIYOS_real *rtol)
{
    return guarded<set_tolerances<List>>(mdl, Family::Constraint, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_setcontolrg(BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               const BIYOS_real *atol,
                               const BIYOS_real *rtol)
{
    return guarded<set_tolerances<Range>>(mdl, Family::Constraint, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_getcontolsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *atol,
                               BIYOS_real *rtol)
{
    return guarded<get_tolerances<List>>(mdl, Family::Constraint, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_getcontolrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_real *atol,
                               BIYOS_real *rtol)
{
    return guarded<get_tolerances<Range>>(mdl, Family::Constraint, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_delcontolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_tolerances<List>>(mdl, Family::Constraint, List{n, idx});
}

BIYOS_status BIYOS_delcontolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_tolerances<Range>>(mdl, Family::Constraint, Range{beg, end});
}

BIYOS_status BIYOS_getncon(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_constraints>(mdl, n);
}

BIYOS_status BIYOS_getconsp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *fun,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    return guarded<get_constraints<List>>(mdl, List{n, idx}, fun, lb, ub);
}

BIYOS_status BIYOS_getconrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            BIYOS_index *fun,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    return guarded<get_constraints<Range>>(mdl, Range{beg, end}, fun, lb, ub);
}
