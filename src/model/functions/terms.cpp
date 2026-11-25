#include "model/functions/terms.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/checks.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr std::string_view kOwner = "function";

std::string variable_name(Index variable)
{
    return item("variable", variable);
}

void check_variable(Index function, Index variable, Index variables)
{
    if (variable < 0 || variable >= variables)
    {
        refuse(kOwner, function, variable_name(variable) + " is unknown");
    }
}

void check_coefficient(Index function, const LinearTerm &term)
{
    const std::string_view fault = value_fault(term.coefficient);
    if (!fault.empty())
    {
        const std::string named = "coefficient of " + variable_name(term.variable);
        refuse(kOwner, function, named + " " + std::string(fault));
    }
}

}

void check_terms(Index function, std::span<const LinearTerm> terms, Index variables)
{
    for (const LinearTerm &term : terms)
    {
        check_variable(function, term.variable, variables);
        check_coefficient(function, term);
    }
}

void sort_terms(Index function, std::span<LinearTerm> terms)
{
    std::ranges::sort(terms, {}, &LinearTerm::variable);
    const auto repeat = std::ranges::adjacent_find(terms, {}, &LinearTerm::variable);
    if (repeat != terms.end())
    {
        refuse(kOwner, function, variable_name(repeat->variable) + " is repeated");
    }
}

void check_sum(Index function, const LinearTerm &held, const LinearTerm &given)
{
    const std::string_view fault = value_fault(held.coefficient + given.coefficient);
    if (!fault.empty())
    {
        const std::string sum = "sum of coefficients of " + variable_name(given.variable);
        refuse(kOwner, function, sum + " " + std::string(fault));
    }
}

}
