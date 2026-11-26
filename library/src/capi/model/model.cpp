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
#include "model/utils/gather.hpp"

#include <cstddef>
#include <span>
#include <vector>

using biyos::EntryTimer;
using biyos::Env;
using biyos::Family;
using biyos::Index;
using biyos::kModule;
using biyos::Model;
using biyos::picked;
using biyos::Session;
using biyos::Variable;
using biyos::VariableKind;
using biyos::widest;
using biyos::capi::delete_tolerances;
using biyos::capi::EnvAccess;
using biyos::capi::get_tolerances;
using biyos::capi::guarded;
using biyos::capi::idle;
using biyos::capi::kinds_of;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::read_field;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::set_tolerances;
using biyos::capi::writable;
using biyos::capi::write_field;

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

std::vector<Variable> widest_variables(std::span<const VariableKind> kinds)
{
    std::vector<Variable> variables;
    variables.reserve(kinds.size());
    for (const VariableKind kind : kinds)
    {
        variables.push_back(widest(kind));
    }
    return variables;
}

void add_variables(Session &session,
                   BIYOS_index n,
                   const BIYOS_int *codes,
                   const BIYOS_real *lower,
                   const BIYOS_real *upper,
                   BIYOS_index *start)
{
    Model &model = writable(session);
    const std::size_t size = length(n);
    const std::vector<VariableKind> kinds = codes == nullptr
                                                ? std::vector(size, VariableKind::Continuous)
                                                : kinds_of<VariableKind>(std::span(codes, size));
    std::vector<Variable> added = widest_variables(kinds);
    read_field(added, &Variable::lower, lower);
    read_field(added, &Variable::upper, upper);
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
    const auto chosen = selected(Family::Variable, model.n_of(Family::Variable), selection);
    std::vector<Variable> changed = picked(model, &Model::variable, chosen);
    read_field(changed, &Variable::lower, lower);
    read_field(changed, &Variable::upper, upper);
    model.set_variables(chosen, changed);
}

template <typename Selection>
void set_kinds(Session &session, Selection selection, const BIYOS_int *kinds)
{
    Model &model = writable(session);
    const auto chosen = selected(Family::Variable, model.n_of(Family::Variable), selection);
    std::vector<Variable> changed = picked(model, &Model::variable, chosen);
    read_field(changed, &Variable::kind, kinds);
    model.set_variables(chosen, changed);
}

void n_variables(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_variables();
}

template <typename Selection>
void get_variables(const Session &session,
                   Selection selection,
                   BIYOS_int *kinds,
                   BIYOS_real *lower,
                   BIYOS_real *upper)
{
    const auto chosen =
        selected(Family::Variable, session.model().n_of(Family::Variable), selection);
    const std::vector<Variable> variables = picked(session.model(), &Model::variable, chosen);
    write_field(variables, &Variable::kind, kinds);
    write_field(variables, &Variable::lower, lower);
    write_field(variables, &Variable::upper, upper);
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
