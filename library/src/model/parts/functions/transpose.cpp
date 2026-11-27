#include "model/parts/functions/transpose.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

std::span<const LinearTerm> within(std::span<const LinearTerm> terms, Index start, Index stop)
{
    const auto from = std::ranges::lower_bound(terms, start, {}, &LinearTerm::variable);
    const auto upto = std::ranges::lower_bound(from, terms.end(), stop, {}, &LinearTerm::variable);
    return {from, upto};
}

auto column_of(std::span<const Offset> starts, Index variable)
{
    const auto j = to_size(variable);
    return std::views::iota(to_size(starts[j]), to_size(starts[j + 1]));
}

template <typename T> void put(std::span<T> out, Offset at, T value)
{
    const auto k = to_size(at);
    if (k < out.size())
    {
        out[k] = value;
    }
}

}

std::vector<Offset> variable_starts(const Model &model, Index start, Index stop)
{
    check_span("variable", start, stop, model.n_variables());
    std::vector<Offset> starts(to_size(stop - start) + 1, 0);
    for (Index function = 0; function < model.n_functions(); ++function)
    {
        for (const LinearTerm &term :
             within(model.linear_terms(function, function + 1), start, stop))
        {
            ++starts[to_size(term.variable - start) + 1];
        }
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    return starts;
}

void variable_terms(const Model &model,
                    Index start,
                    std::span<const Offset> starts,
                    std::span<Index> functions,
                    std::span<Real> values)
{
    if (functions.empty() && values.empty())
    {
        return;
    }
    std::vector<Offset> next(starts.begin(), starts.end() - 1);
    const auto end = start + to_index(next.size());
    for (Index function = 0; function < model.n_functions(); ++function)
    {
        for (const LinearTerm &term :
             within(model.linear_terms(function, function + 1), start, end))
        {
            const Offset at = next[to_size(term.variable - start)]++;
            put(functions, at, function);
            put(values, at, term.coefficient);
        }
    }
}

std::size_t column_count(const Model &model, std::span<const Index> variables)
{
    const std::vector<Offset> starts = variable_starts(model, 0, model.n_variables());
    std::size_t count = 0;
    for (const Index variable : variables)
    {
        check_index("variable", variable, model.n_variables());
        count += column_of(starts, variable).size();
    }
    return count;
}

ColumnTerms columns_of(const Model &model, std::span<const Index> variables)
{
    const std::vector<Offset> starts = variable_starts(model, 0, model.n_variables());
    const auto total = to_size(starts.back());
    std::vector<Index> functions(total);
    std::vector<Real> values(total);
    variable_terms(model, 0, starts, functions, values);
    ColumnTerms columns{.starts = {0}, .functions = {}, .values = {}};
    for (const Index variable : variables)
    {
        check_index("variable", variable, model.n_variables());
        for (const std::size_t t : column_of(starts, variable))
        {
            columns.functions.push_back(functions[t]);
            columns.values.push_back(values[t]);
        }
        columns.starts.push_back(to_offset(columns.functions.size()));
    }
    return columns;
}

ColumnTerms columns_of(const Model &model, Index start, Index stop)
{
    ColumnTerms columns{
        .starts = variable_starts(model, start, stop),
        .functions = {},
        .values = {},
    };
    const auto total = to_size(columns.starts.back());
    columns.functions.resize(total);
    columns.values.resize(total);
    variable_terms(model, start, columns.starts, columns.functions, columns.values);
    return columns;
}

}
