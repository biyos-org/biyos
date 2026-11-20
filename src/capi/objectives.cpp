#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::listed;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

struct Fields
{
    const int *senses;
    const int *priorities;
    const double *weights;
    const double *absolute;
    const double *relative;
};

struct Outputs
{
    int *functions;
    int *senses;
    int *priorities;
    double *weights;
    double *absolute;
    double *relative;
};

template <typename T> T entry_or(const T *values, std::size_t k, T held)
{
    return values == nullptr ? held : values[k];
}

template <typename T> void put(T *values, std::size_t k, T value)
{
    if (values != nullptr)
    {
        values[k] = value;
    }
}

biyos::Objective given(biyos::Objective objective, std::size_t k, const Fields &fields)
{
    const int sense = entry_or(fields.senses, k, static_cast<int>(objective.sense));
    objective.sense = static_cast<biyos::Sense>(sense);
    objective.priority = entry_or(fields.priorities, k, objective.priority);
    objective.weight = entry_or(fields.weights, k, objective.weight);
    objective.absolute = entry_or(fields.absolute, k, objective.absolute);
    objective.relative = entry_or(fields.relative, k, objective.relative);
    return objective;
}

std::vector<biyos::Objective> new_objectives(std::span<const biyos::Index> functions,
                                             biyos::Index start,
                                             const Fields &fields)
{
    std::vector<biyos::Objective> added;
    added.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        const biyos::Index index = start + static_cast<biyos::Index>(k);
        added.push_back(given(biyos::default_objective(functions[k], index), k, fields));
    }
    return added;
}

void add_objectives(BIYOS_model &model,
                    int count,
                    const int *functions,
                    const int *senses,
                    const int *priorities,
                    const double *weights,
                    const double *absolute,
                    const double *relative,
                    int *first)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> owners = listed(count, functions, "funs");
    core.check_objective_room(owners.size());
    const Fields fields{
        .senses = senses,
        .priorities = priorities,
        .weights = weights,
        .absolute = absolute,
        .relative = relative,
    };
    const biyos::Index start = core.objective_count();
    core.add_objectives(new_objectives(owners, start, fields));
    if (first != nullptr)
    {
        *first = start;
    }
}

void set_objectives(BIYOS_model &model,
                    int count,
                    const int *indices,
                    const int *senses,
                    const int *priorities,
                    const double *weights,
                    const double *absolute,
                    const double *relative)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> changed = listed(count, indices);
    const Fields fields{
        .senses = senses,
        .priorities = priorities,
        .weights = weights,
        .absolute = absolute,
        .relative = relative,
    };
    std::vector<biyos::Objective> objectives;
    objectives.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        objectives.push_back(given(core.objective(changed[k]), k, fields));
    }
    core.set_objectives(changed, objectives);
}

void objective_count(BIYOS_model &model, int *count)
{
    *require(count, "n") = model.core.objective_count();
}

void copy_out(const biyos::Objective &objective, std::size_t k, const Outputs &outputs)
{
    put(outputs.functions, k, objective.function);
    put(outputs.senses, k, static_cast<int>(objective.sense));
    put(outputs.priorities, k, objective.priority);
    put(outputs.weights, k, objective.weight);
    put(outputs.absolute, k, objective.absolute);
    put(outputs.relative, k, objective.relative);
}

void get_objectives(BIYOS_model &model,
                    int first,
                    int count,
                    int *functions,
                    int *senses,
                    int *priorities,
                    double *weights,
                    double *absolute,
                    double *relative)
{
    const std::span<const biyos::Objective> found = model.core.objectives(first, count);
    const Outputs outputs{
        .functions = functions,
        .senses = senses,
        .priorities = priorities,
        .weights = weights,
        .absolute = absolute,
        .relative = relative,
    };
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        copy_out(found[k], k, outputs);
    }
}

}

BIYOS_status BIYOS_addobjs(BIYOS_model *mdl,
                           int n,
                           const int *funs,
                           const int *senses,
                           const int *prios,
                           const double *weights,
                           const double *abs,
                           const double *rel,
                           int *beg)
{
    return guarded<add_objectives>(mdl, n, funs, senses, prios, weights, abs, rel, beg);
}

BIYOS_status BIYOS_setobjs(BIYOS_model *mdl,
                           int n,
                           const int *idx,
                           const int *senses,
                           const int *prios,
                           const double *weights,
                           const double *abs,
                           const double *rel)
{
    return guarded<set_objectives>(mdl, n, idx, senses, prios, weights, abs, rel);
}

BIYOS_status BIYOS_getnobjs(BIYOS_model *mdl, int *n)
{
    return guarded<objective_count>(mdl, n);
}

BIYOS_status BIYOS_getobjs(BIYOS_model *mdl,
                           int beg,
                           int n,
                           int *funs,
                           int *senses,
                           int *prios,
                           double *weights,
                           double *abs,
                           double *rel)
{
    return guarded<get_objectives>(mdl, beg, n, funs, senses, prios, weights, abs, rel);
}
