#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/env.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::idle;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::picked;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

void create(biyos::Env &env, BIYOS_model **mdl)
{
    BIYOS_model **made = require(mdl, "mdl");
    writable(env);
    *made = new BIYOS_model(env);
}

void make_copy(const BIYOS_model &mdl, BIYOS_env *env, BIYOS_model **copy)
{
    BIYOS_model **made = require(copy, "copy");
    biyos::Env &target = idle(require(env, "env")->core, "argument 'env'");
    *made = new BIYOS_model(target, mdl.core);
}

void freeable(BIYOS_model &mdl)
{
    writable(mdl);
}

biyos::VariableKind kind_at(const int *knds, std::size_t k)
{
    return knds == nullptr ? biyos::VariableKind::Continuous
                           : static_cast<biyos::VariableKind>(knds[k]);
}

biyos::Variable bounded(biyos::Variable var,
                        std::size_t k,
                        const double *lower,
                        const double *upper)
{
    if (lower != nullptr)
    {
        var.lower = lower[k];
    }
    if (upper != nullptr)
    {
        var.upper = upper[k];
    }
    return var;
}

biyos::Variable kinded(biyos::Variable var, std::size_t k, const int *knds)
{
    if (knds != nullptr)
    {
        var.kind = static_cast<biyos::VariableKind>(knds[k]);
    }
    return var;
}

template <auto Change, typename... Arrays>
void set_each(biyos::Model &core, std::span<const biyos::Index> idx, Arrays... arrays)
{
    std::vector<biyos::Variable> changed;
    changed.reserve(idx.size());
    for (std::size_t k = 0; k < idx.size(); ++k)
    {
        changed.push_back(Change(core.variable(idx[k]), k, arrays...));
    }
    core.set_vars(idx, changed);
}

void add_vars(BIYOS_model &mdl,
              int n,
              const int *knds,
              const double *lower,
              const double *upper,
              int *beg)
{
    biyos::Model &core = writable(mdl);
    const std::size_t size = length(n);
    std::vector<biyos::Variable> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        added.push_back(bounded(biyos::widest(kind_at(knds, k)), k, lower, upper));
    }
    const biyos::Index start = core.n_vars();
    core.add_vars(added);
    if (beg != nullptr)
    {
        *beg = start;
    }
}

void set_bnds(BIYOS_model &mdl, int n, const int *idx, const double *lower, const double *upper)
{
    biyos::Model &core = writable(mdl);
    set_each<bounded>(core, listed(n, idx), lower, upper);
}

void set_kinds(BIYOS_model &mdl, int n, const int *idx, const int *knds)
{
    biyos::Model &core = writable(mdl);
    set_each<kinded>(core, listed(n, idx), knds);
}

void n_vars(const BIYOS_model &mdl, int *n)
{
    *require(n, "n") = mdl.core.n_vars();
}

void copy_out(const biyos::Variable &var, std::size_t k, int *knds, double *lower, double *upper)
{
    if (knds != nullptr)
    {
        knds[k] = static_cast<int>(var.kind);
    }
    if (lower != nullptr)
    {
        lower[k] = var.lower;
    }
    if (upper != nullptr)
    {
        upper[k] = var.upper;
    }
}

void get_vars(const BIYOS_model &mdl, int beg, int n, int *knds, double *lower, double *upper)
{
    const std::span<const biyos::Variable> found = mdl.core.vars(beg, n);
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        copy_out(found[k], k, knds, lower, upper);
    }
}

void get_bnds(const BIYOS_model &mdl, int n, const int *idx, double *lower, double *upper)
{
    const std::vector<biyos::Variable> vars =
        picked(mdl.core, &biyos::Model::variable, listed(n, idx));
    for (std::size_t k = 0; k < vars.size(); ++k)
    {
        copy_out(vars[k], k, nullptr, lower, upper);
    }
}

void get_kinds(const BIYOS_model &mdl, int n, const int *idx, int *knds)
{
    const std::vector<biyos::Variable> vars =
        picked(mdl.core, &biyos::Model::variable, listed(n, idx));
    for (std::size_t k = 0; k < vars.size(); ++k)
    {
        copy_out(vars[k], k, knds, nullptr, nullptr);
    }
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
                          BIYOS_index *beg)
{
    return guarded<add_vars>(mdl, n, knd, lb, ub, beg);
}

BIYOS_status BIYOS_setvarbndsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_bnds>(mdl, n, idx, lb, ub);
}

BIYOS_status BIYOS_getvarbndsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_bnds>(mdl, n, idx, lb, ub);
}

BIYOS_status BIYOS_setvarkndsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_int *knd)
{
    return guarded<set_kinds>(mdl, n, idx, knd);
}

BIYOS_status BIYOS_getvarkndsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_int *knd)
{
    return guarded<get_kinds>(mdl, n, idx, knd);
}

BIYOS_status BIYOS_getnvar(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_vars>(mdl, n);
}

BIYOS_status BIYOS_getvarrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index n,
                            BIYOS_int *knd,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    return guarded<get_vars>(mdl, beg, n, knd, lb, ub);
}
