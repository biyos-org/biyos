#pragma once

#include "core/index.hpp"
#include "expr/terms.hpp"
#include "model/model.hpp"

#include <limits>
#include <vector>

namespace biyos::test
{

struct Quadratic
{
    std::vector<Index> functions;
    std::vector<Offset> starts;
    std::vector<QuadraticTerm> terms;
};

struct Parts
{
    std::vector<Variable> variables;
    std::vector<double> constants;
    std::vector<Offset> starts;
    std::vector<LinearTerm> terms;
    Quadratic quadratic;
    std::vector<Constraint> constraints;
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
                        {.first = 1, .second = 1, .coefficient = 0.5},
                        {.first = 2, .second = 0, .coefficient = -1.5},
                        {.first = 0, .second = 0, .coefficient = 2.0},
                    },
            },
        .constraints =
            {
                {.function = 0, .lower = -1.0, .upper = 1.0},
                {.function = 1, .lower = 0.0, .upper = kInfinity},
                {.function = 0, .lower = 3.0, .upper = 3.0},
            },
    };
}

inline Model built(const Parts &parts)
{
    Model model;
    model.add_variables(parts.variables);
    model.add_functions(parts.constants, parts.starts, parts.terms);
    const Quadratic &quadratic = parts.quadratic;
    model.add_quadratic_terms(quadratic.functions, quadratic.starts, quadratic.terms);
    model.add_constraints(parts.constraints);
    return model;
}

}
