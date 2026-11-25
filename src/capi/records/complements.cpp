#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/model/tolerances.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "model/model.hpp"

#include <cstddef>
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
using biyos::capi::writable;

namespace
{
struct Fields
{
    const BIYOS_index *functions = nullptr;
    const BIYOS_index *multipliers = nullptr;
    const BIYOS_real *lower = nullptr;
    const BIYOS_real *upper = nullptr;
};

struct Outputs
{
    BIYOS_index *functions = nullptr;
    BIYOS_index *multipliers = nullptr;
    BIYOS_real *lower = nullptr;
    BIYOS_real *upper = nullptr;
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

void add_complements(BIYOS_model &model, BIYOS_index n, const Fields &fields, BIYOS_index *start)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(fields.functions, "fun1");
        require(fields.multipliers, "fun2");
    }
    const biyos::Complement nonnegative{
        .function = 0,
        .lower = 0.0,
        .upper = biyos::kInfinity,
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

void n_complements(const BIYOS_model &model, BIYOS_index *n)
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
                           const BIYOS_index *fun1,
                           const BIYOS_index *fun2,
                           const BIYOS_real *lb,
                           const BIYOS_real *ub,
                           BIYOS_index *pos)
{
    const Fields fields{
        .functions = fun1,
        .multipliers = fun2,
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
                                BIYOS_index beg,
                                BIYOS_index end,
                                const BIYOS_real *lb,
                                const BIYOS_real *ub)
{
    return guarded<set_bounds<Range>>(mdl, Range{beg, end}, Fields{.lower = lb, .upper = ub});
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
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_real *lb,
                                BIYOS_real *ub)
{
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, Outputs{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_setccontolsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const BIYOS_real *atol,
                                const BIYOS_real *rtol)
{
    return guarded<set_tolerances<List>>(mdl, Family::Complement, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_setccontolrg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const BIYOS_real *atol,
                                const BIYOS_real *rtol)
{
    return guarded<set_tolerances<Range>>(mdl, Family::Complement, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_getccontolsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_real *atol,
                                BIYOS_real *rtol)
{
    return guarded<get_tolerances<List>>(mdl, Family::Complement, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_getccontolrg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_real *atol,
                                BIYOS_real *rtol)
{
    return guarded<get_tolerances<Range>>(mdl, Family::Complement, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_delccontolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_tolerances<List>>(mdl, Family::Complement, List{n, idx});
}

BIYOS_status BIYOS_delccontolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_tolerances<Range>>(mdl, Family::Complement, Range{beg, end});
}

BIYOS_status BIYOS_getnccon(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_complements>(mdl, n);
}

BIYOS_status BIYOS_getcconsp(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_index *fun1,
                             BIYOS_index *fun2,
                             BIYOS_real *lb,
                             BIYOS_real *ub)
{
    const Outputs outputs{
        .functions = fun1,
        .multipliers = fun2,
        .lower = lb,
        .upper = ub,
    };
    return guarded<get_chosen<List>>(mdl, List{n, idx}, outputs);
}

BIYOS_status BIYOS_getcconrg(const BIYOS_model *mdl,
                             BIYOS_index beg,
                             BIYOS_index end,
                             BIYOS_index *fun1,
                             BIYOS_index *fun2,
                             BIYOS_real *lb,
                             BIYOS_real *ub)
{
    const Outputs outputs{
        .functions = fun1,
        .multipliers = fun2,
        .lower = lb,
        .upper = ub,
    };
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, outputs);
}
