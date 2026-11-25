#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "core/timing/timer.hpp"
#include "model/checks.hpp"
#include "model/functions/transpose.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

using biyos::Family;
using biyos::Session;
using biyos::to_index;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::Coordinates;
using biyos::capi::given;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;

namespace
{

struct Columns
{
    std::vector<biyos::Offset> starts;
    std::vector<biyos::Index> functions;
    std::vector<biyos::Real> values;
};

std::vector<biyos::Offset> every_start(const biyos::Model &model)
{
    return biyos::variable_starts(model, 0, model.n_variables());
}

std::size_t count_of(std::span<const biyos::Offset> starts, std::span<const biyos::Index> variables)
{
    std::size_t count = 0;
    for (const biyos::Index variable : variables)
    {
        biyos::check_index("variable", variable, to_index(starts.size() - 1));
        const auto k = to_size(variable);
        count += to_size(starts[k + 1] - starts[k]);
    }
    return count;
}

Columns columns_of(const biyos::Model &model, std::vector<biyos::Offset> starts)
{
    const auto total = to_size(starts.back());
    Columns columns{
        .starts = std::move(starts),
        .functions = std::vector<biyos::Index>(total),
        .values = std::vector<biyos::Real>(total),
    };
    biyos::variable_terms(model, 0, columns.starts, columns.functions, columns.values);
    return columns;
}

void write_columns(const Columns &columns,
                   std::span<const biyos::Index> variables,
                   const Coordinates &out)
{
    std::size_t k = 0;
    for (const biyos::Index variable : variables)
    {
        const auto j = to_size(variable);
        for (auto at = columns.starts[j]; at < columns.starts[j + 1]; ++at)
        {
            const auto t = to_size(at);
            const biyos::LinearTerm term{.variable = variable, .coefficient = columns.values[t]};
            biyos::capi::put_term(out, k, columns.functions[t], term);
            ++k;
        }
    }
}

template <typename Selection>
void n_column_terms(const Session &session, Selection selection, BIYOS_offset *nnz)
{
    const auto chosen = selected(session.model(), Family::Variable, selection);
    const biyos::EntryTimer timer(session.env(), biyos::kModule, "columns");
    const std::size_t count = count_of(every_start(session.model()), chosen);
    *require(nnz, "nnz") = to_offset(count);
}

void get_listed(const Session &session,
                List list,
                BIYOS_offset capacity,
                BIYOS_offset *nnz,
                const Coordinates &out)
{
    const auto chosen = selected(session.model(), Family::Variable, list);
    const biyos::EntryTimer timer(session.env(), biyos::kModule, "columns");
    std::vector<biyos::Offset> starts = every_start(session.model());
    const std::size_t count = count_of(starts, chosen);
    check_capacity(capacity, count);
    write_columns(columns_of(session.model(), std::move(starts)), chosen, out);
    if (nnz != nullptr)
    {
        *nnz = to_offset(count);
    }
}

void get_ranged(const Session &session,
                Range range,
                BIYOS_offset capacity,
                BIYOS_offset *counts,
                BIYOS_index *functions,
                BIYOS_real *values)
{
    const biyos::EntryTimer timer(session.env(), biyos::kModule, "columns");
    const std::vector<biyos::Offset> found =
        biyos::variable_starts(session.model(), range.start, range.stop);
    const auto count = to_size(found.back());
    check_capacity(capacity, count);
    biyos::variable_terms(session.model(),
                          range.start,
                          found,
                          given(functions, count),
                          given(values, count));
    biyos::capi::write_counts(found, counts);
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
                                 BIYOS_offset *nnz,
                                 BIYOS_index *fun,
                                 BIYOS_index *var,
                                 BIYOS_real *coef)
{
    const Coordinates out{.functions = fun, .variables = var, .values = coef};
    return guarded<get_listed>(mdl, List{n, idx}, cap, nnz, out);
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
