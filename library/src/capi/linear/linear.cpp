#include "biyos/biyos.h"

#include "capi/linear/grouped.hpp"
#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <concepts>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

using biyos::Family;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::Session;
using biyos::to_size;
using biyos::capi::check_known;
using biyos::capi::Grouped;
using biyos::capi::grouped;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::owners_of;
using biyos::capi::packed;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::starts_of;
using biyos::capi::writable;

namespace
{

void add_linear(Model &model, std::span<const Index> functions, std::span<const LinearTerm> terms)
{
    Grouped<LinearTerm> groups = grouped(functions, terms);
    model.add_linear_terms(groups.functions, groups.starts, std::move(groups.terms));
}

std::vector<LinearTerm> column_terms(std::span<const Index> variables,
                                     std::span<const Offset> starts,
                                     std::span<const Real> values)
{
    std::vector<LinearTerm> terms;
    terms.reserve(to_size(starts.back()));
    for (std::size_t j = 0; j < variables.size(); ++j)
    {
        for (auto t = starts[j]; t < starts[j + 1]; ++t)
        {
            const Real value = values[to_size(t)];
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
    const std::span<const Index> owners(functions, size);
    add_linear(model, owners, packed(size, variables, values));
}

template <typename Selection>
void add_rows(Session &session,
              Selection selection,
              const BIYOS_offset *counts,
              const BIYOS_index *variables,
              const BIYOS_real *values)
{
    Model &model = writable(session);
    const auto functions = selected(Family::Function, model.n_of(Family::Function), selection);
    if constexpr (std::same_as<Selection, List>)
    {
        check_known(Family::Function, model.n_of(Family::Function), functions);
    }
    const std::vector<Offset> begins = starts_of(functions.size(), counts);
    model.check_room(0, begins.back());
    std::vector<LinearTerm> terms = packed(to_size(begins.back()), variables, values);
    if constexpr (std::same_as<Selection, List>)
    {
        add_linear(model, owners_of(functions, begins), terms);
    }
    else
    {
        model.add_linear_terms(functions, begins, std::move(terms));
    }
}

template <typename Selection>
void add_columns(Session &session,
                 Selection selection,
                 const BIYOS_offset *counts,
                 const BIYOS_index *functions,
                 const BIYOS_real *values)
{
    Model &model = writable(session);
    const auto variables = selected(Family::Variable, model.n_of(Family::Variable), selection);
    if constexpr (std::same_as<Selection, List>)
    {
        check_known(Family::Variable, model.n_of(Family::Variable), variables);
    }
    const std::vector<Offset> begins = starts_of(variables.size(), counts);
    model.check_room(0, begins.back());
    const auto total = to_size(begins.back());
    if (total > 0)
    {
        require(functions, "fun");
        require(values, "coef");
    }
    const std::span<const Index> owners(functions, total);
    const std::span<const Real> coefficients(values, total);
    add_linear(model, owners, column_terms(variables, begins, coefficients));
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

BIYOS_status BIYOS_addrlintermsp(BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *var,
                                 const BIYOS_real *coef)
{
    return guarded<add_rows<List>>(mdl, List{n, idx}, cnt, var, coef);
}

BIYOS_status BIYOS_addrlintermrg(BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *var,
                                 const BIYOS_real *coef)
{
    return guarded<add_rows<Range>>(mdl, Range{beg, end}, cnt, var, coef);
}

BIYOS_status BIYOS_addclintermsp(BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *fun,
                                 const BIYOS_real *coef)
{
    return guarded<add_columns<List>>(mdl, List{n, idx}, cnt, fun, coef);
}

BIYOS_status BIYOS_addclintermrg(BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const BIYOS_offset *cnt,
                                 const BIYOS_index *fun,
                                 const BIYOS_real *coef)
{
    return guarded<add_columns<Range>>(mdl, Range{beg, end}, cnt, fun, coef);
}
