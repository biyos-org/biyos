#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/selection.hpp"
#include "capi/tolerances.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <limits>
#include <vector>

using biyos::Family;
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
using biyos::capi::ToleranceCalls;
using biyos::capi::writable;

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();

constexpr ToleranceCalls kTolerances{
    .family = Family::Complement,
    .read = &biyos::Model::complement_tolerance,
    .set = &biyos::Model::set_complement_tolerances,
    .remove = &biyos::Model::delete_complement_tolerances,
};

struct Fields
{
    const int *functions = nullptr;
    const int *multipliers = nullptr;
    const double *lower = nullptr;
    const double *upper = nullptr;
};

struct Outputs
{
    int *functions = nullptr;
    int *multipliers = nullptr;
    double *lower = nullptr;
    double *upper = nullptr;
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

biyos::Complement given(biyos::Complement complement, std::size_t k, const Fields &fields)
{
    complement.function = entry_or(fields.functions, k, complement.function);
    complement.multiplier = entry_or(fields.multipliers, k, complement.multiplier);
    complement.lower = entry_or(fields.lower, k, complement.lower);
    complement.upper = entry_or(fields.upper, k, complement.upper);
    return complement;
}

void add_complements(BIYOS_model &model, int n, const Fields &fields, int *start)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(fields.functions, "funidx1");
        require(fields.multipliers, "funidx2");
    }
    const biyos::Complement nonnegative{
        .function = 0,
        .lower = 0.0,
        .upper = kInfinity,
        .multiplier = 0,
    };
    std::vector<biyos::Complement> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        added.push_back(given(nonnegative, k, fields));
    }
    const biyos::Index index = core.n_complements();
    core.add_complements(added);
    if (start != nullptr)
    {
        *start = index;
    }
}

template <typename Selection>
void set_bounds(BIYOS_model &model, Selection selection, const Fields &fields)
{
    biyos::Model &core = writable(model);
    const auto changed = selected(core, Family::Complement, selection);
    std::vector<biyos::Complement> complements;
    complements.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        complements.push_back(given(core.complement(changed[k]), k, fields));
    }
    core.set_complements(changed, complements);
}

void n_complements(const BIYOS_model &model, int *n)
{
    *require(n, "n") = model.core.n_complements();
}

void copy_out(const biyos::Complement &complement, std::size_t k, const Outputs &outputs)
{
    put(outputs.functions, k, complement.function);
    put(outputs.multipliers, k, complement.multiplier);
    put(outputs.lower, k, complement.lower);
    put(outputs.upper, k, complement.upper);
}

template <typename Selection>
void get_chosen(const BIYOS_model &model, Selection selection, const Outputs &outputs)
{
    const auto chosen = selected(model.core, Family::Complement, selection);
    const std::vector<biyos::Complement> complements =
        picked(model.core, &biyos::Model::complement, chosen);
    for (std::size_t k = 0; k < complements.size(); ++k)
    {
        copy_out(complements[k], k, outputs);
    }
}

}

BIYOS_status BIYOS_addccon(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_index *funidx1,
                           const BIYOS_index *funidx2,
                           const BIYOS_real *lb,
                           const BIYOS_real *ub,
                           BIYOS_index *pos)
{
    const Fields fields{
        .functions = funidx1,
        .multipliers = funidx2,
        .lower = lb,
        .upper = ub,
    };
    return guarded<add_complements>(mdl, n, fields, pos);
}

BIYOS_status BIYOS_setcconbndsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const BIYOS_real *lb,
                                const BIYOS_real *ub)
{
    return guarded<set_bounds<List>>(mdl, List{n, idx}, Fields{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_setcconbndrg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const BIYOS_real *lb,
                                const BIYOS_real *ub)
{
    return guarded<set_bounds<Range>>(mdl, Range{pos, n}, Fields{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_getcconbndsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_real *lb,
                                BIYOS_real *ub)
{
    return guarded<get_chosen<List>>(mdl, List{n, idx}, Outputs{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_getcconbndrg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                BIYOS_real *lb,
                                BIYOS_real *ub)
{
    return guarded<get_chosen<Range>>(mdl, Range{pos, n}, Outputs{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_setccontolsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const BIYOS_real *atol,
                                const BIYOS_real *rtol)
{
    return guarded<set_tolerances<List>>(mdl, kTolerances, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_setccontolrg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const BIYOS_real *atol,
                                const BIYOS_real *rtol)
{
    return guarded<set_tolerances<Range>>(mdl, kTolerances, Range{pos, n}, atol, rtol);
}

BIYOS_status BIYOS_getccontolsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_real *atol,
                                BIYOS_real *rtol)
{
    return guarded<get_tolerances<List>>(mdl, kTolerances, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_getccontolrg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                BIYOS_real *atol,
                                BIYOS_real *rtol)
{
    return guarded<get_tolerances<Range>>(mdl, kTolerances, Range{pos, n}, atol, rtol);
}

BIYOS_status BIYOS_delccontolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_tolerances<List>>(mdl, kTolerances, List{n, idx});
}

BIYOS_status BIYOS_delccontolrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_tolerances<Range>>(mdl, kTolerances, Range{pos, n});
}

BIYOS_status BIYOS_getnccon(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_complements>(mdl, n);
}

BIYOS_status BIYOS_getcconsp(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_index *funidx1,
                             BIYOS_index *funidx2,
                             BIYOS_real *lb,
                             BIYOS_real *ub)
{
    const Outputs outputs{
        .functions = funidx1,
        .multipliers = funidx2,
        .lower = lb,
        .upper = ub,
    };
    return guarded<get_chosen<List>>(mdl, List{n, idx}, outputs);
}

BIYOS_status BIYOS_getcconrg(const BIYOS_model *mdl,
                             BIYOS_index pos,
                             BIYOS_index n,
                             BIYOS_index *funidx1,
                             BIYOS_index *funidx2,
                             BIYOS_real *lb,
                             BIYOS_real *ub)
{
    const Outputs outputs{
        .functions = funidx1,
        .multipliers = funidx2,
        .lower = lb,
        .upper = ub,
    };
    return guarded<get_chosen<Range>>(mdl, Range{pos, n}, outputs);
}
