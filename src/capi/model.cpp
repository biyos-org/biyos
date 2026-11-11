#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/environment.hpp"
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

void create(biyos::Environment &env, biyos_model **model)
{
    biyos_model **made = require(model, "model");
    writable(env);
    *made = new biyos_model(env);
}

void make_copy(biyos_model &model, biyos_env *env, biyos_model **copy)
{
    biyos_model **made = require(copy, "copy");
    biyos::Environment &target = idle(require(env, "env")->core, "argument 'env'");
    *made = new biyos_model(target, model.core);
}

void freeable(biyos_model &model)
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

void add_variables(biyos_model &model,
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

void set_bounds(biyos_model &model,
                int count,
                const int *indices,
                const double *lower,
                const double *upper)
{
    biyos::Model &core = writable(model);
    set_each<bounded>(core, listed(count, indices), lower, upper);
}

void set_kinds(biyos_model &model, int count, const int *indices, const int *kinds)
{
    biyos::Model &core = writable(model);
    set_each<kinded>(core, listed(count, indices), kinds);
}

void variable_count(biyos_model &model, int *count)
{
    *require(count, "count") = model.core.variable_count();
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

void get_variables(biyos_model &model,
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

biyos_status biyos_model_create(biyos_env *env, biyos_model **model)
{
    if (model != nullptr)
    {
        *model = nullptr;
    }
    return guarded<create>(env, model);
}

biyos_status biyos_model_copy(biyos_model *model, biyos_env *env, biyos_model **copy)
{
    if (copy != nullptr)
    {
        *copy = nullptr;
    }
    return guarded<make_copy>(model, env, copy);
}

biyos_status biyos_model_free(biyos_model *model)
{
    if (model == nullptr)
    {
        return BIYOS_OK;
    }
    const biyos_status status = guarded<freeable>(model);
    if (status == BIYOS_OK)
    {
        delete model;
    }
    return status;
}

biyos_status biyos_add_variables(biyos_model *model,
                                 int count,
                                 const int *kinds,
                                 const double *lower,
                                 const double *upper,
                                 int *first)
{
    return guarded<add_variables>(model, count, kinds, lower, upper, first);
}

biyos_status biyos_set_variable_bounds(biyos_model *model,
                                       int count,
                                       const int *indices,
                                       const double *lower,
                                       const double *upper)
{
    return guarded<set_bounds>(model, count, indices, lower, upper);
}

biyos_status biyos_set_variable_kinds(biyos_model *model,
                                      int count,
                                      const int *indices,
                                      const int *kinds)
{
    return guarded<set_kinds>(model, count, indices, kinds);
}

biyos_status biyos_variable_count(biyos_model *model, int *count)
{
    return guarded<variable_count>(model, count);
}

biyos_status biyos_get_variables(biyos_model *model,
                                 int first,
                                 int count,
                                 int *kinds,
                                 double *lower,
                                 double *upper)
{
    return guarded<get_variables>(model, first, count, kinds, lower, upper);
}
