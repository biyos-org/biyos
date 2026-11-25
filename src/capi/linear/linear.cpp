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

void add_sliced(biyos::Model &core,
                std::span<const biyos::Index> owners,
                const BIYOS_offset *counts,
                const BIYOS_index *variables,
                const BIYOS_real *values)
{
    const std::vector<biyos::Offset> begins = starts_of(owners.size(), counts);
    core.check_room(0, begins.back());
    const auto total = to_size(begins.back());
    core.add_linear_terms(owners, begins, biyos::capi::packed(total, variables, values));
}

void add_grouped(biyos::Model &core,
                 std::span<const biyos::Index> functions,
                 const std::vector<LinearTerm> &terms)
{
    biyos::capi::Grouped<LinearTerm> groups =
        biyos::capi::grouped(functions, std::span<const LinearTerm>(terms));
    core.add_linear_terms(groups.functions, groups.starts, std::move(groups.terms));
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

void add_listed(BIYOS_model &session,
                BIYOS_offset nnz,
                const BIYOS_index *functions,
                const BIYOS_index *variables,
                const BIYOS_real *values)
{
    biyos::Model &core = writable(session);
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(functions, "fun");
    }
    core.check_room(0, nnz);
    const std::span<const biyos::Index> owners(functions, size);
    add_grouped(core, owners, biyos::capi::packed(size, variables, values));
}

void add_rows(BIYOS_model &session,
              Range range,
              const BIYOS_offset *counts,
              const BIYOS_index *variables,
              const BIYOS_real *values)
{
    biyos::Model &core = writable(session);
    add_sliced(core, selected(core, Family::Function, range), counts, variables, values);
}

void add_columns(BIYOS_model &session,
                 Range range,
                 const BIYOS_offset *counts,
                 const BIYOS_index *functions,
                 const BIYOS_real *values)
{
    biyos::Model &core = writable(session);
    const std::vector<biyos::Index> variables = selected(core, Family::Variable, range);
    const std::vector<biyos::Offset> begins = starts_of(variables.size(), counts);
    core.check_room(0, begins.back());
    const auto total = to_size(begins.back());
    if (total > 0)
    {
        require(functions, "fun");
        require(values, "coef");
    }
    const std::span<const biyos::Index> owners(functions, total);
    add_grouped(core, owners, column_terms(variables, begins, values));
}

}

BIYOS_status BIYOS_addlintermsp(BIYOS_model *mdl,
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
