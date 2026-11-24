#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/linear/grouped.hpp"
#include "capi/model/selection.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

using biyos::Family;
using biyos::QuadraticTerm;
using biyos::capi::Grouped;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::offsets;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;

namespace
{

void add_sliced(biyos::Model &core,
                std::span<const biyos::Index> owners,
                const std::int64_t *starts,
                const int *rows,
                const int *columns,
                const double *values)
{
    const std::vector<biyos::Offset> begins = offsets(owners.size(), starts);
    biyos::check_starts(begins);
    core.check_quadratic_room(begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    core.add_quadratic_terms(owners, begins, biyos::capi::packed(total, rows, columns, values));
}

void add_listed(BIYOS_model &model,
                std::int64_t nnz,
                const int *functions,
                const int *rows,
                const int *columns,
                const double *values)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(functions, "fun");
    }
    core.check_quadratic_room(nnz);
    const std::vector<QuadraticTerm> terms = biyos::capi::packed(size, rows, columns, values);
    const std::span<const biyos::Index> owners(functions, size);
    Grouped<QuadraticTerm> groups =
        biyos::capi::grouped(owners, std::span<const QuadraticTerm>(terms));
    core.add_quadratic_terms(groups.functions, groups.starts, std::move(groups.terms));
}

void add_ranged(BIYOS_model &model,
                Range range,
                const std::int64_t *starts,
                const int *rows,
                const int *columns,
                const double *values)
{
    biyos::Model &core = writable(model);
    add_sliced(core, selected(core, Family::Function, range), starts, rows, columns, values);
}

}

BIYOS_status BIYOS_addquadtermsp(BIYOS_model *mdl,
                                 BIYOS_offset nnz,
                                 const BIYOS_index *fun,
                                 const BIYOS_index *var1,
                                 const BIYOS_index *var2,
                                 const BIYOS_real *coef)
{
    return guarded<add_listed>(mdl, nnz, fun, var1, var2, coef);
}

BIYOS_status BIYOS_addquadtermrg(BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 const BIYOS_offset *off,
                                 const BIYOS_index *var1,
                                 const BIYOS_index *var2,
                                 const BIYOS_real *coef)
{
    return guarded<add_ranged>(mdl, Range{pos, n}, off, var1, var2, coef);
}
