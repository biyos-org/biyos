#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>

using biyos::Family;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;

namespace
{

struct Pairs
{
    BIYOS_index *functions;
    BIYOS_index *rows;
    BIYOS_index *columns;
    BIYOS_real *values;
};

std::size_t count_of(const biyos::Model &core, std::span<const biyos::Index> functions)
{
    std::size_t count = 0;
    for (const biyos::Index function : functions)
    {
        count += core.quadratic_terms(function, function + 1).size();
    }
    return count;
}

template <typename Selection>
void n_pairs(const BIYOS_model &model, Selection selection, BIYOS_offset *nnz)
{
    const auto chosen = selected(model.core, Family::Function, selection);
    const std::size_t count = count_of(model.core, chosen);
    *require(nnz, "nnz") = to_offset(count);
}

void put_pair(const Pairs &out,
              std::size_t k,
              biyos::Index function,
              const biyos::QuadraticTerm &term)
{
    if (out.functions != nullptr)
    {
        out.functions[k] = function;
    }
    if (out.rows != nullptr)
    {
        out.rows[k] = term.row;
    }
    if (out.columns != nullptr)
    {
        out.columns[k] = term.column;
    }
    if (out.values != nullptr)
    {
        out.values[k] = term.coefficient;
    }
}

void write_pairs(const biyos::Model &core,
                 std::span<const biyos::Index> functions,
                 const Pairs &out)
{
    std::size_t k = 0;
    for (const biyos::Index function : functions)
    {
        for (const biyos::QuadraticTerm &term : core.quadratic_terms(function, function + 1))
        {
            put_pair(out, k, function, term);
            ++k;
        }
    }
}

void get_listed(const BIYOS_model &model,
                List list,
                BIYOS_offset capacity,
                BIYOS_offset *nnz,
                const Pairs &out)
{
    const auto chosen = selected(model.core, Family::Function, list);
    const std::size_t count = count_of(model.core, chosen);
    check_capacity(capacity, count);
    write_pairs(model.core, chosen, out);
    if (nnz != nullptr)
    {
        *nnz = to_offset(count);
    }
}

void get_ranged(const BIYOS_model &model,
                Range range,
                BIYOS_offset capacity,
                BIYOS_offset *counts,
                const Pairs &out)
{
    const biyos::Model &core = model.core;
    const auto terms = core.quadratic_terms(range.start, range.stop);
    check_capacity(capacity, terms.size());
    const auto record = [&](biyos::Index j)
    { return core.quadratic_terms(range.start + j, range.start + j + 1); };
    biyos::capi::write_counts(to_size(range.stop - range.start), counts, record);
    biyos::capi::unpack(terms, terms.size(), out.rows, out.columns, out.values);
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
                                 BIYOS_offset *nnz,
                                 BIYOS_index *fun,
                                 BIYOS_index *var1,
                                 BIYOS_index *var2,
                                 BIYOS_real *coef)
{
    const Pairs out{.functions = fun, .rows = var1, .columns = var2, .values = coef};
    return guarded<get_listed>(mdl, List{n, idx}, cap, nnz, out);
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
    const Pairs out{.functions = nullptr, .rows = var1, .columns = var2, .values = coef};
    return guarded<get_ranged>(mdl, Range{beg, end}, cap, cnt, out);
}
