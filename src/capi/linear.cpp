#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/grouped.hpp"
#include "capi/selection.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

using biyos::Family;
using biyos::LinearTerm;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::offsets;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;

namespace
{

void add_sliced(biyos::Model &core,
                std::span<const biyos::Index> owners,
                const std::int64_t *starts,
                const int *variables,
                const double *values)
{
    const std::vector<biyos::Offset> begins = offsets(owners.size(), starts);
    biyos::check_starts(begins);
    core.check_room(0, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
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
                                     const double *values)
{
    std::vector<LinearTerm> terms;
    terms.reserve(static_cast<std::size_t>(starts.back()));
    for (std::size_t j = 0; j < variables.size(); ++j)
    {
        for (auto t = starts[j]; t < starts[j + 1]; ++t)
        {
            const double value = values[static_cast<std::size_t>(t)];
            terms.push_back(LinearTerm{.variable = variables[j], .coefficient = value});
        }
    }
    return terms;
}

void add_linear_terms(BIYOS_model &model,
                      int n,
                      const int *functions,
                      const std::int64_t *starts,
                      const int *variables,
                      const double *values)
{
    add_sliced(writable(model), listed(n, functions, "funidx"), starts, variables, values);
}

void add_listed(BIYOS_model &model,
                std::int64_t nnz,
                const int *functions,
                const int *variables,
                const double *values)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(functions, "funidx");
    }
    core.check_room(0, nnz);
    const std::span<const biyos::Index> owners(functions, size);
    add_grouped(core, owners, biyos::capi::packed(size, variables, values));
}

void add_rows(BIYOS_model &model,
              Range range,
              const std::int64_t *starts,
              const int *variables,
              const double *values)
{
    biyos::Model &core = writable(model);
    add_sliced(core, selected(core, Family::Function, range), starts, variables, values);
}

void add_columns(BIYOS_model &model,
                 Range range,
                 const std::int64_t *starts,
                 const int *functions,
                 const double *values)
{
    biyos::Model &core = writable(model);
    const std::vector<biyos::Index> variables = selected(core, Family::Variable, range);
    const std::vector<biyos::Offset> begins = offsets(variables.size(), starts);
    biyos::check_starts(begins);
    core.check_room(0, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    if (total > 0)
    {
        require(functions, "funidx");
        require(values, "coef");
    }
    const std::span<const biyos::Index> owners(functions, total);
    add_grouped(core, owners, column_terms(variables, begins, values));
}

}

BIYOS_status BIYOS_addlinterms(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *funidx,
                               const BIYOS_offset *off,
                               const BIYOS_index *varidx,
                               const BIYOS_real *coef)
{
    return guarded<add_linear_terms>(mdl, n, funidx, off, varidx, coef);
}

BIYOS_status BIYOS_addlintermsp(BIYOS_model *mdl,
                                BIYOS_offset nnz,
                                const BIYOS_index *funidx,
                                const BIYOS_index *varidx,
                                const BIYOS_real *coef)
{
    return guarded<add_listed>(mdl, nnz, funidx, varidx, coef);
}

BIYOS_status BIYOS_addrlintermrg(BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 const BIYOS_offset *off,
                                 const BIYOS_index *varidx,
                                 const BIYOS_real *coef)
{
    return guarded<add_rows>(mdl, Range{pos, n}, off, varidx, coef);
}

BIYOS_status BIYOS_addclintermrg(BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 const BIYOS_offset *off,
                                 const BIYOS_index *funidx,
                                 const BIYOS_real *coef)
{
    return guarded<add_columns>(mdl, Range{pos, n}, off, funidx, coef);
}
