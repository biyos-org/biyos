#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::offsets;
using biyos::capi::require;
using biyos::capi::writable;
using biyos::capi::write_starts;

namespace
{

std::vector<double> constants_of(std::size_t count, const double *constants)
{
    if (constants == nullptr)
    {
        return std::vector<double>(count, 0.0);
    }
    return std::vector<double>(constants, constants + count);
}

void add_functions(BIYOS_model &model,
                   int count,
                   const double *constants,
                   const std::int64_t *starts,
                   const int *variables,
                   const double *values,
                   int *first)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(count);
    const std::vector<double> added = constants_of(size, constants);
    const std::vector<biyos::Offset> begins = offsets(size, starts);
    biyos::check_starts(begins);
    core.check_room(size, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    const biyos::Index start = core.function_count();
    core.add_functions(added, begins, biyos::capi::packed(total, variables, values));
    if (first != nullptr)
    {
        *first = start;
    }
}

void function_count(BIYOS_model &model, int *count)
{
    *require(count, "n") = model.core.function_count();
}

void write_constants(std::span<const double> found, double *constants)
{
    if (constants != nullptr)
    {
        std::ranges::copy(found, constants);
    }
}

void get_functions(BIYOS_model &model,
                   int first,
                   int count,
                   double *constants,
                   std::int64_t *starts,
                   std::int64_t size,
                   std::int64_t *needed,
                   int *variables,
                   double *values)
{
    const std::span<const double> held = model.core.constants(first, count);
    const std::span<const biyos::LinearTerm> found = model.core.terms(first, count);
    const std::size_t room = length(size, "size");
    require(needed, "needed");
    write_constants(held, constants);
    const auto before = [&](biyos::Index j) { return model.core.terms(first, j); };
    write_starts(held.size(), starts, before);
    biyos::capi::unpack(found, room, variables, values);
    *needed = static_cast<std::int64_t>(found.size());
}

}

BIYOS_status BIYOS_addfuns(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_real *consts,
                           const BIYOS_offset *starts,
                           const BIYOS_index *varidx,
                           const BIYOS_real *vals,
                           BIYOS_index *beg)
{
    return guarded<add_functions>(mdl, n, consts, starts, varidx, vals, beg);
}

BIYOS_status BIYOS_getnfuns(BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<function_count>(mdl, n);
}

BIYOS_status BIYOS_getfuns(BIYOS_model *mdl,
                           BIYOS_index beg,
                           BIYOS_index n,
                           BIYOS_real *consts,
                           BIYOS_offset *starts,
                           BIYOS_offset size,
                           BIYOS_offset *needed,
                           BIYOS_index *varidx,
                           BIYOS_real *vals)
{
    return guarded<get_functions>(mdl, beg, n, consts, starts, size, needed, varidx, vals);
}
