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

std::vector<double> constants_of(std::size_t n, const double *constants)
{
    if (constants == nullptr)
    {
        return std::vector<double>(n, 0.0);
    }
    return std::vector<double>(constants, constants + n);
}

void add_funs(BIYOS_model &mdl,
              int n,
              const double *constants,
              const std::int64_t *starts,
              const int *vars,
              const double *values,
              int *beg)
{
    biyos::Model &core = writable(mdl);
    const std::size_t size = length(n);
    const std::vector<double> added = constants_of(size, constants);
    const std::vector<biyos::Offset> begins = offsets(size, starts);
    biyos::check_starts(begins);
    core.check_room(size, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    const biyos::Index start = core.n_functions();
    core.add_functions(added, begins, biyos::capi::packed(total, vars, values));
    if (beg != nullptr)
    {
        *beg = start;
    }
}

void n_funs(const BIYOS_model &mdl, int *n)
{
    *require(n, "n") = mdl.core.n_functions();
}

void write_constants(std::span<const double> found, double *constants)
{
    if (constants != nullptr)
    {
        std::ranges::copy(found, constants);
    }
}

void get_funs(const BIYOS_model &mdl,
              int beg,
              int n,
              double *constants,
              std::int64_t *starts,
              std::int64_t size,
              std::int64_t *needed,
              int *vars,
              double *values)
{
    const std::span<const double> held = mdl.core.constants(beg, n);
    const std::span<const biyos::LinearTerm> found = mdl.core.linear_terms(beg, n);
    const std::size_t room = length(size, "size");
    require(needed, "nnz");
    write_constants(held, constants);
    const auto before = [&](biyos::Index j) { return mdl.core.linear_terms(beg, j); };
    write_starts(held.size(), starts, before);
    biyos::capi::unpack(found, room, vars, values);
    *needed = static_cast<std::int64_t>(found.size());
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
    return guarded<add_funs>(mdl, n, cst, off, varidx, coef, pos);
}

BIYOS_status BIYOS_getnfun(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_funs>(mdl, n);
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
    return guarded<get_funs>(mdl, pos, n, cst, off, cap, nnz, varidx, coef);
}
