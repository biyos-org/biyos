#include "model/compare/compare.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/points.hpp"
#include "model/attributes/overrides.hpp"
#include "model/compare/fields.hpp"
#include "model/compare/match.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

std::string differ(const IndexPair &a, const IndexPair &b)
{
    return first_of({
        compared("row", a.row, b.row),
        compared("column", a.column, b.column),
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

std::string differ(const Complement &a, const Complement &b)
{
    return first_of({
        compared("function", a.function, b.function),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
        compared("multiplier", a.multiplier, b.multiplier),
    });
}

std::string differ(const Member &a, const Member &b)
{
    return first_of({
        compared("variable", a.variable, b.variable),
        compared("weight", a.weight, b.weight),
    });
}

std::string differ(Index a, Index b)
{
    return compared("function", a, b);
}

std::string differ(double a, double b)
{
    return compared("value", a, b);
}

std::string differ(const Override &a, const Override &b)
{
    return first_of({
        compared("absolute tolerance", a.absolute, b.absolute),
        compared("relative tolerance", a.relative, b.relative),
    });
}

template <typename T> Offset size_of(std::span<const T> terms)
{
    return to_offset(terms.size());
}

template <typename T> const T &at(std::span<const T> terms, Offset k)
{
    return terms[to_size(k)];
}

template <typename T>
std::string term_differences(std::string_view noun, std::span<const T> a, std::span<const T> b)
{
    const auto term = [&](Offset k) { return differ(at(a, k), at(b, k)); };
    return table_difference(noun, size_of(a), size_of(b), term);
}

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

std::string presence(std::string_view field, const Routine &a, const Routine &b)
{
    if (static_cast<bool>(a) == static_cast<bool>(b))
    {
        return "";
    }
    return std::string(field) + (a ? " given against none" : " none against given");
}

template <typename T> std::span<const T> all_of(const std::vector<T> &items)
{
    return items;
}

std::string index_differences(std::string_view noun,
                              std::string_view field,
                              std::span<const Index> a,
                              std::span<const Index> b)
{
    const auto entry = [&](Offset k) { return compared(field, at(a, k), at(b, k)); };
    return table_difference(noun, size_of(a), size_of(b), entry);
}

std::string differ(const Callback &a, const Callback &b)
{
    return first_of({
        index_differences("covered function", "index", a.covered, b.covered),
        a.data == b.data ? "" : "data differs",
        presence("value function", a.function, b.function),
        term_differences("Jacobian entry", all_of(a.jacobian_pairs), all_of(b.jacobian_pairs)),
        presence("Jacobian function", a.jacobian, b.jacobian),
        term_differences("Hessian entry", all_of(a.hessian_pairs), all_of(b.hessian_pairs)),
        presence("Hessian function", a.hessian, b.hessian),
        presence("Hessian product", a.hessian_product, b.hessian_product),
    });
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
