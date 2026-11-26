#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/model/tolerances.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/busy.hpp"
#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/timing/timer.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <vector>

using biyos::EntryTimer;
using biyos::Env;
using biyos::Family;
using biyos::Index;
using biyos::kModule;
using biyos::Model;
using biyos::Session;
using biyos::Variable;
using biyos::VariableKind;
using biyos::widest;
using biyos::capi::delete_tolerances;
using biyos::capi::EnvAccess;
using biyos::capi::get_tolerances;
using biyos::capi::guarded;
using biyos::capi::idle;
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

void create(Env &env, BIYOS_model **mdl)
{
    BIYOS_model **made = require(mdl, "mdl");
    idle(env, "environment");
    *made = new BIYOS_model(env);
}

void make_copy(const Session &session, BIYOS_env *env, BIYOS_model **copy)
{
    BIYOS_model **made = require(copy, "copy");
    Env &target = idle(require(env, "env")->core, "argument 'env'");
    const EnvAccess access(target);
    access.check();
    const EntryTimer timer(session.stream(), kModule, "copy");
    *made = new BIYOS_model(target, session);
}

void freeable(Session &session)
{
    writable(session);
}

VariableKind kind_at(const BIYOS_int *kinds, std::size_t k)
{
    return kinds == nullptr ? VariableKind::Continuous : static_cast<VariableKind>(kinds[k]);
}

Variable bounded(Variable variable, std::size_t k, const BIYOS_real *lower, const BIYOS_real *upper)
{
    if (lower != nullptr)
    {
        variable.lower = lower[k];
    }
    if (upper != nullptr)
    {
        variable.upper = upper[k];
    }
    return variable;
}

Variable kinded(Variable variable, std::size_t k, const BIYOS_int *kinds)
{
    if (kinds != nullptr)
    {
        variable.kind = static_cast<VariableKind>(kinds[k]);
    }
    return variable;
}

template <auto Change, typename... Arrays>
void set_each(Model &model, std::span<const Index> indices, Arrays... arrays)
{
    std::vector<Variable> changed;
    changed.reserve(indices.size());
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        changed.push_back(Change(model.variable(indices[k]), k, arrays...));
    }
    model.set_variables(indices, changed);
}

void add_variables(Session &session,
                   BIYOS_index n,
                   const BIYOS_int *kinds,
                   const BIYOS_real *lower,
                   const BIYOS_real *upper,
                   BIYOS_index *start)
{
    Model &model = writable(session);
    const std::size_t size = length(n);
    std::vector<Variable> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        added.push_back(bounded(widest(kind_at(kinds, k)), k, lower, upper));
    }
    const Index index = model.n_variables();
    model.add_variables(added);
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
    const auto chosen = selected(model, Family::Variable, selection);
    set_each<bounded>(model, chosen, lower, upper);
}

template <typename Selection>
void set_kinds(Session &session, Selection selection, const BIYOS_int *kinds)
{
    Model &model = writable(session);
    const auto chosen = selected(model, Family::Variable, selection);
    set_each<kinded>(model, chosen, kinds);
}

void n_variables(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_variables();
}

void copy_out(const Variable &variable,
              std::size_t k,
              BIYOS_int *kinds,
              BIYOS_real *lower,
              BIYOS_real *upper)
{
    if (kinds != nullptr)
    {
        kinds[k] = static_cast<BIYOS_int>(variable.kind);
    }
    if (lower != nullptr)
    {
        lower[k] = variable.lower;
    }
    if (upper != nullptr)
    {
        upper[k] = variable.upper;
    }
}

template <typename Selection>
void get_variables(const Session &session,
                   Selection selection,
                   BIYOS_int *kinds,
                   BIYOS_real *lower,
                   BIYOS_real *upper)
{
    const auto chosen = selected(session.model(), Family::Variable, selection);
    const std::vector<Variable> variables = picked(session.model(), &Model::variable, chosen);
    for (std::size_t k = 0; k < variables.size(); ++k)
    {
        copy_out(variables[k], k, kinds, lower, upper);
    }
}

