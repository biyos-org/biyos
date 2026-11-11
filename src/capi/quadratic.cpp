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

void add_terms(biyos_model &model,
               int count,
               const int *functions,
               const std::int64_t *starts,
               const int *firsts,
               const int *seconds,
               const double *values)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> owners = listed(count, functions, "functions");
    const std::vector<biyos::Offset> begins = offsets(owners.size(), starts);
    biyos::check_starts(begins);
    core.check_quadratic_room(begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    core.add_quadratic_terms(owners, begins, biyos::capi::packed(total, firsts, seconds, values));
}

void get_terms(biyos_model &model,
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

biyos_status biyos_add_quadratic_terms(biyos_model *model,
                                       int count,
                                       const int *functions,
                                       const int64_t *starts,
                                       const int *firsts,
                                       const int *seconds,
                                       const double *values)
{
    return guarded<add_terms>(model, count, functions, starts, firsts, seconds, values);
}

biyos_status biyos_get_quadratic_terms(biyos_model *model,
                                       int first,
                                       int count,
                                       int64_t *starts,
                                       int64_t size,
                                       int64_t *needed,
                                       int *firsts,
                                       int *seconds,
                                       double *values)
{
    return guarded<get_terms>(model, first, count, starts, size, needed, firsts, seconds, values);
}
