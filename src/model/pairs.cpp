#include "model/pairs.hpp"

#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/terms.hpp"
#include "model/checks.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace biyos
{

namespace
{

constexpr std::string_view kOwner = "function";

void check_var(Index fun, Index var, Index vars)
{
    if (var < 0 || var >= vars)
    {
        refuse(kOwner, fun, item("variable", var) + " is unknown");
    }
}

void check_coefficient(Index fun, const QuadraticTerm &term)
{
    const std::string_view fault = value_fault(term.coefficient);
    if (!fault.empty())
    {
        refuse(kOwner, fun, "coefficient of " + pair_name(term) + " " + std::string(fault));
    }
}

void order_pair(QuadraticTerm &term) noexcept
{
    if (term.second < term.first)
    {
        std::swap(term.first, term.second);
    }
}

}

std::string pair_name(const QuadraticTerm &term)
{
    return "pair (" + text_of(term.first) + ", " + text_of(term.second) + ")";
}

void check_pairs(Index fun, std::span<const QuadraticTerm> terms, Index vars)
{
    for (const QuadraticTerm &term : terms)
    {
        check_var(fun, term.first, vars);
        check_var(fun, term.second, vars);
        check_coefficient(fun, term);
    }
}

void sort_pairs(Index fun, std::span<QuadraticTerm> terms)
{
    std::ranges::for_each(terms, order_pair);
    std::ranges::sort(terms, {}, pair_of);
    const auto repeat = std::ranges::adjacent_find(terms, {}, pair_of);
    if (repeat != terms.end())
    {
        refuse(kOwner, fun, pair_name(*repeat) + " is repeated");
    }
}

void check_sum(Index fun, const QuadraticTerm &held, const QuadraticTerm &given)
{
    const std::string_view fault = value_fault(held.coefficient + given.coefficient);
    if (!fault.empty())
    {
        const std::string sum = "sum of coefficients of " + pair_name(given);
        refuse(kOwner, fun, sum + " " + std::string(fault));
    }
}

}
