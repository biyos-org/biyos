#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "model/model.hpp"

#include <initializer_list>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace biyos::test::nl
{

inline constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
inline constexpr Index kDepth = 100000;

inline Variable continuous(Real lower, Real upper)
{
    return Variable{.kind = VariableKind::Continuous, .lower = lower, .upper = upper};
}

inline Offset node(Model &model,
                   Operator op,
                   std::initializer_list<Offset> children,
                   std::initializer_list<Real> values)
{
    return model.add_node(static_cast<int>(op), children, values);
}

inline void add_goal(Model &model, Index function, ObjectiveSense sense)
{
    Objective objective = default_objective(function, 0);
    objective.sense = sense;
    model.add_objectives(std::span(&objective, 1));
}

inline Model tiny_wanted(Offset (*build)(Model &, Offset))
{
    Model model;
    model.add_variables(std::vector{continuous(-kInfinity, kInfinity)});
    model.add_functions(std::vector{0.0, 0.0}, std::vector<Offset>{0, 0, 0}, {});
    const Offset root = build(model, model.add_variable_node(0));
    const Index function = 1;
    model.attach_expressions(std::span(&function, 1), std::span(&root, 1));
    const Constraint free{.function = 0, .lower = -kInfinity, .upper = kInfinity};
    model.add_constraints(std::span(&free, 1));
    add_goal(model, 1, ObjectiveSense::Minimize);
    return model;
}

inline Offset square_of(Model &model, Offset x)
{
    return node(model, Operator::ConstantPower, {x}, {2.0});
}

inline Offset log10_of(Model &model, Offset x)
{
    const Offset log = node(model, Operator::Log, {x}, {});
    return node(model, Operator::Product, {log}, {std::numbers::log10e});
}

inline Offset less_of(Model &model, Offset x)
{
    const Offset one = node(model, Operator::Constant, {}, {1.0});
    const Offset sum = node(model, Operator::Sum, {x, one}, {0.0, 1.0, -1.0});
    const Offset zero = node(model, Operator::Constant, {}, {0.0});
    return node(model, Operator::Max, {sum, zero}, {});
}

inline Offset pieces_of(Model &model, Offset x)
{
    const std::vector<Real> xs{-1.0, 0.0, 1.0, 2.0, 4.0};
    const std::vector<Real> ys{-3.0, 0.0, 3.0, 7.0, 17.0};
    return model.add_piecewise(model.add_piecewise_linear(xs, ys), x);
}

inline Offset sum_of(Model &model, Offset x)
{
    const Offset one = node(model, Operator::Constant, {}, {1.0});
    const Offset minus = node(model, Operator::Negation, {x}, {});
    return node(model, Operator::Sum, {x, one, minus}, {0.0, 1.0, 1.0, 1.0});
}

inline Index count_of(const Model &model, Operator op)
{
    Index count = 0;
    for (Offset k = 0; k < model.graph().size(); ++k)
    {
        count += model.graph().op(k) == op ? 1 : 0;
    }
    return count;
}

}
