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

biyos::Objective given(biyos::Objective obj, std::size_t k, const Fields &fields)
{
    const int sense = entry_or(fields.senses, k, static_cast<int>(obj.sense));
    obj.sense = static_cast<biyos::Sense>(sense);
    obj.priority = entry_or(fields.priorities, k, obj.priority);
    obj.weight = entry_or(fields.weights, k, obj.weight);
    obj.absolute = entry_or(fields.absolute, k, obj.absolute);
    obj.relative = entry_or(fields.relative, k, obj.relative);
    return obj;
}

std::vector<biyos::Objective> new_objs(std::span<const biyos::Index> funs,
                                       biyos::Index start,
                                       const Fields &fields)
{
    std::vector<biyos::Objective> added;
    added.reserve(funs.size());
    for (std::size_t k = 0; k < funs.size(); ++k)
    {
        const biyos::Index idx = start + static_cast<biyos::Index>(k);
        added.push_back(given(biyos::default_obj(funs[k], idx), k, fields));
    }
    return added;
}

void add_objs(BIYOS_model &mdl,
              int n,
              const int *funs,
              const int *senses,
              const int *priorities,
              const double *weights,
              const double *absolute,
              const double *relative,
              int *beg)
{
    biyos::Model &core = writable(mdl);
    const std::span<const biyos::Index> owners = listed(n, funs, "funidx");
    core.check_obj_room(owners.size());
    const Fields fields{
        .senses = senses,
        .priorities = priorities,
        .weights = weights,
        .absolute = absolute,
        .relative = relative,
    };
    const biyos::Index start = core.n_objs();
    core.add_objs(new_objs(owners, start, fields));
    if (beg != nullptr)
    {
        *beg = start;
    }
}

void set_objs(BIYOS_model &mdl,
              int n,
              const int *idx,
              const int *senses,
              const int *priorities,
              const double *weights,
              const double *absolute,
              const double *relative)
{
    biyos::Model &core = writable(mdl);
    const std::span<const biyos::Index> changed = listed(n, idx);
    const Fields fields{
        .senses = senses,
        .priorities = priorities,
        .weights = weights,
        .absolute = absolute,
        .relative = relative,
    };
    std::vector<biyos::Objective> objs;
    objs.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        objs.push_back(given(core.objective(changed[k]), k, fields));
    }
    core.set_objs(changed, objs);
}

void n_objs(const BIYOS_model &mdl, int *n)
{
    *require(n, "n") = mdl.core.n_objs();
}

void copy_out(const biyos::Objective &obj, std::size_t k, const Outputs &outputs)
{
    put(outputs.functions, k, obj.function);
    put(outputs.senses, k, static_cast<int>(obj.sense));
    put(outputs.priorities, k, obj.priority);
    put(outputs.weights, k, obj.weight);
    put(outputs.absolute, k, obj.absolute);
    put(outputs.relative, k, obj.relative);
}

void get_objs(const BIYOS_model &mdl,
              int beg,
              int n,
              int *funs,
              int *senses,
              int *priorities,
              double *weights,
              double *absolute,
              double *relative)
{
    const std::span<const biyos::Objective> found = mdl.core.objs(beg, n);
    const Outputs outputs{
        .functions = funs,
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
    return guarded<add_objs>(mdl, n, funidx, sense, prio, coef, atol, rtol, beg);
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
    return guarded<set_objs>(mdl, n, idx, sense, prio, coef, atol, rtol);
}

BIYOS_status BIYOS_getnobjs(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_objs>(mdl, n);
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
    return guarded<get_objs>(mdl, beg, n, funidx, sense, prio, coef, atol, rtol);
}
