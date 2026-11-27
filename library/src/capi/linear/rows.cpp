#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"
#include "model/utils/gather.hpp"

#include <cstddef>

using biyos::Family;
using biyos::Gathered;
using biyos::gathered;
using biyos::item_count;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Session;
using biyos::to_offset;
using biyos::capi::check_capacity;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::write_counts;
using biyos::capi::write_field;

namespace
{

template <typename Selection>
void n_row_terms(const Session &session, Selection selection, BIYOS_offset *nnz)
{
    const auto chosen =
        selected(Family::Function, session.model().n_of(Family::Function), selection);
    const std::size_t count = item_count(session.model(), &Model::linear_terms, chosen);
    *require(nnz, "nnz") = to_offset(count);
}

template <typename Selection>
void get_rows(const Session &session,
              Selection selection,
              BIYOS_offset capacity,
              BIYOS_offset *counts,
              BIYOS_index *variables,
              BIYOS_real *values)
{
    const auto chosen =
        selected(Family::Function, session.model().n_of(Family::Function), selection);
    const Gathered<LinearTerm> rows = gathered(session.model(), &Model::linear_terms, chosen);
    check_capacity(capacity, rows.items.size());
    write_counts(rows.starts, counts);
    write_field(rows.items, &LinearTerm::variable, variables);
    write_field(rows.items, &LinearTerm::coefficient, values);
}

}

BIYOS_status BIYOS_getnrlintermsp(const BIYOS_model *mdl,
                                  BIYOS_index n,
                                  const BIYOS_index *fun,
                                  BIYOS_offset *nnz)
{
    return guarded<n_row_terms<List>>(mdl, List{n, fun}, nnz);
}

BIYOS_status BIYOS_getnrlintermrg(const BIYOS_model *mdl,
                                  BIYOS_index beg,
                                  BIYOS_index end,
                                  BIYOS_offset *nnz)
{
    return guarded<n_row_terms<Range>>(mdl, Range{beg, end}, nnz);
}

BIYOS_status BIYOS_getrlintermsp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *fun,
                                 BIYOS_offset cap,
                                 BIYOS_offset *cnt,
                                 BIYOS_index *var,
                                 BIYOS_real *coef)
{
    return guarded<get_rows<List>>(mdl, List{n, fun}, cap, cnt, var, coef);
}

BIYOS_status BIYOS_getrlintermrg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 BIYOS_offset cap,
                                 BIYOS_offset *cnt,
                                 BIYOS_index *var,
                                 BIYOS_real *coef)
{
    return guarded<get_rows<Range>>(mdl, Range{beg, end}, cap, cnt, var, coef);
}
