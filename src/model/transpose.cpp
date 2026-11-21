#include "model/transpose.hpp"

#include "core/index.hpp"
#include "expr/terms.hpp"
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

std::span<const LinearTerm> within(std::span<const LinearTerm> terms, Index beg, Index end)
{
    const auto from = std::ranges::lower_bound(terms, beg, {}, &LinearTerm::variable);
    const auto upto = std::ranges::lower_bound(from, terms.end(), end, {}, &LinearTerm::variable);
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

std::vector<Offset> var_starts(const Model &mdl, Index beg, Index n)
{
    check_span("variable", beg, n, mdl.n_vars());
    std::vector<Offset> starts(static_cast<std::size_t>(n) + 1, 0);
    for (Index fun = 0; fun < mdl.n_funs(); ++fun)
    {
        for (const LinearTerm &term : within(mdl.terms(fun, 1), beg, beg + n))
        {
            ++starts[static_cast<std::size_t>(term.variable - beg) + 1];
        }
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    return starts;
}

void var_terms(const Model &mdl,
               Index beg,
               std::span<const Offset> starts,
               std::span<Index> funs,
               std::span<double> values)
{
    if (funs.empty() && values.empty())
    {
        return;
    }
    std::vector<Offset> next(starts.begin(), starts.end() - 1);
    const auto end = beg + static_cast<Index>(next.size());
    for (Index fun = 0; fun < mdl.n_funs(); ++fun)
    {
        for (const LinearTerm &term : within(mdl.terms(fun, 1), beg, end))
        {
            const Offset at = next[static_cast<std::size_t>(term.variable - beg)]++;
            put(funs, at, fun);
            put(values, at, term.coefficient);
        }
    }
}

}
