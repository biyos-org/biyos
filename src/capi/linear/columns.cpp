#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/model/selection.hpp"
#include "core/index.hpp"
#include "core/timing/timer.hpp"
#include "model/checks.hpp"
#include "model/functions/transpose.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

using biyos::Family;
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
    std::vector<double> values;
};

std::vector<biyos::Offset> every_start(const biyos::Model &core)
{
    return biyos::variable_starts(core, 0, core.n_variables());
}

std::size_t count_of(std::span<const biyos::Offset> starts, std::span<const biyos::Index> variables)
{
    std::size_t count = 0;
    for (const biyos::Index variable : variables)
    {
        biyos::check_index("variable", variable, static_cast<biyos::Index>(starts.size() - 1));
        const auto k = static_cast<std::size_t>(variable);
        count += static_cast<std::size_t>(starts[k + 1] - starts[k]);
    }
    return count;
}

Columns columns_of(const biyos::Model &core, std::vector<biyos::Offset> starts)
{
    const auto total = static_cast<std::size_t>(starts.back());
    Columns columns{
        .starts = std::move(starts),
        .functions = std::vector<biyos::Index>(total),
        .values = std::vector<double>(total),
    };
    biyos::variable_terms(core, 0, columns.starts, columns.functions, columns.values);
    return columns;
}

void write_columns(const Columns &columns,
                   std::span<const biyos::Index> variables,
                   const Coordinates &out)
{
    std::size_t k = 0;
    for (const biyos::Index variable : variables)
    {
        const auto j = static_cast<std::size_t>(variable);
        for (auto at = columns.starts[j]; at < columns.starts[j + 1]; ++at)
        {
            const auto t = static_cast<std::size_t>(at);
            const biyos::LinearTerm term{.variable = variable, .coefficient = columns.values[t]};
            biyos::capi::put_term(out, k, columns.functions[t], term);
            ++k;
        }
    }
}

template <typename Selection>
void n_column_terms(const BIYOS_model &model, Selection selection, std::int64_t *nnz)
{
    const auto chosen = selected(model.core, Family::Variable, selection);
    const biyos::EntryTimer timer(model.handle.env(), "model", "columns");
    const std::size_t count = count_of(every_start(model.core), chosen);
    *require(nnz, "nnz") = static_cast<std::int64_t>(count);
}

void get_listed(const BIYOS_model &model,
                List list,
                std::int64_t capacity,
                std::int64_t *nnz,
                const Coordinates &out)
{
    const auto chosen = selected(model.core, Family::Variable, list);
    const biyos::EntryTimer timer(model.handle.env(), "model", "columns");
    std::vector<biyos::Offset> starts = every_start(model.core);
    const std::size_t count = count_of(starts, chosen);
    check_capacity(capacity, count);
    write_columns(columns_of(model.core, std::move(starts)), chosen, out);
    if (nnz != nullptr)
    {
        *nnz = static_cast<std::int64_t>(count);
    }
}

void get_ranged(const BIYOS_model &model,
                Range range,
                std::int64_t capacity,
                std::int64_t *starts,
                int *functions,
                double *values)
{
    const biyos::EntryTimer timer(model.handle.env(), "model", "columns");
    const std::vector<biyos::Offset> found =
        biyos::variable_starts(model.core, range.start, range.stop);
    const auto count = static_cast<std::size_t>(found.back());
    check_capacity(capacity, count);
    biyos::variable_terms(model.core,
                          range.start,
                          found,
                          given(functions, count),
                          given(values, count));
    if (starts != nullptr)
    {
        std::ranges::copy(found, starts);
    }
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
                                 BIYOS_offset *off,
                                 BIYOS_index *fun,
                                 BIYOS_real *coef)
{
    return guarded<get_ranged>(mdl, Range{beg, end}, cap, off, fun, coef);
}
