#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/model/tolerances.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "model/model.hpp"
#include "model/utils/gather.hpp"

#include <span>
#include <vector>

using biyos::Constraint;
using biyos::Family;
using biyos::Index;
using biyos::kInfinity;
using biyos::Model;
using biyos::picked;
using biyos::Session;
using biyos::capi::delete_tolerances;
using biyos::capi::get_tolerances;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::listed;
using biyos::capi::Range;
using biyos::capi::read_field;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::set_tolerances;
using biyos::capi::writable;
using biyos::capi::write_field;

namespace
{
std::vector<Constraint> free_constraints(std::span<const Index> functions)
{
    std::vector<Constraint> found;
    found.reserve(functions.size());
    for (const Index function : functions)
    {
        found.push_back(Constraint{.function = function, .lower = -kInfinity, .upper = kInfinity});
    }
    return found;
}

void add_constraints(Session &session,
                     BIYOS_index n,
                     const BIYOS_index *functions,
                     const BIYOS_real *lower,
                     const BIYOS_real *upper,
                     BIYOS_index *start)
{
    Model &model = writable(session);
    std::vector<Constraint> added = free_constraints(listed(n, functions, "fun"));
    read_field(added, &Constraint::lower, lower);
    read_field(added, &Constraint::upper, upper);
    const Index index = model.n_constraints();
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
    const auto changed = selected(Family::Constraint, model.n_of(Family::Constraint), selection);
    std::vector<Constraint> constraints = picked(model, &Model::constraint, changed);
    read_field(constraints, &Constraint::lower, lower);
    read_field(constraints, &Constraint::upper, upper);
    model.set_constraints(changed, constraints);
}

void n_constraints(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_constraints();
}

template <typename Selection>
void get_constraints(const Session &session,
                     Selection selection,
                     BIYOS_index *functions,
                     BIYOS_real *lower,
                     BIYOS_real *upper)
{
    const auto chosen =
        selected(Family::Constraint, session.model().n_of(Family::Constraint), selection);
    const std::vector<Constraint> constraints = picked(session.model(), &Model::constraint, chosen);
    write_field(constraints, &Constraint::function, functions);
    write_field(constraints, &Constraint::lower, lower);
    write_field(constraints, &Constraint::upper, upper);
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
