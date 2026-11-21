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

std::string var_name(Index var)
{
    return item("variable", var);
}

void check_var(Index fun, Index var, Index vars)
{
    if (var < 0 || var >= vars)
    {
        refuse(kOwner, fun, var_name(var) + " is unknown");
    }
}

void check_coefficient(Index fun, const LinearTerm &term)
{
    const std::string_view fault = value_fault(term.coefficient);
    if (!fault.empty())
    {
        const std::string named = "coefficient of " + var_name(term.variable);
        refuse(kOwner, fun, named + " " + std::string(fault));
    }
}

}

void check_terms(Index fun, std::span<const LinearTerm> terms, Index vars)
{
    for (const LinearTerm &term : terms)
    {
        check_var(fun, term.variable, vars);
        check_coefficient(fun, term);
    }
}

void sort_terms(Index fun, std::span<LinearTerm> terms)
{
    std::ranges::sort(terms, {}, &LinearTerm::variable);
    const auto repeat = std::ranges::adjacent_find(terms, {}, &LinearTerm::variable);
    if (repeat != terms.end())
    {
        refuse(kOwner, fun, var_name(repeat->variable) + " is repeated");
    }
}

}
