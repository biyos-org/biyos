#include "biyos/biyos.h"

#include "capi/linear/grouped.hpp"
#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

using biyos::Family;
using biyos::QuadraticTerm;
using biyos::to_size;
using biyos::capi::Grouped;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::starts_of;
using biyos::capi::writable;

namespace
{

void add_sliced(biyos::Model &model,
                std::span<const biyos::Index> owners,
                const BIYOS_offset *counts,
                const BIYOS_index *rows,
                const BIYOS_index *columns,
                const BIYOS_real *values)
{
    const std::vector<biyos::Offset> begins = starts_of(owners.size(), counts);
    model.check_quadratic_room(begins.back());
    const auto total = to_size(begins.back());
    model.add_quadratic_terms(owners, begins, biyos::capi::packed(total, rows, columns, values));
}

void add_listed(biyos::Session &session,
                BIYOS_offset nnz,
                const BIYOS_index *functions,
                const BIYOS_index *rows,
                const BIYOS_index *columns,
                const BIYOS_real *values)
{
    biyos::Model &model = writable(session);
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(functions, "fun");
    }
    model.check_quadratic_room(nnz);
    const std::vector<QuadraticTerm> terms = biyos::capi::packed(size, rows, columns, values);
    const std::span<const biyos::Index> owners(functions, size);
    Grouped<QuadraticTerm> groups =
        biyos::capi::grouped(owners, std::span<const QuadraticTerm>(terms));
    model.add_quadratic_terms(groups.functions, groups.starts, std::move(groups.terms));
}

void add_ranged(biyos::Session &session,
                Range range,
                const BIYOS_offset *counts,
                const BIYOS_index *rows,
                const BIYOS_index *columns,
                const BIYOS_real *values)
{
    biyos::Model &model = writable(session);
    add_sliced(model, selected(model, Family::Function, range), counts, rows, columns, values);
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
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *var1,
                                 const BIYOS_index *var2,
                                 const BIYOS_real *coef)
{
    return guarded<add_ranged>(mdl, Range{beg, end}, cnt, var1, var2, coef);
}
