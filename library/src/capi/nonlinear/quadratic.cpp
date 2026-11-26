#include "biyos/biyos.h"

#include "capi/linear/grouped.hpp"
#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <concepts>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::QuadraticTerm;
using biyos::Session;
using biyos::to_size;
using biyos::capi::check_known;
using biyos::capi::grouped;
using biyos::capi::Grouped;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::owners_of;
using biyos::capi::packed;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::starts_of;
using biyos::capi::writable;

namespace
{

void add_quadratic(Model &model,
                   std::span<const Index> functions,
                   std::span<const QuadraticTerm> terms)
{
    Grouped<QuadraticTerm> groups = grouped(functions, terms);
    model.add_quadratic_terms(groups.functions, groups.starts, std::move(groups.terms));
}

void add_listed(Session &session,
                BIYOS_offset nnz,
                const BIYOS_index *functions,
                const BIYOS_index *rows,
                const BIYOS_index *columns,
                const BIYOS_real *values)
{
    Model &model = writable(session);
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(functions, "fun");
    }
    model.check_quadratic_room(nnz);
    const std::span<const Index> owners(functions, size);
    add_quadratic(model, owners, packed(size, rows, columns, values));
}

template <typename Selection>
void add_counted(Session &session,
                 Selection selection,
                 const BIYOS_offset *counts,
                 const BIYOS_index *rows,
                 const BIYOS_index *columns,
                 const BIYOS_real *values)
{
    Model &model = writable(session);
    const auto functions = selected(Family::Function, model.n_of(Family::Function), selection);
    if constexpr (std::same_as<Selection, List>)
    {
        check_known(Family::Function, model.n_of(Family::Function), functions);
    }
    const std::vector<Offset> begins = starts_of(functions.size(), counts);
    model.check_quadratic_room(begins.back());
    std::vector<QuadraticTerm> terms = packed(to_size(begins.back()), rows, columns, values);
    if constexpr (std::same_as<Selection, List>)
    {
        add_quadratic(model, owners_of(functions, begins), terms);
    }
    else
    {
        model.add_quadratic_terms(functions, begins, std::move(terms));
    }
}

}

BIYOS_status BIYOS_addquadterm(BIYOS_model *mdl,
                               BIYOS_offset nnz,
                               const BIYOS_index *fun,
                               const BIYOS_index *var1,
                               const BIYOS_index *var2,
                               const BIYOS_real *coef)
{
    return guarded<add_listed>(mdl, nnz, fun, var1, var2, coef);
}

BIYOS_status BIYOS_addquadtermsp(BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *var1,
                                 const BIYOS_index *var2,
                                 const BIYOS_real *coef)
{
    return guarded<add_counted<List>>(mdl, List{n, idx}, cnt, var1, var2, coef);
}

BIYOS_status BIYOS_addquadtermrg(BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *var1,
                                 const BIYOS_index *var2,
                                 const BIYOS_real *coef)
{
    return guarded<add_counted<Range>>(mdl, Range{beg, end}, cnt, var1, var2, coef);
}
