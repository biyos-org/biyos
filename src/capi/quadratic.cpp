#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::offsets;
using biyos::capi::require;
using biyos::capi::writable;
using biyos::capi::write_starts;

namespace
{

void add_terms(BIYOS_model &model,
               int count,
               const int *functions,
               const std::int64_t *starts,
               const int *firsts,
               const int *seconds,
               const double *values)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> owners = listed(count, functions, "funs");
    const std::vector<biyos::Offset> begins = offsets(owners.size(), starts);
    biyos::check_starts(begins);
    core.check_quadratic_room(begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    core.add_quadratic_terms(owners, begins, biyos::capi::packed(total, firsts, seconds, values));
}

void get_terms(BIYOS_model &model,
               int first,
               int count,
               std::int64_t *starts,
               std::int64_t size,
               std::int64_t *needed,
               int *firsts,
               int *seconds,
               double *values)
{
    const std::span<const biyos::QuadraticTerm> found = model.core.quadratic_terms(first, count);
    const std::size_t room = length(size, "size");
    require(needed, "needed");
    const auto before = [&](biyos::Index j) { return model.core.quadratic_terms(first, j); };
    write_starts(static_cast<std::size_t>(count), starts, before);
    biyos::capi::unpack(found, room, firsts, seconds, values);
    *needed = static_cast<std::int64_t>(found.size());
}

}

BIYOS_status BIYOS_addquadterms(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *funs,
                                const BIYOS_offset *starts,
                                const BIYOS_index *vars1,
                                const BIYOS_index *vars2,
                                const BIYOS_real *vals)
{
    return guarded<add_terms>(mdl, n, funs, starts, vars1, vars2, vals);
}

BIYOS_status BIYOS_getquadterms(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index n,
                                BIYOS_offset size,
                                BIYOS_offset *starts,
                                BIYOS_offset *needed,
                                BIYOS_index *vars1,
                                BIYOS_index *vars2,
                                BIYOS_real *vals)
{
    return guarded<get_terms>(mdl, beg, n, starts, size, needed, vars1, vars2, vals);
}
