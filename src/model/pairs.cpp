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

void check_variable(Index function, Index variable, Index variables)
{
    if (variable < 0 || variable >= variables)
    {
        refuse(kOwner, function, item("variable", variable) + " is unknown");
    }
}

void check_coefficient(Index function, const QuadraticTerm &term)
{
    const std::string_view fault = value_fault(term.coefficient);
    if (!fault.empty())
    {
        refuse(kOwner, function, "coefficient of " + pair_name(term) + " " + std::string(fault));
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

void check_pairs(Index function, std::span<const QuadraticTerm> terms, Index variables)
{
    for (const QuadraticTerm &term : terms)
    {
        check_variable(function, term.first, variables);
        check_variable(function, term.second, variables);
        check_coefficient(function, term);
    }
}

void sort_pairs(Index function, std::span<QuadraticTerm> terms)
{
    std::ranges::for_each(terms, order_pair);
    std::ranges::sort(terms, {}, pair_of);
    const auto repeat = std::ranges::adjacent_find(terms, {}, pair_of);
    if (repeat != terms.end())
    {
        refuse(kOwner, function, pair_name(*repeat) + " is repeated");
    }
}

void check_sum(Index function, const QuadraticTerm &held, const QuadraticTerm &given)
{
    const std::string_view fault = value_fault(held.coefficient + given.coefficient);
    if (!fault.empty())
    {
        const std::string sum = "sum of coefficients of " + pair_name(given);
        refuse(kOwner, function, sum + " " + std::string(fault));
    }
}

}
