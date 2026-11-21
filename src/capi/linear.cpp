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
using biyos::capi::listed;
using biyos::capi::offsets;
using biyos::capi::writable;

namespace
{

void add_lin_terms(BIYOS_model &mdl,
                   int n,
                   const int *funs,
                   const std::int64_t *starts,
                   const int *vars,
                   const double *values)
{
    biyos::Model &core = writable(mdl);
    const std::span<const biyos::Index> owners = listed(n, funs, "funidx");
    const std::vector<biyos::Offset> begins = offsets(owners.size(), starts);
    biyos::check_starts(begins);
    core.check_room(0, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    core.add_lin_terms(owners, begins, biyos::capi::packed(total, vars, values));
}

}

BIYOS_status BIYOS_addlinterms(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *funidx,
                               const BIYOS_offset *off,
                               const BIYOS_index *varidx,
                               const BIYOS_real *coef)
{
    return guarded<add_lin_terms>(mdl, n, funidx, off, varidx, coef);
}
