#include "expr/terms.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

bool linear_order(const LinearTerm &a, const LinearTerm &b) noexcept
{
    return a.variable < b.variable;
}

bool pair_order(const QuadraticTerm &a, const QuadraticTerm &b) noexcept
{
    return a.first < b.first || (a.first == b.first && a.second < b.second);
}

void order_pair(QuadraticTerm &term) noexcept
{
    if (term.first > term.second)
    {
        std::swap(term.first, term.second);
    }
}

template <typename Term, typename Order> void add_by(std::vector<Term> &terms, Order order)
{
    std::stable_sort(terms.begin(), terms.end(), order);
    std::size_t count = 0;
    for (const Term &term : terms)
    {
        if (count > 0 && !order(terms[count - 1], term))
        {
            terms[count - 1].coefficient += term.coefficient;
            continue;
        }
        terms[count++] = term;
    }
    terms.resize(count);
}

}

void add_alike(std::vector<LinearTerm> &terms)
{
    add_by(terms, linear_order);
}

void add_alike(std::vector<QuadraticTerm> &terms)
{
    for (QuadraticTerm &term : terms)
    {
        order_pair(term);
    }
    add_by(terms, pair_order);
}

}
