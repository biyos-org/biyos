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
        added.push_back(given(biyos::default_obj(functions[k], index), k, fields));
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
    const std::span<const biyos::Index> owners = listed(count, functions, "funidx");
    core.check_obj_room(owners.size());
    const Fields fields{
        .senses = senses,
        .priorities = priorities,
        .weights = weights,
        .absolute = absolute,
        .relative = relative,
    };
    const biyos::Index start = core.n_objs();
    core.add_objs(new_objectives(owners, start, fields));
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
    core.set_objs(changed, objectives);
}

void objective_count(const BIYOS_model &model, int *count)
{
    *require(count, "n") = model.core.n_objs();
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

void get_objectives(const BIYOS_model &model,
                    int first,
                    int count,
                    int *functions,
                    int *senses,
                    int *priorities,
                    double *weights,
                    double *absolute,
                    double *relative)
{
    const std::span<const biyos::Objective> found = model.core.objs(first, count);
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
                           BIYOS_index n,
                           const BIYOS_index *funidx,
                           const BIYOS_int *sense,
                           const BIYOS_int *prio,
                           const BIYOS_real *coef,
                           const BIYOS_real *atol,
                           const BIYOS_real *rtol,
                           BIYOS_index *beg)
{
    return guarded<add_objectives>(mdl, n, funidx, sense, prio, coef, atol, rtol, beg);
}

BIYOS_status BIYOS_setobjs(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_index *idx,
                           const BIYOS_int *sense,
                           const BIYOS_int *prio,
                           const BIYOS_real *coef,
                           const BIYOS_real *atol,
                           const BIYOS_real *rtol)
{
    return guarded<set_objectives>(mdl, n, idx, sense, prio, coef, atol, rtol);
}

BIYOS_status BIYOS_getnobjs(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<objective_count>(mdl, n);
}

BIYOS_status BIYOS_getobjs(const BIYOS_model *mdl,
                           BIYOS_index beg,
                           BIYOS_index n,
                           BIYOS_index *funidx,
                           BIYOS_int *sense,
                           BIYOS_int *prio,
                           BIYOS_real *coef,
                           BIYOS_real *atol,
                           BIYOS_real *rtol)
{
    return guarded<get_objectives>(mdl, beg, n, funidx, sense, prio, coef, atol, rtol);
}
