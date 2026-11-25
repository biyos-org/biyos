#include "model/functions/transpose.hpp"

#include "core/index.hpp"
#include "expr/forms/terms.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
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

template <typename T> void put(std::span<T> out, Offset at, T value)
{
    const auto k = static_cast<std::size_t>(at);
    if (k < out.size())
    {
        out[k] = value;
    }
}

}

std::vector<Offset> variable_starts(const Model &model, Index start, Index stop)
{
    check_span("variable", start, stop, model.n_variables());
    std::vector<Offset> starts(static_cast<std::size_t>(stop - start) + 1, 0);
    for (Index function = 0; function < model.n_functions(); ++function)
    {
        for (const LinearTerm &term :
             within(model.linear_terms(function, function + 1), start, stop))
        {
            ++starts[static_cast<std::size_t>(term.variable - start) + 1];
        }
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    return starts;
}

void variable_terms(const Model &model,
                    Index start,
                    std::span<const Offset> starts,
                    std::span<Index> functions,
                    std::span<double> values)
{
    if (functions.empty() && values.empty())
    {
        return;
    }
    std::vector<Offset> next(starts.begin(), starts.end() - 1);
    const auto end = start + static_cast<Index>(next.size());
    for (Index function = 0; function < model.n_functions(); ++function)
    {
        for (const LinearTerm &term :
             within(model.linear_terms(function, function + 1), start, end))
        {
            const Offset at = next[static_cast<std::size_t>(term.variable - start)]++;
            put(functions, at, function);
            put(values, at, term.coefficient);
        }
    }
}

}
