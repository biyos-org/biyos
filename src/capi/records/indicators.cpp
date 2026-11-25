#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <vector>

using biyos::Family;
using biyos::capi::entry_or;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::picked;
using biyos::capi::put;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;

namespace
{
struct Fields
{
    const BIYOS_index *variables = nullptr;
    const BIYOS_int *values = nullptr;
    const BIYOS_index *functions = nullptr;
    const BIYOS_real *lower = nullptr;
    const BIYOS_real *upper = nullptr;
};

struct Outputs
{
    BIYOS_index *variables = nullptr;
    BIYOS_int *values = nullptr;
    BIYOS_index *functions = nullptr;
    BIYOS_real *lower = nullptr;
    BIYOS_real *upper = nullptr;
};

biyos::Indicator given(biyos::Indicator indicator, std::size_t k, const Fields &fields)
{
    indicator.variable = entry_or(fields.variables, k, indicator.variable);
    indicator.value = entry_or(fields.values, k, indicator.value);
    indicator.function = entry_or(fields.functions, k, indicator.function);
    indicator.lower = entry_or(fields.lower, k, indicator.lower);
    indicator.upper = entry_or(fields.upper, k, indicator.upper);
    return indicator;
}

void require_fields(const Fields &fields)
{
    require(fields.variables, "var");
    require(fields.values, "val");
    require(fields.functions, "fun");
}

void add_indicators(BIYOS_model &session, BIYOS_index n, const Fields &fields, BIYOS_index *start)
{
    biyos::Model &core = writable(session);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require_fields(fields);
    }
    const biyos::Indicator free{
        .variable = 0,
        .value = 0,
        .function = 0,
        .lower = -biyos::kInfinity,
        .upper = biyos::kInfinity,
    };
    std::vector<biyos::Indicator> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        added.push_back(given(free, k, fields));
    }
    const biyos::Index index = core.n_indicators();
    core.add_indicators(added);
    if (start != nullptr)
    {
        *start = index;
    }
}

template <typename Selection>
void set_chosen(BIYOS_model &session, Selection selection, const Fields &fields)
{
    biyos::Model &core = writable(session);
    const auto changed = selected(core, Family::Indicator, selection);
    std::vector<biyos::Indicator> indicators;
    indicators.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        indicators.push_back(given(core.indicator(changed[k]), k, fields));
    }
    core.set_indicators(changed, indicators);
}

void n_indicators(const BIYOS_model &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_indicators();
}

void copy_out(const biyos::Indicator &indicator, std::size_t k, const Outputs &outputs)
{
    put(outputs.variables, k, indicator.variable);
    put(outputs.values, k, indicator.value);
    put(outputs.functions, k, indicator.function);
    put(outputs.lower, k, indicator.lower);
    put(outputs.upper, k, indicator.upper);
}

template <typename Selection>
void get_chosen(const BIYOS_model &session, Selection selection, const Outputs &outputs)
{
    const auto chosen = selected(session.model(), Family::Indicator, selection);
    const std::vector<biyos::Indicator> indicators =
        picked(session.model(), &biyos::Model::indicator, chosen);
    for (std::size_t k = 0; k < indicators.size(); ++k)
    {
        copy_out(indicators[k], k, outputs);
    }
}

}

BIYOS_status BIYOS_addind(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_index *var,
                          const BIYOS_int *val,
                          const BIYOS_index *fun,
                          const BIYOS_real *lb,
                          const BIYOS_real *ub,
                          BIYOS_index *pos)
{
    const Fields fields{
        .variables = var,
        .values = val,
        .functions = fun,
        .lower = lb,
        .upper = ub,
    };
    return guarded<add_indicators>(mdl, n, fields, pos);
}

BIYOS_status BIYOS_setindbndsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_chosen<List>>(mdl, List{n, idx}, Fields{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_setindbndrg(BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_chosen<Range>>(mdl, Range{beg, end}, Fields{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_getindbndsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_chosen<List>>(mdl, List{n, idx}, Outputs{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_getindbndrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, Outputs{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_setindvarsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_index *var,
                               const BIYOS_int *val)
{
    const Fields fields{.variables = var, .values = val};
    return guarded<set_chosen<List>>(mdl, List{n, idx}, fields);
}

BIYOS_status BIYOS_setindvarrg(BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               const BIYOS_index *var,
                               const BIYOS_int *val)
{
    const Fields fields{.variables = var, .values = val};
    return guarded<set_chosen<Range>>(mdl, Range{beg, end}, fields);
}

BIYOS_status BIYOS_getindvarsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_index *var,
                               BIYOS_int *val)
{
    const Outputs outputs{.variables = var, .values = val};
    return guarded<get_chosen<List>>(mdl, List{n, idx}, outputs);
}

BIYOS_status BIYOS_getindvarrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_index *var,
                               BIYOS_int *val)
{
    const Outputs outputs{.variables = var, .values = val};
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, outputs);
}

BIYOS_status BIYOS_getnind(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_indicators>(mdl, n);
}

BIYOS_status BIYOS_getindsp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *var,
                            BIYOS_int *val,
                            BIYOS_index *fun,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    const Outputs outputs{
        .variables = var,
        .values = val,
        .functions = fun,
        .lower = lb,
        .upper = ub,
    };
    return guarded<get_chosen<List>>(mdl, List{n, idx}, outputs);
}

BIYOS_status BIYOS_getindrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            BIYOS_index *var,
                            BIYOS_int *val,
                            BIYOS_index *fun,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    const Outputs outputs{
        .variables = var,
        .values = val,
        .functions = fun,
        .lower = lb,
        .upper = ub,
    };
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, outputs);
}
