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
using biyos::Model;
using biyos::QuadraticTerm;
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

struct Pairs
{
    BIYOS_index *rows;
    BIYOS_index *columns;
    BIYOS_real *values;
};

template <typename Selection>
void n_pairs(const Session &session, Selection selection, BIYOS_offset *nnz)
{
    const auto chosen =
        selected(Family::Function, session.model().n_of(Family::Function), selection);
    const std::size_t count = item_count(session.model(), &Model::quadratic_terms, chosen);
    *require(nnz, "nnz") = to_offset(count);
}

template <typename Selection>
void get_pairs(const Session &session,
               Selection selection,
               BIYOS_offset capacity,
               BIYOS_offset *counts,
               const Pairs &out)
{
    const auto chosen =
        selected(Family::Function, session.model().n_of(Family::Function), selection);
    const Gathered<QuadraticTerm> pairs =
        gathered(session.model(), &Model::quadratic_terms, chosen);
    check_capacity(capacity, pairs.items.size());
    write_counts(pairs.starts, counts);
    write_field(pairs.items, &QuadraticTerm::row, out.rows);
    write_field(pairs.items, &QuadraticTerm::column, out.columns);
    write_field(pairs.items, &QuadraticTerm::coefficient, out.values);
}

}

BIYOS_status BIYOS_getnquadtermsp(const BIYOS_model *mdl,
                                  BIYOS_index n,
                                  const BIYOS_index *idx,
                                  BIYOS_offset *nnz)
{
    return guarded<n_pairs<List>>(mdl, List{n, idx}, nnz);
}

BIYOS_status BIYOS_getnquadtermrg(const BIYOS_model *mdl,
                                  BIYOS_index beg,
                                  BIYOS_index end,
                                  BIYOS_offset *nnz)
{
    return guarded<n_pairs<Range>>(mdl, Range{beg, end}, nnz);
}

BIYOS_status BIYOS_getquadtermsp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 BIYOS_offset cap,
                                 BIYOS_offset *cnt,
                                 BIYOS_index *var1,
                                 BIYOS_index *var2,
                                 BIYOS_real *coef)
{
    const Pairs out{.rows = var1, .columns = var2, .values = coef};
    return guarded<get_pairs<List>>(mdl, List{n, idx}, cap, cnt, out);
}

BIYOS_status BIYOS_getquadtermrg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 BIYOS_offset cap,
                                 BIYOS_offset *cnt,
                                 BIYOS_index *var1,
                                 BIYOS_index *var2,
                                 BIYOS_real *coef)
{
    const Pairs out{.rows = var1, .columns = var2, .values = coef};
    return guarded<get_pairs<Range>>(mdl, Range{beg, end}, cap, cnt, out);
}
