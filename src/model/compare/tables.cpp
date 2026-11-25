#include "model/compare/compare.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/points.hpp"
#include "model/attributes/records.hpp"
#include "model/compare/fields.hpp"
#include "model/compare/match.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

double constant_of(const Model &model, Index function)
{
    return model.constants(function, function + 1).front();
}

std::span<const QuadraticTerm> quadratic_of(const Model &model, Index function)
{
    return model.quadratic_terms(function, function + 1);
}

Offset root_of(const Model &model, Index function)
{
    return model.roots(function, function + 1).front();
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
        differ(a.tolerance_of(Family::Variable, variable),
               b.tolerance_of(Family::Variable, variable)),
        named(a, b, Family::Variable, variable),
    });
}

std::string function_difference(const Model &a, const Model &b, Index function, Matching &matching)
{
    return first_of({
        compared("constant", constant_of(a, function), constant_of(b, function)),
        term_differences("term",
                         a.linear_terms(function, function + 1),
                         b.linear_terms(function, function + 1)),
        term_differences("quadratic term", quadratic_of(a, function), quadratic_of(b, function)),
        expression_difference(matching, root_of(a, function), root_of(b, function)),
        named(a, b, Family::Function, function),
    });
}

std::string constraint_difference(const Model &a, const Model &b, Index constraint)
{
    return first_of({
        differ(a.constraint(constraint), b.constraint(constraint)),
        differ(a.tolerance_of(Family::Constraint, constraint),
               b.tolerance_of(Family::Constraint, constraint)),
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
    return static_cast<int>(model.kind_of_special_ordered_set(sos));
}

std::string special_ordered_set_difference(const Model &a, const Model &b, Index sos)
{
    return first_of({
        compared("kind", kind_of(a, sos), kind_of(b, sos)),
        term_differences("member", a.members(sos, sos + 1), b.members(sos, sos + 1)),
        named(a, b, Family::SpecialOrderedSet, sos),
    });
}

int cone_kind(const Model &model, Index cone)
{
    return static_cast<int>(model.cone_kinds(cone, cone + 1).front());
}

std::string cone_difference(const Model &a, const Model &b, Index cone)
{
    return first_of({
        compared("kind", cone_kind(a, cone), cone_kind(b, cone)),
        term_differences("member",
                         a.cone_functions(cone, cone + 1),
                         b.cone_functions(cone, cone + 1)),
        term_differences("parameter",
                         a.cone_parameters(cone, cone + 1),
                         b.cone_parameters(cone, cone + 1)),
        differ(a.tolerance_of(Family::Cone, cone), b.tolerance_of(Family::Cone, cone)),
        named(a, b, Family::Cone, cone),
    });
}

std::string complement_difference(const Model &a, const Model &b, Index complement)
{
    return first_of({
        differ(a.complement(complement), b.complement(complement)),
        differ(a.tolerance_of(Family::Complement, complement),
               b.tolerance_of(Family::Complement, complement)),
        named(a, b, Family::Complement, complement),
    });
}

std::string variable_differences(const Model &a, const Model &b)
{
    const auto variable = [&](Index k) { return variable_difference(a, b, k); };
    return table_difference("variable", a.n_variables(), b.n_variables(), variable);
}

std::string point_difference(const PiecewiseLinear &a, const PiecewiseLinear &b, Offset k)
{
    return first_of({
        compared("x", at(a.x, k), at(b.x, k)),
        compared("y", at(a.y, k), at(b.y, k)),
    });
}

std::string piecewise_linear_difference(const Points &a, const Points &b, Offset pwl)
{
    const PiecewiseLinear x = a.piecewise_linear(pwl);
    const PiecewiseLinear y = b.piecewise_linear(pwl);
    const auto point = [&](Offset k) { return point_difference(x, y, k); };
    return table_difference("point", size_of(x.x), size_of(y.x), point);
}

std::string piecewise_linear_differences(const Model &a, const Model &b)
{
    const Points &x = a.graph().points();
    const Points &y = b.graph().points();
    const auto pwl = [&](Offset k) { return piecewise_linear_difference(x, y, k); };
    return table_difference("pwl", x.size(), y.size(), pwl);
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

std::string special_ordered_set_differences(const Model &a, const Model &b)
{
    const auto sos = [&](Index k) { return special_ordered_set_difference(a, b, k); };
    return table_difference("set", a.n_special_ordered_sets(), b.n_special_ordered_sets(), sos);
}

std::string cone_differences(const Model &a, const Model &b)
{
    const auto cone = [&](Index k) { return cone_difference(a, b, k); };
    return table_difference("cone", a.n_cones(), b.n_cones(), cone);
}

std::string complement_differences(const Model &a, const Model &b)
{
    const auto complement = [&](Index k) { return complement_difference(a, b, k); };
    return table_difference("complement", a.n_complements(), b.n_complements(), complement);
}

std::string callback_difference(const Model &a, const Model &b, Index callback)
{
    return differ(a.callback(callback), b.callback(callback));
}

std::string callback_differences(const Model &a, const Model &b)
{
    const auto callback = [&](Index k) { return callback_difference(a, b, k); };
    return table_difference("callback", a.n_callbacks(), b.n_callbacks(), callback);
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
        found = piecewise_linear_differences(a, b);
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
        found = special_ordered_set_differences(a, b);
    }
    if (found.empty())
    {
        found = cone_differences(a, b);
    }
    if (found.empty())
    {
        found = complement_differences(a, b);
    }
    if (found.empty())
    {
        found = callback_differences(a, b);
    }
    return found;
}

}