template <typename Selection>
void get_bounds(const Session &session, Selection selection, BIYOS_real *lower, BIYOS_real *upper)
{
    get_variables(session, selection, nullptr, lower, upper);
}

template <typename Selection>
void get_kinds(const Session &session, Selection selection, BIYOS_int *kinds)
{
    get_variables(session, selection, kinds, nullptr, nullptr);
}

}

BIYOS_status BIYOS_newmdl(BIYOS_env *env, BIYOS_model **mdl)
{
    if (mdl != nullptr)
    {
        *mdl = nullptr;
    }
    return guarded<create>(env, mdl);
}

BIYOS_status BIYOS_copymdl(const BIYOS_model *mdl, BIYOS_env *env, BIYOS_model **copy)
{
    if (copy != nullptr)
    {
        *copy = nullptr;
    }
    return guarded<make_copy>(mdl, env, copy);
}

BIYOS_status BIYOS_freemdl(BIYOS_model *mdl)
{
    if (mdl == nullptr)
    {
        return BIYOS_OK;
    }
    const BIYOS_status status = guarded<freeable>(mdl);
    if (status == BIYOS_OK)
    {
        delete mdl;
    }
    return status;
}

BIYOS_status BIYOS_addvar(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_int *knd,
                          const BIYOS_real *lb,
                          const BIYOS_real *ub,
                          BIYOS_index *pos)
{
    return guarded<add_variables>(mdl, n, knd, lb, ub, pos);
}

BIYOS_status BIYOS_setvarbndsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_bounds<List>>(mdl, List{n, idx}, lb, ub);
}

BIYOS_status BIYOS_setvarbndrg(BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_bounds<Range>>(mdl, Range{beg, end}, lb, ub);
}

BIYOS_status BIYOS_getvarbndsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_bounds<List>>(mdl, List{n, idx}, lb, ub);
}

BIYOS_status BIYOS_getvarbndrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_bounds<Range>>(mdl, Range{beg, end}, lb, ub);
}

BIYOS_status BIYOS_setvarkndsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_int *knd)
{
    return guarded<set_kinds<List>>(mdl, List{n, idx}, knd);
}

BIYOS_status BIYOS_setvarkndrg(BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               const BIYOS_int *knd)
{
    return guarded<set_kinds<Range>>(mdl, Range{beg, end}, knd);
}

BIYOS_status BIYOS_getvarkndsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_int *knd)
{
    return guarded<get_kinds<List>>(mdl, List{n, idx}, knd);
}

BIYOS_status BIYOS_getvarkndrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_int *knd)
{
    return guarded<get_kinds<Range>>(mdl, Range{beg, end}, knd);
}

BIYOS_status BIYOS_setvartolsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *atol,
                               const BIYOS_real *rtol)
{
    return guarded<set_tolerances<List>>(mdl, Family::Variable, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_setvartolrg(BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               const BIYOS_real *atol,
                               const BIYOS_real *rtol)
{
    return guarded<set_tolerances<Range>>(mdl, Family::Variable, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_getvartolsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *atol,
                               BIYOS_real *rtol)
{
    return guarded<get_tolerances<List>>(mdl, Family::Variable, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_getvartolrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_real *atol,
                               BIYOS_real *rtol)
{
    return guarded<get_tolerances<Range>>(mdl, Family::Variable, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_delvartolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_tolerances<List>>(mdl, Family::Variable, List{n, idx});
}

BIYOS_status BIYOS_delvartolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_tolerances<Range>>(mdl, Family::Variable, Range{beg, end});
}

BIYOS_status BIYOS_getnvar(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_variables>(mdl, n);
}

BIYOS_status BIYOS_getvarsp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_int *knd,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    return guarded<get_variables<List>>(mdl, List{n, idx}, knd, lb, ub);
}

BIYOS_status BIYOS_getvarrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            BIYOS_int *knd,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    return guarded<get_variables<Range>>(mdl, Range{beg, end}, knd, lb, ub);
}
