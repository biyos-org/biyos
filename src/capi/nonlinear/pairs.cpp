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
using biyos::Model;
using biyos::QuadraticTerm;
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

struct Pairs
{
    BIYOS_index *rows;
    BIYOS_index *columns;
    BIYOS_real *values;
};

std::span<const QuadraticTerm> quadratic_terms_of(const Model &model, Index function)
{
    return model.quadratic_terms(function, function + 1);
}

std::size_t count_of(const Model &model, std::span<const Index> functions)
{
    std::size_t count = 0;
    for (const Index function : functions)
    {
        count += quadratic_terms_of(model, function).size();
    }
    return count;
}

template <typename Selection>
void n_pairs(const Session &session, Selection selection, BIYOS_offset *nnz)
{
    const auto chosen = selected(session.model(), Family::Function, selection);
    const std::size_t count = count_of(session.model(), chosen);
    *require(nnz, "nnz") = to_offset(count);
}

void put_pair(const Pairs &out, std::size_t k, const QuadraticTerm &term)
{
    put(out.rows, k, term.row);
    put(out.columns, k, term.column);
    put(out.values, k, term.coefficient);
}

void write_pairs(const Model &model, std::span<const Index> functions, const Pairs &out)
{
    std::size_t k = 0;
    for (const Index function : functions)
    {
        for (const QuadraticTerm &term : quadratic_terms_of(model, function))
        {
            put_pair(out, k, term);
            ++k;
        }
    }
}

template <typename Selection>
void get_pairs(const Session &session,
               Selection selection,
               BIYOS_offset capacity,
               BIYOS_offset *counts,
               const Pairs &out)
{
    const Model &model = session.model();
    const auto chosen = selected(model, Family::Function, selection);
    check_capacity(capacity, count_of(model, chosen));
    const auto record = [&](Index j) { return quadratic_terms_of(model, chosen[to_size(j)]); };
    write_counts(chosen.size(), counts, record);
    write_pairs(model, chosen, out);
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
