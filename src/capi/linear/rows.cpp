#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/model/selection.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

using biyos::Family;
using biyos::capi::check_capacity;
using biyos::capi::Coordinates;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;

namespace
{

std::size_t count_of(const biyos::Model &core, std::span<const biyos::Index> functions)
{
    std::size_t count = 0;
    for (const biyos::Index function : functions)
    {
        count += core.linear_terms(function, 1).size();
    }
    return count;
}

template <typename Selection>
void n_row_terms(const BIYOS_model &model, Selection selection, std::int64_t *nnz)
{
    const auto chosen = selected(model.core, Family::Function, selection);
    const std::size_t count = count_of(model.core, chosen);
    *require(nnz, "nnz") = static_cast<std::int64_t>(count);
}

void write_rows(const biyos::Model &core,
                std::span<const biyos::Index> functions,
                const Coordinates &out)
{
    std::size_t k = 0;
    for (const biyos::Index function : functions)
    {
        for (const biyos::LinearTerm &term : core.linear_terms(function, 1))
        {
            biyos::capi::put_term(out, k, function, term);
            ++k;
        }
    }
}

void get_listed(const BIYOS_model &model,
                List list,
                std::int64_t capacity,
                std::int64_t *nnz,
                const Coordinates &out)
{
    const auto chosen = selected(model.core, Family::Function, list);
    const std::size_t count = count_of(model.core, chosen);
    check_capacity(capacity, count);
    write_rows(model.core, chosen, out);
    if (nnz != nullptr)
    {
        *nnz = static_cast<std::int64_t>(count);
    }
}

void get_ranged(const BIYOS_model &model,
                Range range,
                std::int64_t capacity,
                std::int64_t *starts,
                int *variables,
                double *values)
{
    const biyos::Model &core = model.core;
    const std::span<const biyos::LinearTerm> terms = core.linear_terms(range.start, range.size);
    check_capacity(capacity, terms.size());
    const auto before = [&](biyos::Index j) { return core.linear_terms(range.start, j); };
    biyos::capi::write_starts(static_cast<std::size_t>(range.size), starts, before);
    biyos::capi::unpack(terms, terms.size(), variables, values);
}

}

BIYOS_status BIYOS_getnrlintermsp(const BIYOS_model *mdl,
                                  BIYOS_index n,
                                  const BIYOS_index *idx,
                                  BIYOS_offset *nnz)
{
    return guarded<n_row_terms<List>>(mdl, List{n, idx}, nnz);
}

BIYOS_status BIYOS_getnrlintermrg(const BIYOS_model *mdl,
                                  BIYOS_index pos,
                                  BIYOS_index n,
                                  BIYOS_offset *nnz)
{
    return guarded<n_row_terms<Range>>(mdl, Range{pos, n}, nnz);
}

BIYOS_status BIYOS_getrlintermsp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 BIYOS_offset cap,
                                 BIYOS_offset *nnz,
                                 BIYOS_index *funidx,
                                 BIYOS_index *varidx,
                                 BIYOS_real *coef)
{
    const Coordinates out{.functions = funidx, .variables = varidx, .values = coef};
    return guarded<get_listed>(mdl, List{n, idx}, cap, nnz, out);
}

BIYOS_status BIYOS_getrlintermrg(const BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 BIYOS_offset cap,
                                 BIYOS_offset *off,
                                 BIYOS_index *varidx,
                                 BIYOS_real *coef)
{
    return guarded<get_ranged>(mdl, Range{pos, n}, cap, off, varidx, coef);
}
