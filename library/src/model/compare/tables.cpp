#include "model/compare/compare.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/points.hpp"
#include "model/compare/fields.hpp"
#include "model/compare/match.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"
#include "model/parts/attributes/records.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

Real constant_of(const Model &model, Index function)
{
    return model.constants(function, function + 1).front();
}

std::span<const LinearTerm> linear_of(const Model &model, Index function)
{
    return model.linear_terms(function, function + 1);
}

std::span<const QuadraticTerm> quadratic_of(const Model &model, Index function)
{
    return model.quadratic_terms(function, function + 1);
}

Offset root_of(const Model &model, Index function)
{
    return model.roots(function, function + 1).front();
}

std::span<const Member> members_of(const Model &model, Index sos)
{
    return model.members(sos, sos + 1);
}

ConeKind cone_kind_of(const Model &model, Index cone)
{
    return model.cone_kinds(cone, cone + 1).front();
}

std::span<const Index> cone_functions_of(const Model &model, Index cone)
{
    return model.cone_functions(cone, cone + 1);
}

std::span<const Real> cone_parameters_of(const Model &model, Index cone)
{
    return model.cone_parameters(cone, cone + 1);
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

std::string tolerated(const Model &a, const Model &b, Family family, Index index)
{
    return differ(a.tolerance_of(family, index), b.tolerance_of(family, index));
}

std::string variable_difference(const Model &a, const Model &b, Index variable)
{
    return first_of({
        differ(a.variable(variable), b.variable(variable)),
        tolerated(a, b, Family::Variable, variable),
        named(a, b, Family::Variable, variable),
    });
}

std::string function_difference(const Model &a, const Model &b, Index function, Matching &matching)
{
    return first_of({
        compared("constant", constant_of(a, function), constant_of(b, function)),
        term_table("term", linear_of(a, function), linear_of(b, function)),
        term_table("quadratic term", quadratic_of(a, function), quadratic_of(b, function)),
        expression_difference(matching, root_of(a, function), root_of(b, function)),
        named(a, b, Family::Function, function),
    });
}

std::string constraint_difference(const Model &a, const Model &b, Index constraint)
{
    return first_of({
        differ(a.constraint(constraint), b.constraint(constraint)),
        tolerated(a, b, Family::Constraint, constraint),
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

std::string special_ordered_set_difference(const Model &a, const Model &b, Index sos)
{
    return first_of({
        compared("kind", a.kind_of_special_ordered_set(sos), b.kind_of_special_ordered_set(sos)),
        term_table("member", members_of(a, sos), members_of(b, sos)),
        named(a, b, Family::SpecialOrderedSet, sos),
    });
}

std::string cone_difference(const Model &a, const Model &b, Index cone)
{
    return first_of({
        compared("kind", cone_kind_of(a, cone), cone_kind_of(b, cone)),
        value_table("member", "function", cone_functions_of(a, cone), cone_functions_of(b, cone)),
        value_table("parameter", "value", cone_parameters_of(a, cone), cone_parameters_of(b, cone)),
        tolerated(a, b, Family::Cone, cone),
        named(a, b, Family::Cone, cone),
    });
}

std::string complement_difference(const Model &a, const Model &b, Index complement)
{
    return first_of({
        differ(a.complement(complement), b.complement(complement)),
        tolerated(a, b, Family::Complement, complement),
        named(a, b, Family::Complement, complement),
    });
}

std::string variable_table(const Model &a, const Model &b)
{
    const auto variable = [&](Index k) { return variable_difference(a, b, k); };
    return table_difference("variable", a.n_variables(), b.n_variables(), variable);
}

const Points &points_of(const Model &model)
{
    return model.graph().points();
}

std::string piecewise_linear_difference(const Model &a, const Model &b, Offset k)
{
    return differ(points_of(a).piecewise_linear(k), points_of(b).piecewise_linear(k));
}

std::string piecewise_linear_table(const Model &a, const Model &b)
{
    const auto piecewise_linear = [&](Offset k) { return piecewise_linear_difference(a, b, k); };
    return table_difference("pwl", points_of(a).size(), points_of(b).size(), piecewise_linear);
}

std::string function_table(const Model &a, const Model &b)
{
    Matching matching(a.graph(), b.graph());
    const auto function = [&](Index k) { return function_difference(a, b, k, matching); };
    return table_difference("function", a.n_functions(), b.n_functions(), function);
}

std::string constraint_table(const Model &a, const Model &b)
{
    const auto constraint = [&](Index k) { return constraint_difference(a, b, k); };
    return table_difference("constraint", a.n_constraints(), b.n_constraints(), constraint);
}

std::string objective_table(const Model &a, const Model &b)
{
    const auto objective = [&](Index k) { return objective_difference(a, b, k); };
    return table_difference("objective", a.n_objectives(), b.n_objectives(), objective);
}

std::string indicator_table(const Model &a, const Model &b)
{
    const auto indicator = [&](Index k) { return indicator_difference(a, b, k); };
    return table_difference("indicator", a.n_indicators(), b.n_indicators(), indicator);
}

std::string set_table(const Model &a, const Model &b)
{
    const auto entry = [&](Index k) { return special_ordered_set_difference(a, b, k); };
    return table_difference("set", a.n_special_ordered_sets(), b.n_special_ordered_sets(), entry);
}

std::string cone_table(const Model &a, const Model &b)
{
    const auto cone = [&](Index k) { return cone_difference(a, b, k); };
    return table_difference("cone", a.n_cones(), b.n_cones(), cone);
}

std::string complement_table(const Model &a, const Model &b)
{
    const auto complement = [&](Index k) { return complement_difference(a, b, k); };
    return table_difference("complement", a.n_complements(), b.n_complements(), complement);
}

std::string callback_difference(const Model &a, const Model &b, Index callback)
{
    return differ(a.callback(callback), b.callback(callback));
}

std::string callback_table(const Model &a, const Model &b)
{
    const auto callback = [&](Index k) { return callback_difference(a, b, k); };
    return table_difference("callback", a.n_callbacks(), b.n_callbacks(), callback);
}

using Table = std::string (*)(const Model &, const Model &);

constexpr auto kTables = std::to_array<Table>({
    variable_table,
    piecewise_linear_table,
    function_table,
    constraint_table,
    objective_table,
    indicator_table,
    set_table,
    cone_table,
    complement_table,
    callback_table,
});

}

std::string difference(const Model &a, const Model &b)
{
    const std::string name = compared("model name", a.name(), b.name());
    if (!name.empty())
    {
        return name;
    }
    for (const Table table : kTables)
    {
        std::string found = table(a, b);
        if (!found.empty())
        {
            return found;
        }
    }
    return "";
}

}
