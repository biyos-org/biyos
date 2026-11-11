#include "model/terms.hpp"

#include "core/index.hpp"
#include "expr/terms.hpp"
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

}
