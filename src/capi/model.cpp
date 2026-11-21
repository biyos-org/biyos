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
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

void create(biyos::Env &env, BIYOS_model **model)
{
    BIYOS_model **made = require(model, "mdl");
    writable(env);
    *made = new BIYOS_model(env);
}

void make_copy(const BIYOS_model &model, BIYOS_env *env, BIYOS_model **copy)
{
    BIYOS_model **made = require(copy, "copy");
    biyos::Env &target = idle(require(env, "env")->core, "argument 'env'");
    *made = new BIYOS_model(target, model.core);
}

void freeable(BIYOS_model &model)
{
    writable(model);
}

biyos::VariableKind kind_at(const int *kinds, std::size_t k)
{
    return kinds == nullptr ? biyos::VariableKind::Continuous
                            : static_cast<biyos::VariableKind>(kinds[k]);
}

biyos::Variable bounded(biyos::Variable variable,
                        std::size_t k,
                        const double *lower,
                        const double *upper)
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

biyos::Variable kinded(biyos::Variable variable, std::size_t k, const int *kinds)
{
    if (kinds != nullptr)
    {
        variable.kind = static_cast<biyos::VariableKind>(kinds[k]);
    }
    return variable;
}

template <auto Change, typename... Arrays>
void set_each(biyos::Model &core, std::span<const biyos::Index> indices, Arrays... arrays)
{
    std::vector<biyos::Variable> changed;
    changed.reserve(indices.size());
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        changed.push_back(Change(core.variable(indices[k]), k, arrays...));
    }
    core.set_variables(indices, changed);
}

void add_variables(BIYOS_model &model,
                   int count,
                   const int *kinds,
                   const double *lower,
                   const double *upper,
                   int *first)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(count);
    std::vector<biyos::Variable> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        added.push_back(bounded(biyos::widest(kind_at(kinds, k)), k, lower, upper));
    }
    const biyos::Index start = core.variable_count();
    core.add_variables(added);
    if (first != nullptr)
    {
        *first = start;
    }
}

void set_bounds(BIYOS_model &model,
                int count,
                const int *indices,
                const double *lower,
                const double *upper)
{
    biyos::Model &core = writable(model);
    set_each<bounded>(core, listed(count, indices), lower, upper);
}

void set_kinds(BIYOS_model &model, int count, const int *indices, const int *kinds)
{
    biyos::Model &core = writable(model);
    set_each<kinded>(core, listed(count, indices), kinds);
}

void variable_count(const BIYOS_model &model, int *count)
{
    *require(count, "n") = model.core.variable_count();
}

void copy_out(const biyos::Variable &variable,
              std::size_t k,
              int *kinds,
              double *lower,
              double *upper)
{
    if (kinds != nullptr)
    {
        kinds[k] = static_cast<int>(variable.kind);
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

void get_variables(const BIYOS_model &model,
                   int first,
                   int count,
                   int *kinds,
                   double *lower,
                   double *upper)
{
    const std::span<const biyos::Variable> found = model.core.variables(first, count);
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        copy_out(found[k], k, kinds, lower, upper);
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

BIYOS_status BIYOS_addvars(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_int *knd,
                           const BIYOS_real *lb,
                           const BIYOS_real *ub,
                           BIYOS_index *beg)
{
    return guarded<add_variables>(mdl, n, knd, lb, ub, beg);
}

BIYOS_status BIYOS_setvarbnd(BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             const BIYOS_real *lb,
                             const BIYOS_real *ub)
{
    return guarded<set_bounds>(mdl, n, idx, lb, ub);
}

BIYOS_status BIYOS_setvarknd(BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             const BIYOS_int *knd)
{
    return guarded<set_kinds>(mdl, n, idx, knd);
}

BIYOS_status BIYOS_getnvars(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<variable_count>(mdl, n);
}

BIYOS_status BIYOS_getvars(const BIYOS_model *mdl,
                           BIYOS_index beg,
                           BIYOS_index n,
                           BIYOS_int *knd,
                           BIYOS_real *lb,
                           BIYOS_real *ub)
{
    return guarded<get_variables>(mdl, beg, n, knd, lb, ub);
}
