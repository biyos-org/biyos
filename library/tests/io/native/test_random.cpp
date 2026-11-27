#include "operators.hpp"
#include "texts.hpp"

#include "biyos/biyos.h"
#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::Complement;
using biyos::ConeKind;
using biyos::Constraint;
using biyos::Family;
using biyos::Index;
using biyos::Indicator;
using biyos::kConeKinds;
using biyos::kFamilies;
using biyos::kSenses;
using biyos::kSpecialOrderedSetKinds;
using biyos::kVariableKinds;
using biyos::LinearTerm;
using biyos::Member;
using biyos::Model;
using biyos::Objective;
using biyos::Offset;
using biyos::Operator;
using biyos::QuadraticTerm;
using biyos::Real;
using biyos::SpecialOrderedSetKind;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::expect_round;
using biyos::test::takes_condition;

namespace
{

using Engine = std::mt19937;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr std::array<std::uint32_t, 8> kSeeds{1, 2, 3, 5, 8, 13, 21, 34};
constexpr std::array kValues{-0.0, 0.0, 0.1, -2.5, 1e300, 5e-324, 3.0, 1.0 / 3.0};
constexpr std::array kSizes{0.1, 1.0, 2.5, 1.0 / 3.0};
constexpr int kNodes = 12;

struct Op
{
    int code;
    int children;
    int values;
};

constexpr std::array kOps{
    Op{.code = BIYOS_OP_CONST, .children = 0, .values = 1},
    Op{.code = BIYOS_OP_EXP, .children = 1, .values = 0},
    Op{.code = BIYOS_OP_LOG, .children = 1, .values = 0},
    Op{.code = BIYOS_OP_CPOW, .children = 1, .values = 1},
    Op{.code = BIYOS_OP_DIV, .children = 2, .values = 0},
    Op{.code = BIYOS_OP_ATAN2, .children = 2, .values = 0},
    Op{.code = BIYOS_OP_LE, .children = 2, .values = 0},
    Op{.code = BIYOS_OP_NE, .children = 2, .values = 0},
    Op{.code = BIYOS_OP_AND, .children = 2, .values = 0},
    Op{.code = BIYOS_OP_OR, .children = 2, .values = 0},
    Op{.code = BIYOS_OP_NOT, .children = 1, .values = 0},
    Op{.code = BIYOS_OP_IF, .children = 3, .values = 0},
    Op{.code = BIYOS_OP_PROD, .children = 2, .values = 1},
    Op{.code = BIYOS_OP_SUM, .children = 2, .values = 3},
};

struct Bounds
{
    Real lower;
    Real upper;
};

int below(Engine &engine, int n)
{
    return static_cast<int>(engine() % static_cast<std::uint32_t>(n));
}

bool coin(Engine &engine)
{
    return below(engine, 2) == 0;
}

template <typename T, std::size_t N> T any(Engine &engine, const std::array<T, N> &items)
{
    return items[static_cast<std::size_t>(below(engine, static_cast<int>(N)))];
}

Real size(Engine &engine)
{
    return any(engine, kSizes);
}

Bounds bounds(Engine &engine)
{
    const Real lower = coin(engine) ? -kInfinity : any(engine, kValues);
    const Real upper = coin(engine) ? kInfinity : (std::max)(lower, 0.0) + size(engine);
    return Bounds{.lower = lower, .upper = upper};
}

std::vector<Index> shuffled(Engine &engine, Index n)
{
    std::vector<Index> all(static_cast<std::size_t>(n));
    std::iota(all.begin(), all.end(), Index{0});
    std::ranges::shuffle(all, engine);
    return all;
}

Variable variable(Engine &engine)
{
    const VariableKind kind = any(engine, kVariableKinds);
    if (kind == VariableKind::Binary)
    {
        return Variable{.kind = kind, .lower = 0.0, .upper = 1.0};
    }
    const Bounds range = bounds(engine);
    return Variable{.kind = kind, .lower = range.lower, .upper = range.upper};
}

void add_variables(Model &model, Engine &engine)
{
    std::vector<Variable> variables{{.kind = VariableKind::Binary, .lower = 0.0, .upper = 1.0}};
    for (int k = below(engine, 5); k >= 0; --k)
    {
        variables.push_back(variable(engine));
    }
    model.add_variables(variables);
}

void add_functions(Model &model, Engine &engine)
{
    std::vector<Real> constants;
    std::vector<Offset> starts{0};
    std::vector<LinearTerm> terms;
    for (int function = 3 + below(engine, 4); function > 0; --function)
    {
        constants.push_back(any(engine, kValues));
        for (Index variable = 0; variable < model.n_variables(); ++variable)
        {
            if (coin(engine))
            {
                terms.push_back({.variable = variable, .coefficient = any(engine, kValues)});
            }
        }
        starts.push_back(static_cast<Offset>(terms.size()));
    }
    model.add_functions(constants, starts, terms);
}

void add_quadratic(Model &model, Engine &engine)
{
    std::vector<Index> functions;
    std::vector<Offset> starts{0};
    std::vector<QuadraticTerm> terms;
    for (Index function = 0; function < model.n_functions(); function += 1 + below(engine, 2))
    {
        for (Index row = 0; row < model.n_variables(); ++row)
        {
            for (Index column = 0; column <= row; column += 1 + below(engine, 3))
            {
                terms.push_back({.row = row, .column = column, .coefficient = size(engine)});
            }
        }
        functions.push_back(function);
        starts.push_back(static_cast<Offset>(terms.size()));
    }
    model.add_quadratic_terms(functions, starts, terms);
}

Offset pick(Engine &engine, std::span<const Offset> nodes)
{
    return nodes[static_cast<std::size_t>(below(engine, static_cast<int>(nodes.size())))];
}

Offset operand(Model &model, Engine &engine, bool condition, std::span<const Offset> nodes)
{
    const Offset first = pick(engine, nodes);
    if (!condition)
    {
        return first;
    }
    const std::array pair{first, pick(engine, nodes)};
    return model.add_node(BIYOS_OP_LT, pair, {});
}

Offset node(Model &model, Engine &engine, std::span<const Offset> nodes)
{
    const Op op = any(engine, kOps);
    const auto drawn = static_cast<Operator>(op.code);
    std::vector<Offset> children;
    for (int k = 0; k < op.children; ++k)
    {
        children.push_back(operand(model, engine, takes_condition(drawn, k), nodes));
    }
    std::vector<Real> values;
    for (int k = 0; k < op.values; ++k)
    {
        values.push_back(size(engine));
    }
    return model.add_node(op.code, children, values);
}

void add_expressions(Model &model, Engine &engine)
{
    std::vector<Offset> nodes;
    for (Index variable = 0; variable < model.n_variables(); ++variable)
    {
        nodes.push_back(model.add_variable_node(variable));
    }
    const std::array x{-1.0, 0.0, 2.0};
    const std::array y{1.0, 0.0, any(engine, kValues)};
    const Offset table = model.add_piecewise_linear(x, y);
    nodes.push_back(model.add_piecewise(table, nodes.front()));
    for (int k = 0; k < kNodes; ++k)
    {
        nodes.push_back(node(model, engine, nodes));
    }
    std::vector<Index> functions;
    std::vector<Offset> roots;
    for (Index function = 0; function < model.n_functions(); function += 1 + below(engine, 2))
    {
        functions.push_back(function);
        roots.push_back(nodes[nodes.size() - 1 - static_cast<std::size_t>(below(engine, 4))]);
    }
    model.attach_expressions(functions, roots);
}

Index function(Model &model, Engine &engine)
{
    return below(engine, model.n_functions());
}

void add_constraints(Model &model, Engine &engine)
{
    std::vector<Constraint> constraints;
    for (int k = below(engine, 5); k > 0; --k)
    {
        const Bounds range = bounds(engine);
        constraints.push_back(
            {.function = function(model, engine), .lower = range.lower, .upper = range.upper});
    }
    model.add_constraints(constraints);
}

void add_objectives(Model &model, Engine &engine)
{
    std::vector<Objective> objectives;
    for (int k = below(engine, 3); k > 0; --k)
    {
        objectives.push_back({
            .function = function(model, engine),
            .sense = any(engine, kSenses),
            .priority = below(engine, 3),
            .weight = size(engine),
            .absolute = coin(engine) ? 0.0 : size(engine),
            .relative = coin(engine) ? kInfinity : size(engine),
        });
    }
    model.add_objectives(objectives);
}

void add_indicators(Model &model, Engine &engine)
{
    std::vector<Indicator> indicators;
    for (int k = below(engine, 3); k > 0; --k)
    {
        const Bounds range = bounds(engine);
        indicators.push_back({
            .variable = 0,
            .value = below(engine, 2),
            .function = function(model, engine),
            .lower = range.lower,
            .upper = range.upper,
        });
    }
    model.add_indicators(indicators);
}

void add_sets(Model &model, Engine &engine)
{
    std::vector<SpecialOrderedSetKind> kinds;
    std::vector<Offset> starts{0};
    std::vector<Member> members;
    for (int k = below(engine, 3); k > 0; --k)
    {
        const std::vector<Index> order = shuffled(engine, model.n_variables());
        const int n = 1 + below(engine, static_cast<int>(order.size()));
        for (int member = 0; member < n; ++member)
        {
            members.push_back({.variable = order[static_cast<std::size_t>(member)],
                               .weight = 1.5 * member - 1.0});
        }
        kinds.push_back(any(engine, kSpecialOrderedSetKinds));
        starts.push_back(static_cast<Offset>(members.size()));
    }
    model.add_special_ordered_sets(kinds, starts, members);
}

int parameters_of(ConeKind kind, Engine &engine)
{
    const bool power = kind == ConeKind::Power || kind == ConeKind::DualPower;
    return power ? 1 + below(engine, 2) : 0;
}

int dimension_of(ConeKind kind, int parameters, Engine &engine)
{
    switch (kind)
    {
    case ConeKind::Nonnegative:
    case ConeKind::SecondOrder:
        return 1 + below(engine, 3);
    case ConeKind::Rotated:
        return 2 + below(engine, 2);
    case ConeKind::Exponential:
    case ConeKind::DualExponential:
        return 3;
    case ConeKind::Power:
    case ConeKind::DualPower:
        return parameters + below(engine, 2);
    case ConeKind::Semidefinite:
        return coin(engine) ? 1 : 3;
    }
    return 1;
}

struct Cones
{
    std::vector<ConeKind> kinds;
    std::vector<Offset> starts{0};
    std::vector<Index> functions;
    std::vector<Offset> parameter_starts{0};
    std::vector<Real> parameters;
};

void add_cone(Cones &cones, const Model &model, Engine &engine)
{
    const ConeKind kind = any(engine, kConeKinds);
    const int parameters = parameters_of(kind, engine);
    const int dimension = dimension_of(kind, parameters, engine);
    const std::vector<Index> order = shuffled(engine, model.n_functions());
    cones.functions.insert(cones.functions.end(), order.begin(), order.begin() + dimension);
    for (int k = 0; k < parameters; ++k)
    {
        cones.parameters.push_back(size(engine));
    }
    cones.kinds.push_back(kind);
    cones.starts.push_back(static_cast<Offset>(cones.functions.size()));
    cones.parameter_starts.push_back(static_cast<Offset>(cones.parameters.size()));
}

void add_cones(Model &model, Engine &engine)
{
    Cones cones;
    for (int k = below(engine, 4); k > 0; --k)
    {
        add_cone(cones, model, engine);
    }
    model.add_cones(cones.kinds,
                    cones.starts,
                    cones.functions,
                    cones.parameter_starts,
                    cones.parameters);
}

void add_complements(Model &model, Engine &engine)
{
    std::vector<Complement> complements;
    for (int k = below(engine, 3); k > 0; --k)
    {
        const Bounds range = bounds(engine);
        complements.push_back({
            .function = function(model, engine),
            .lower = range.lower,
            .upper = range.upper,
            .multiplier = function(model, engine),
        });
    }
    model.add_complements(complements);
}

void add_names(Model &model, Engine &engine, Family family)
{
    std::vector<Index> indices;
    std::vector<std::string> names;
    for (Index k = 0; k < model.n_of(family); ++k)
    {
        if (coin(engine))
        {
            indices.push_back(k);
            names.push_back(std::string(coin(engine) ? "n " : "n\\") + std::to_string(k));
        }
    }
    const std::vector<std::string_view> views(names.begin(), names.end());
    model.set_names(family, indices, views);
}

void add_tolerances(Model &model, Engine &engine, Family family)
{
    for (Index k = 0; k < model.n_of(family); ++k)
    {
        const Real tolerance = size(engine);
        const std::span<const Real> given(&tolerance, 1);
        const std::span<const Index> index(&k, 1);
        const int parts = below(engine, 4);
        if (parts % 2 == 1)
        {
            model.set_tolerances(family, index, given, {});
        }
        if (parts / 2 == 1)
        {
            model.set_tolerances(family, index, {}, given);
        }
    }
}

Model random_model(Engine &engine)
{
    Model model;
    add_variables(model, engine);
    add_functions(model, engine);
    add_quadratic(model, engine);
    add_expressions(model, engine);
    add_constraints(model, engine);
    add_objectives(model, engine);
    add_indicators(model, engine);
    add_sets(model, engine);
    add_cones(model, engine);
    add_complements(model, engine);
    if (coin(engine))
    {
        model.set_name("random model");
    }
    for (const Family family : kFamilies)
    {
        add_names(model, engine, family);
        add_tolerances(model, engine, family);
    }
    return model;
}

}

TEST(NativeTest, Random)
{
    for (const std::uint32_t seed : kSeeds)
    {
        SCOPED_TRACE(seed);
        Engine engine(seed);
        expect_round(random_model(engine));
    }
}
