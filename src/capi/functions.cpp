#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/selection.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using biyos::Family;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::offsets;
using biyos::capi::picked;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;
using biyos::capi::write_starts;

namespace
{

std::vector<double> constants_of(std::size_t n, const double *constants)
{
    if (constants == nullptr)
    {
        return std::vector<double>(n, 0.0);
    }
    return std::vector<double>(constants, constants + n);
}

void add_functions(BIYOS_model &model,
                   int n,
                   const double *constants,
                   const std::int64_t *starts,
                   const int *variables,
                   const double *values,
                   int *start)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(n);
    const std::vector<double> added = constants_of(size, constants);
    const std::vector<biyos::Offset> begins = offsets(size, starts);
    biyos::check_starts(begins);
    core.check_room(size, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    const biyos::Index index = core.n_functions();
    core.add_functions(added, begins, biyos::capi::packed(total, variables, values));
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

void get_functions(const BIYOS_model &model,
                   int start,
                   int size,
                   double *constants,
                   std::int64_t *starts,
                   std::int64_t capacity,
                   std::int64_t *needed,
                   int *variables,
                   double *values)
{
    const std::span<const double> held = model.core.constants(start, size);
    const std::span<const biyos::LinearTerm> found = model.core.linear_terms(start, size);
    const std::size_t room = length(capacity, "size");
    require(needed, "nnz");
    write_constants(held, constants);
    const auto before = [&](biyos::Index j) { return model.core.linear_terms(start, j); };
    write_starts(held.size(), starts, before);
    biyos::capi::unpack(found, room, variables, values);
    *needed = static_cast<std::int64_t>(found.size());
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

BIYOS_status BIYOS_addfuns(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_real *cst,
                           const BIYOS_offset *off,
                           const BIYOS_index *varidx,
                           const BIYOS_real *coef,
                           BIYOS_index *pos)
{
    return guarded<add_functions>(mdl, n, cst, off, varidx, coef, pos);
}

BIYOS_status BIYOS_getnfun(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_functions>(mdl, n);
}

BIYOS_status BIYOS_getfuns(const BIYOS_model *mdl,
                           BIYOS_index pos,
                           BIYOS_index n,
                           BIYOS_offset cap,
                           BIYOS_real *cst,
                           BIYOS_offset *off,
                           BIYOS_offset *nnz,
                           BIYOS_index *varidx,
                           BIYOS_real *coef)
{
    return guarded<get_functions>(mdl, pos, n, cst, off, cap, nnz, varidx, coef);
}

BIYOS_status BIYOS_addcstsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            const BIYOS_real *cst)
{
    return guarded<add_constants<List>>(mdl, List{n, idx}, cst);
}

BIYOS_status BIYOS_addcstrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, const BIYOS_real *cst)
{
    return guarded<add_constants<Range>>(mdl, Range{pos, n}, cst);
}

BIYOS_status BIYOS_setcstsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            const BIYOS_real *cst)
{
    return guarded<set_constants<List>>(mdl, List{n, idx}, cst);
}

BIYOS_status BIYOS_setcstrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, const BIYOS_real *cst)
{
    return guarded<set_constants<Range>>(mdl, Range{pos, n}, cst);
}

BIYOS_status BIYOS_getcstsp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_real *cst)
{
    return guarded<get_constants<List>>(mdl, List{n, idx}, cst);
}

BIYOS_status BIYOS_getcstrg(const BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_real *cst)
{
    return guarded<get_constants<Range>>(mdl, Range{pos, n}, cst);
}
