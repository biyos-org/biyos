#include "biyos/biyos.h"

#include "capi/linear/grouped.hpp"
#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

using biyos::Family;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Session;
using biyos::to_size;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::starts_of;
using biyos::capi::writable;

namespace
{

void add_sliced(Model &model,
                std::span<const biyos::Index> owners,
                const BIYOS_offset *counts,
                const BIYOS_index *variables,
                const BIYOS_real *values)
{
    const std::vector<biyos::Offset> begins = starts_of(owners.size(), counts);
    model.check_room(0, begins.back());
    const auto total = to_size(begins.back());
    model.add_linear_terms(owners, begins, biyos::capi::packed(total, variables, values));
}

void add_grouped(Model &model,
                 std::span<const biyos::Index> functions,
                 const std::vector<LinearTerm> &terms)
{
    biyos::capi::Grouped<LinearTerm> groups =
        biyos::capi::grouped(functions, std::span<const LinearTerm>(terms));
    model.add_linear_terms(groups.functions, groups.starts, std::move(groups.terms));
}

std::vector<LinearTerm> column_terms(std::span<const biyos::Index> variables,
                                     std::span<const biyos::Offset> starts,
                                     const BIYOS_real *values)
{
    std::vector<LinearTerm> terms;
    terms.reserve(to_size(starts.back()));
    for (std::size_t j = 0; j < variables.size(); ++j)
    {
        for (auto t = starts[j]; t < starts[j + 1]; ++t)
        {
            const BIYOS_real value = values[to_size(t)];
            terms.push_back(LinearTerm{.variable = variables[j], .coefficient = value});
        }
    }
    return terms;
}

void add_listed(Session &session,
                BIYOS_offset nnz,
                const BIYOS_index *functions,
                const BIYOS_index *variables,
                const BIYOS_real *values)
{
    Model &model = writable(session);
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(functions, "fun");
    }
    model.check_room(0, nnz);
    const std::span<const biyos::Index> owners(functions, size);
    add_grouped(model, owners, biyos::capi::packed(size, variables, values));
}

void add_rows(Session &session,
              Range range,
              const BIYOS_offset *counts,
              const BIYOS_index *variables,
              const BIYOS_real *values)
{
    Model &model = writable(session);
    add_sliced(model, selected(model, Family::Function, range), counts, variables, values);
}

void add_columns(Session &session,
                 Range range,
                 const BIYOS_offset *counts,
                 const BIYOS_index *functions,
                 const BIYOS_real *values)
{
    Model &model = writable(session);
    const std::vector<biyos::Index> variables = selected(model, Family::Variable, range);
    const std::vector<biyos::Offset> begins = starts_of(variables.size(), counts);
    model.check_room(0, begins.back());
    const auto total = to_size(begins.back());
    if (total > 0)
    {
        require(functions, "fun");
        require(values, "coef");
    }
    const std::span<const biyos::Index> owners(functions, total);
    add_grouped(model, owners, column_terms(variables, begins, values));
}

}

BIYOS_status BIYOS_addlinterm(BIYOS_model *mdl,
                              BIYOS_offset nnz,
                              const BIYOS_index *fun,
                              const BIYOS_index *var,
                              const BIYOS_real *coef)
{
    return guarded<add_listed>(mdl, nnz, fun, var, coef);
}

BIYOS_status BIYOS_addrlintermrg(BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *var,
                                 const BIYOS_real *coef)
{
    return guarded<add_rows>(mdl, Range{beg, end}, cnt, var, coef);
}

BIYOS_status BIYOS_addclintermrg(BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *fun,
                                 const BIYOS_real *coef)
{
    return guarded<add_columns>(mdl, Range{beg, end}, cnt, fun, coef);
}
