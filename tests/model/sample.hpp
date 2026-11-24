#pragma once

#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/forms/terms.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::test
{

struct Quadratic
{
    std::vector<Index> functions;
    std::vector<Offset> starts;
    std::vector<QuadraticTerm> terms;
};

struct Node
{
    int code;
    std::vector<Offset> children = {};
    std::vector<double> values = {};
    Index variable = -1;
};

struct Expressions
{
    std::vector<Node> nodes;
    std::vector<Index> functions;
    std::vector<Offset> roots;
};

struct SpecialOrderedSets
{
    std::vector<SpecialOrderedSetKind> kinds;
    std::vector<Offset> starts;
    std::vector<Member> members;
};

struct Cones
{
    std::vector<ConeKind> kinds;
    std::vector<Offset> starts;
    std::vector<Index> functions;
    std::vector<Offset> parameter_starts;
    std::vector<double> parameters;
};

struct Naming
{
    std::vector<Index> indices;
    std::vector<std::string_view> names;
};

struct Parts
{
    std::vector<Variable> variables;
    std::vector<double> constants;
    std::vector<Offset> starts;
    std::vector<LinearTerm> terms;
    Quadratic quadratic;
    Expressions expressions;
    std::vector<Constraint> constraints;
    std::vector<Objective> objectives;
    std::vector<Indicator> indicators;
    SpecialOrderedSets special_ordered_sets;
    Cones cones;
    std::vector<Complement> complements;
    std::string name;
    Naming variable_names;
    Naming function_names;
    Naming constraint_names;
    Naming objective_names;
    Naming indicator_names;
    Naming special_ordered_set_names;
    Naming cone_names;
    Naming complement_names;
};

inline Parts sample()
{
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    return Parts{
        .variables =
            {
                {.kind = VariableKind::Continuous, .lower = -0.0, .upper = 2.5},
                {.kind = VariableKind::Binary, .lower = 0.0, .upper = 1.0},
                {.kind = VariableKind::Integer, .lower = -kInfinity, .upper = 3.0},
            },
        .constants = {1.5, 0.0, 0.1},
        .starts = {0, 2, 3, 3},
        .terms =
            {
                {.variable = 0, .coefficient = 2.0},
                {.variable = 2, .coefficient = -1.0},
                {.variable = 1, .coefficient = 0.1},
            },
        .quadratic =
            {
                .functions = {2, 0},
                .starts = {0, 1, 3},
                .terms =
                    {
                        {.row = 1, .column = 1, .coefficient = 0.5},
                        {.row = 2, .column = 0, .coefficient = -1.5},
                        {.row = 0, .column = 0, .coefficient = 2.0},
                    },
            },
        .expressions =
            {
                .nodes =
                    {
                        {.code = BIYOS_OP_VAR, .variable = 0},
                        {.code = BIYOS_OP_VAR, .variable = 2},
                        {.code = BIYOS_OP_EXP, .children = {0}},
                        {.code = BIYOS_OP_PROD, .children = {2, 1}, .values = {0.5}},
                        {.code = BIYOS_OP_SUM, .children = {2, 3}, .values = {0.0, 2.0, 3.0}},
                        {.code = BIYOS_OP_CPOW, .children = {4}, .values = {1.5}},
                    },
                .functions = {0, 2, 1},
                .roots = {4, 3, 5},
            },
        .constraints =
            {
                {.function = 0, .lower = -1.0, .upper = 1.0},
                {.function = 1, .lower = 0.0, .upper = kInfinity},
                {.function = 0, .lower = 3.0, .upper = 3.0},
            },
        .objectives =
            {
                {
                    .function = 2,
                    .sense = ObjectiveSense::Maximize,
                    .priority = 1,
                    .weight = 2.0,
                    .absolute = 0.5,
                    .relative = 0.0,
                },
                {
                    .function = 0,
                    .sense = ObjectiveSense::Minimize,
                    .priority = 0,
                    .weight = 1.0,
                    .absolute = 0.0,
                    .relative = kInfinity,
                },
            },
        .indicators =
            {
                {.variable = 1, .value = 1, .function = 2, .lower = -kInfinity, .upper = 4.0},
                {.variable = 1, .value = 0, .function = 2, .lower = 1.0, .upper = 1.0},
            },
        .special_ordered_sets =
            {
                .kinds = {SpecialOrderedSetKind::Sos2, SpecialOrderedSetKind::Sos1},
                .starts = {0, 3, 5},
                .members =
                    {
                        {.variable = 2, .weight = 3.0},
                        {.variable = 0, .weight = 1.0},
                        {.variable = 1, .weight = 2.5},
                        {.variable = 0, .weight = -1.0},
                        {.variable = 1, .weight = 0.5},
                    },
            },
        .cones =
            {
                .kinds = {ConeKind::SecondOrder, ConeKind::Power},
                .starts = {0, 2, 5},
                .functions = {2, 0, 0, 1, 2},
                .parameter_starts = {0, 0, 2},
                .parameters = {0.25, 0.75},
            },
        .complements =
            {
                {.function = 1, .lower = 0.0, .upper = kInfinity, .multiplier = 2},
                {.function = 0, .lower = -2.0, .upper = 2.0, .multiplier = 0},
            },
        .name = "sample",
        .variable_names = {.indices = {2, 0}, .names = {"z", "x"}},
        .function_names = {.indices = {1}, .names = {"f"}},
        .constraint_names = {.indices = {1}, .names = {"c"}},
        .objective_names = {.indices = {0}, .names = {"cost"}},
        .indicator_names = {.indices = {1}, .names = {"off"}},
        .special_ordered_set_names = {.indices = {1}, .names = {"s"}},
        .cone_names = {.indices = {1}, .names = {"k"}},
        .complement_names = {.indices = {1}, .names = {"pair"}},
    };
}

inline void name(Model &model, Family family, const Naming &naming)
{
    model.set_names(family, naming.indices, naming.names);
}

inline std::vector<Offset> mapped(const std::vector<Offset> &ids,
                                  const std::vector<Offset> &positions)
{
    std::vector<Offset> found;
    for (const Offset position : positions)
    {
        found.push_back(ids[static_cast<std::size_t>(position)]);
    }
    return found;
}

inline Offset added(Model &model, const std::vector<Offset> &ids, const Node &node)
{
    if (node.code == BIYOS_OP_VAR)
    {
        return model.add_variable_node(node.variable);
    }
    return model.add_node(node.code, mapped(ids, node.children), node.values);
}

inline void attach(Model &model, const Expressions &expressions)
{
    std::vector<Offset> ids;
    for (const Node &node : expressions.nodes)
    {
        ids.push_back(added(model, ids, node));
    }
    model.attach_expressions(expressions.functions, mapped(ids, expressions.roots));
}

inline Model built(const Parts &parts)
{
    Model model;
    model.add_variables(parts.variables);
    model.add_functions(parts.constants, parts.starts, parts.terms);
    const Quadratic &quadratic = parts.quadratic;
    model.add_quadratic_terms(quadratic.functions, quadratic.starts, quadratic.terms);
    attach(model, parts.expressions);
    model.add_constraints(parts.constraints);
    model.add_objectives(parts.objectives);
    model.add_indicators(parts.indicators);
    model.add_special_ordered_sets(parts.special_ordered_sets.kinds,
                                   parts.special_ordered_sets.starts,
                                   parts.special_ordered_sets.members);
    const Cones &cones = parts.cones;
    model.add_cones(cones.kinds,
                    cones.starts,
                    cones.functions,
                    cones.parameter_starts,
                    cones.parameters);
    model.add_complements(parts.complements);
    if (!parts.name.empty())
    {
        model.set_name(parts.name);
    }
    name(model, Family::Variable, parts.variable_names);
    name(model, Family::Function, parts.function_names);
    name(model, Family::Constraint, parts.constraint_names);
    name(model, Family::Objective, parts.objective_names);
    name(model, Family::Indicator, parts.indicator_names);
    name(model, Family::SpecialOrderedSet, parts.special_ordered_set_names);
    name(model, Family::Cone, parts.cone_names);
    name(model, Family::Complement, parts.complement_names);
    return model;
}

}
