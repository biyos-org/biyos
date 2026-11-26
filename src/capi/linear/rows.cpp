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
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::put;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::write_counts;

namespace
{

std::span<const LinearTerm> linear_terms_of(const Model &model, Index function)
{
    return model.linear_terms(function, function + 1);
}

std::size_t count_of(const Model &model, std::span<const Index> functions)
{
    std::size_t count = 0;
    for (const Index function : functions)
    {
        count += linear_terms_of(model, function).size();
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

void write_rows(const Model &model,
                std::span<const Index> functions,
                BIYOS_index *variables,
                BIYOS_real *values)
{
    std::size_t k = 0;
    for (const Index function : functions)
    {
        for (const LinearTerm &term : linear_terms_of(model, function))
        {
            put(variables, k, term.variable);
            put(values, k, term.coefficient);
            ++k;
        }
    }
}

template <typename Selection>
void get_rows(const Session &session,
              Selection selection,
              BIYOS_offset capacity,
              BIYOS_offset *counts,
              BIYOS_index *variables,
              BIYOS_real *values)
{
    const Model &model = session.model();
    const auto chosen = selected(model, Family::Function, selection);
    check_capacity(capacity, count_of(model, chosen));
    const auto record = [&](Index j) { return linear_terms_of(model, chosen[to_size(j)]); };
    write_counts(chosen.size(), counts, record);
    write_rows(model, chosen, variables, values);
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
                                 BIYOS_offset *cnt,
                                 BIYOS_index *var,
                                 BIYOS_real *coef)
{
    return guarded<get_rows<List>>(mdl, List{n, idx}, cap, cnt, var, coef);
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
