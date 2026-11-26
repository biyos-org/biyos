#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/timing/timer.hpp"
#include "model/functions/transpose.hpp"
#include "model/model.hpp"

#include <cstddef>

using biyos::column_count;
using biyos::columns_of;
using biyos::ColumnTerms;
using biyos::EntryTimer;
using biyos::Family;
using biyos::kModule;
using biyos::Session;
using biyos::to_offset;
using biyos::capi::check_capacity;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::write_array;
using biyos::capi::write_counts;

namespace
{

template <typename Selection>
void n_column_terms(const Session &session, Selection selection, BIYOS_offset *nnz)
{
    const auto chosen = selected(session.model(), Family::Variable, selection);
    const EntryTimer timer(session.stream(), kModule, "columns");
    const std::size_t count = column_count(session.model(), chosen);
    *require(nnz, "nnz") = to_offset(count);
}

void get_listed(const Session &session,
                List list,
                BIYOS_offset capacity,
                BIYOS_offset *counts,
                BIYOS_index *functions,
                BIYOS_real *values)
{
    const auto chosen = selected(session.model(), Family::Variable, list);
    const EntryTimer timer(session.stream(), kModule, "columns");
    const ColumnTerms columns = columns_of(session.model(), chosen);
    check_capacity(capacity, columns.functions.size());
    write_counts(columns.starts, counts);
    write_array(columns.functions, functions);
    write_array(columns.values, values);
}

void get_ranged(const Session &session,
                Range range,
                BIYOS_offset capacity,
                BIYOS_offset *counts,
                BIYOS_index *functions,
                BIYOS_real *values)
{
    const EntryTimer timer(session.stream(), kModule, "columns");
    const ColumnTerms columns = columns_of(session.model(), range.start, range.stop);
    check_capacity(capacity, columns.functions.size());
    write_counts(columns.starts, counts);
    write_array(columns.functions, functions);
    write_array(columns.values, values);
}

}

BIYOS_status BIYOS_getnclintermsp(const BIYOS_model *mdl,
                                  BIYOS_index n,
                                  const BIYOS_index *idx,
                                  BIYOS_offset *nnz)
{
    return guarded<n_column_terms<List>>(mdl, List{n, idx}, nnz);
}

BIYOS_status BIYOS_getnclintermrg(const BIYOS_model *mdl,
                                  BIYOS_index beg,
                                  BIYOS_index end,
                                  BIYOS_offset *nnz)
{
    return guarded<n_column_terms<Range>>(mdl, Range{beg, end}, nnz);
}

BIYOS_status BIYOS_getclintermsp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 BIYOS_offset cap,
                                 BIYOS_offset *cnt,
                                 BIYOS_index *fun,
                                 BIYOS_real *coef)
{
    return guarded<get_listed>(mdl, List{n, idx}, cap, cnt, fun, coef);
}

BIYOS_status BIYOS_getclintermrg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 BIYOS_offset cap,
                                 BIYOS_offset *cnt,
                                 BIYOS_index *fun,
                                 BIYOS_real *coef)
{
    return guarded<get_ranged>(mdl, Range{beg, end}, cap, cnt, fun, coef);
}
