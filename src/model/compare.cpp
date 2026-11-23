#include "model/compare.hpp"

#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/terms.hpp"
#include "model/fields.hpp"
#include "model/match.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

template <typename Count, typename Compare>
std::string table_difference(std::string_view noun, Count a, Count b, Compare compare)
{
    if (a != b)
    {
        return against(std::string(noun) + " count", a, b);
    }
    for (Count k = 0; k < a; ++k)
    {
        const std::string found = compare(k);
        if (!found.empty())
        {
            return std::string(noun) + " " + text_of(k) + ": " + found;
        }
    }
    return "";
}

int kind_of(const Variable &variable)
{
    return static_cast<int>(variable.kind);
}

std::string differ(const Variable &a, const Variable &b)
{
    return first_of({
        compared("kind", kind_of(a), kind_of(b)),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
    });
}

std::string differ(const LinearTerm &a, const LinearTerm &b)
{
    return first_of({
        compared("variable", a.variable, b.variable),
        compared("coefficient", a.coefficient, b.coefficient),
    });
}

std::string differ(const QuadraticTerm &a, const QuadraticTerm &b)
{
    return first_of({
        compared("first variable", a.row, b.row),
        compared("second variable", a.column, b.column),
        compared("coefficient", a.coefficient, b.coefficient),
    });
}

std::string differ(const Constraint &a, const Constraint &b)
{
    return first_of({
        compared("function", a.function, b.function),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
    });
}

int sense_of(const Objective &objective)
{
    return static_cast<int>(objective.sense);
}

std::string differ(const Objective &a, const Objective &b)
{
    return first_of({
        compared("function", a.function, b.function),
        compared("sense", sense_of(a), sense_of(b)),
        compared("priority", a.priority, b.priority),
        compared("weight", a.weight, b.weight),
        compared("absolute tolerance", a.absolute, b.absolute),
        compared("relative tolerance", a.relative, b.relative),
    });
}

std::string differ(const Indicator &a, const Indicator &b)
{
    return first_of({
        compared("variable", a.variable, b.variable),
        compared("value", a.value, b.value),
        compared("function", a.function, b.function),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
    });
}

std::string differ(const Member &a, const Member &b)
{
    return first_of({
        compared("variable", a.variable, b.variable),
        compared("weight", a.weight, b.weight),
    });
}

template <typename T> Offset size_of(std::span<const T> terms)
{
    return static_cast<Offset>(terms.size());
}

template <typename T> const T &at(std::span<const T> terms, Offset k)
{
    return terms[static_cast<std::size_t>(k)];
}

template <typename T>
std::string term_differences(std::string_view noun, std::span<const T> a, std::span<const T> b)
{
    const auto term = [&](Offset k) { return differ(at(a, k), at(b, k)); };
    return table_difference(noun, size_of(a), size_of(b), term);
}

double constant_of(const Model &model, Index function)
{
    return model.constants(function, 1).front();
}

std::span<const QuadraticTerm> quadratic_of(const Model &model, Index function)
{
    return model.quadratic_terms(function, 1);
}

Offset root_of(const Model &model, Index function)
{
    return model.roots(function, 1).front();
}

std::string root_text(Offset root)
{
    return root == kNoRoot ? "none" : "node " + text_of(root);
}

std::string expression_difference(Matching &matching, Offset a, Offset b)
{
    if (a == kNoRoot || b == kNoRoot)
    {
        return a == b ? "" : "root " + root_text(a) + " against " + root_text(b);
    }
    return matching.difference(a, b);
}

std::string named(const Model &a, const Model &b, Family family, Index index)
{
    return compared("name", a.name_of(family, index), b.name_of(family, index));
}

std::string variable_difference(const Model &a, const Model &b, Index variable)
{
    return first_of({
        differ(a.variable(variable), b.variable(variable)),
        named(a, b, Family::Variable, variable),
    });
}

std::string function_difference(const Model &a, const Model &b, Index function, Matching &matching)
{
    return first_of({
        compared("constant", constant_of(a, function), constant_of(b, function)),
        term_differences("term", a.linear_terms(function, 1), b.linear_terms(function, 1)),
        term_differences("quadratic term", quadratic_of(a, function), quadratic_of(b, function)),
        expression_difference(matching, root_of(a, function), root_of(b, function)),
        named(a, b, Family::Function, function),
    });
}

std::string constraint_difference(const Model &a, const Model &b, Index constraint)
{
    return first_of({
        differ(a.constraint(constraint), b.constraint(constraint)),
        named(a, b, Family::Constraint, constraint),
    });
}

std::string objective_difference(const Model &a, const Model &b, Index objective)
{
    return first_of({
        differ(a.objective(objective), b.objective(objective)),
        named(a, b, Family::Objective, objective),
    });
}

std::string indicator_difference(const Model &a, const Model &b, Index indicator)
{
    return first_of({
        differ(a.indicator(indicator), b.indicator(indicator)),
        named(a, b, Family::Indicator, indicator),
    });
}

int kind_of(const Model &model, Index sos)
{
    return static_cast<int>(model.kind_of_sos(sos));
}

std::string sos_difference(const Model &a, const Model &b, Index sos)
{
    return first_of({
        compared("kind", kind_of(a, sos), kind_of(b, sos)),
        term_differences("member", a.members(sos, 1), b.members(sos, 1)),
        named(a, b, Family::Sos, sos),
    });
}

std::string variable_differences(const Model &a, const Model &b)
{
    const auto variable = [&](Index k) { return variable_difference(a, b, k); };
    return table_difference("variable", a.n_variables(), b.n_variables(), variable);
}

std::string function_differences(const Model &a, const Model &b)
{
    Matching matching(a.graph(), b.graph());
    const auto function = [&](Index k) { return function_difference(a, b, k, matching); };
    return table_difference("function", a.n_functions(), b.n_functions(), function);
}

std::string constraint_differences(const Model &a, const Model &b)
{
    const auto constraint = [&](Index k) { return constraint_difference(a, b, k); };
    return table_difference("constraint", a.n_constraints(), b.n_constraints(), constraint);
}

std::string objective_differences(const Model &a, const Model &b)
{
    const auto objective = [&](Index k) { return objective_difference(a, b, k); };
    return table_difference("objective", a.n_objectives(), b.n_objectives(), objective);
}

std::string indicator_differences(const Model &a, const Model &b)
{
    const auto indicator = [&](Index k) { return indicator_difference(a, b, k); };
    return table_difference("indicator", a.n_indicators(), b.n_indicators(), indicator);
}

std::string sos_differences(const Model &a, const Model &b)
{
    const auto sos = [&](Index k) { return sos_difference(a, b, k); };
    return table_difference("set", a.n_sos(), b.n_sos(), sos);
}

}

std::string difference(const Model &a, const Model &b)
{
    std::string found = compared("model name", a.name(), b.name());
    if (found.empty())
    {
        found = variable_differences(a, b);
    }
    if (found.empty())
    {
        found = function_differences(a, b);
    }
    if (found.empty())
    {
        found = constraint_differences(a, b);
    }
    if (found.empty())
    {
        found = objective_differences(a, b);
    }
    if (found.empty())
    {
        found = indicator_differences(a, b);
    }
    if (found.empty())
    {
        found = sos_differences(a, b);
    }
    return found;
}

}
