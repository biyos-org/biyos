#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/model/selection.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

using biyos::Family;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::picked;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;

namespace
{

void add_empty(BIYOS_model &model, int n, int *start)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(n);
    core.check_room(size, 0);
    const biyos::Index index = core.n_functions();
    const std::vector<biyos::Offset> starts(size + 1, 0);
    core.add_functions(std::vector<double>(size, 0.0), starts, {});
    if (start != nullptr)
    {
        *start = index;
    }
}

void n_functions(const BIYOS_model &model, int *n)
{
    *require(n, "n") = model.core.n_functions();
}

void write_constants(std::span<const double> found, double *constants)
{
    if (constants != nullptr)
    {
        std::ranges::copy(found, constants);
    }
}

void check_chosen(const biyos::Model &core, std::span<const biyos::Index> chosen)
{
    for (const biyos::Index function : chosen)
    {
        biyos::check_index("function", function, core.n_functions());
    }
}

template <typename Selection>
void add_constants(BIYOS_model &model, Selection selection, const double *constants)
{
    biyos::Model &core = writable(model);
    const auto chosen = selected(core, Family::Function, selection);
    check_chosen(core, chosen);
    if (constants != nullptr)
    {
        core.add_constants(chosen, std::span(constants, chosen.size()));
    }
}

template <typename Selection>
void set_constants(BIYOS_model &model, Selection selection, const double *constants)
{
    biyos::Model &core = writable(model);
    const auto chosen = selected(core, Family::Function, selection);
    check_chosen(core, chosen);
    if (constants != nullptr)
    {
        core.set_constants(chosen, std::span(constants, chosen.size()));
    }
}

template <typename Selection>
void get_constants(const BIYOS_model &model, Selection selection, double *constants)
{
    const auto chosen = selected(model.core, Family::Function, selection);
    write_constants(picked(model.core, &biyos::Model::constant, chosen), constants);
}

}

BIYOS_status BIYOS_addfun(BIYOS_model *mdl, BIYOS_index n, BIYOS_index *pos)
{
    return guarded<add_empty>(mdl, n, pos);
}

BIYOS_status BIYOS_getnfun(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_functions>(mdl, n);
}

BIYOS_status BIYOS_addcstsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            const BIYOS_real *cst)
{
    return guarded<add_constants<List>>(mdl, List{n, idx}, cst);
}

BIYOS_status BIYOS_addcstrg(BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            const BIYOS_real *cst)
{
    return guarded<add_constants<Range>>(mdl, Range{beg, end}, cst);
}

BIYOS_status BIYOS_setcstsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            const BIYOS_real *cst)
{
    return guarded<set_constants<List>>(mdl, List{n, idx}, cst);
}

BIYOS_status BIYOS_setcstrg(BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            const BIYOS_real *cst)
{
    return guarded<set_constants<Range>>(mdl, Range{beg, end}, cst);
}

BIYOS_status BIYOS_getcstsp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_real *cst)
{
    return guarded<get_constants<List>>(mdl, List{n, idx}, cst);
}

BIYOS_status BIYOS_getcstrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            BIYOS_real *cst)
{
    return guarded<get_constants<Range>>(mdl, Range{beg, end}, cst);
}
