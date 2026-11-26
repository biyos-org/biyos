#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>

using biyos::Family;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Session;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::Coordinates;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::put_term;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::unpack;
using biyos::capi::write_counts;

namespace
{

std::size_t count_of(const Model &model, std::span<const Index> functions)
{
    std::size_t count = 0;
    for (const Index function : functions)
    {
        count += model.linear_terms(function, function + 1).size();
    }
    return count;
}

template <typename Selection>
void n_row_terms(const Session &session, Selection selection, BIYOS_offset *nnz)
{
    const auto chosen = selected(session.model(), Family::Function, selection);
    const std::size_t count = count_of(session.model(), chosen);
    *require(nnz, "nnz") = to_offset(count);
}

void write_rows(const Model &model, std::span<const Index> functions, const Coordinates &out)
{
    std::size_t k = 0;
    for (const Index function : functions)
    {
        for (const LinearTerm &term : model.linear_terms(function, function + 1))
        {
            put_term(out, k, function, term);
            ++k;
        }
    }
}

void get_listed(const Session &session,
                List list,
                BIYOS_offset capacity,
                BIYOS_offset *nnz,
                const Coordinates &out)
{
    const auto chosen = selected(session.model(), Family::Function, list);
    const std::size_t count = count_of(session.model(), chosen);
    check_capacity(capacity, count);
    write_rows(session.model(), chosen, out);
    if (nnz != nullptr)
    {
        *nnz = to_offset(count);
    }
}

void get_ranged(const Session &session,
                Range range,
                BIYOS_offset capacity,
                BIYOS_offset *counts,
                BIYOS_index *variables,
                BIYOS_real *values)
{
    const Model &model = session.model();
    const std::span<const LinearTerm> terms = model.linear_terms(range.start, range.stop);
    check_capacity(capacity, terms.size());
    const auto record = [&](Index j)
    { return model.linear_terms(range.start + j, range.start + j + 1); };
    write_counts(to_size(range.stop - range.start), counts, record);
    unpack(terms, terms.size(), variables, values);
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
                                  BIYOS_index beg,
                                  BIYOS_index end,
                                  BIYOS_offset *nnz)
{
    return guarded<n_row_terms<Range>>(mdl, Range{beg, end}, nnz);
}

BIYOS_status BIYOS_getrlintermsp(const BIYOS_model *mdl,
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

BIYOS_status BIYOS_getrlintermrg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 BIYOS_offset cap,
                                 BIYOS_offset *cnt,
                                 BIYOS_index *var,
                                 BIYOS_real *coef)
{
    return guarded<get_ranged>(mdl, Range{beg, end}, cap, cnt, var, coef);
}
