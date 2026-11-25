#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/index.hpp"
#include "expr/forms/terms.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/sparsity/reach.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace
{

using biyos::Evaluation;
using biyos::Index;
using biyos::IndexPair;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::QuadraticTerm;
using biyos::Request;
using biyos::Variable;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

int succeed(const Request &, std::span<double>, void *)
{
    return 0;
}

Model with_functions(Index variables, Index functions)
{
    Model model;
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    model.add_variables(std::vector<Variable>(static_cast<std::size_t>(variables), free));
    const auto n = static_cast<std::size_t>(functions);
    model.add_functions(std::vector<double>(n, 0.0), std::vector<Offset>(n + 1, 0), {});
    return model;
}

void add_linear(Model &model, Index function, Index variable, double coefficient)
{
    const std::array functions{function};
    const std::array<Offset, 2> starts{0, 1};
    model.add_linear_terms(functions, starts, {{.variable = variable, .coefficient = coefficient}});
}

void add_quadratic(Model &model, Index function, QuadraticTerm term)
{
    const std::array functions{function};
    const std::array<Offset, 2> starts{0, 1};
    model.add_quadratic_terms(functions, starts, {term});
}

void attach_sine(Model &model, Index function, Index variable)
{
    const Offset x = model.add_variable_node(variable);
    const std::array children{x};
    const Offset node = model.add_node(BIYOS_OP_SIN, children, {});
    const std::array functions{function};
    model.attach_expressions(functions, std::span(&node, 1));
}

Model merged()
{
    Model model = with_functions(3, 2);
    add_linear(model, 0, 2, 2.0);
    add_quadratic(model, 0, {.row = 0, .column = 1, .coefficient = 0.5});
    attach_sine(model, 1, 2);
    const std::array covered{1, 0};
    const Index callback = model.add_callback(covered, nullptr);
    model.set_callback_function(callback, succeed);
    const std::array<Index, 3> functions{0, 1, 1};
    const std::array<Index, 3> variables{2, 0, 0};
    model.set_callback_jacobian(callback, functions, variables, succeed);
    const std::array<Index, 1> first{0};
    const std::array<Index, 1> second{0};
    model.set_callback_hessian(callback, first, second, succeed);
    return model;
}

std::vector<IndexPair> entries_of(const biyos::Pattern &pattern)
{
    std::vector<IndexPair> entries;
    for (std::size_t row = 0; row + 1 < pattern.starts.size(); ++row)
    {
        for (Offset k = pattern.starts[row]; k < pattern.starts[row + 1]; ++k)
        {
            entries.push_back(
                {static_cast<Index>(row), pattern.variables[static_cast<std::size_t>(k)]});
        }
    }
    return entries;
}

std::vector<IndexPair> entries_of(const biyos::Lower &lower)
{
    std::vector<IndexPair> entries;
    for (std::size_t column = 0; column + 1 < lower.starts.size(); ++column)
    {
        for (Offset k = lower.starts[column]; k < lower.starts[column + 1]; ++k)
        {
            entries.push_back(
                {lower.rows[static_cast<std::size_t>(k)], static_cast<Index>(column)});
        }
    }
    return entries;
}

}

TEST(EvaluationTest, Patterns)
{
    biyos::Env env;
    const Model model = merged();
    const Evaluation evaluation(env, model);
    const std::vector<IndexPair> jacobian{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 2}};
    EXPECT_EQ(entries_of(evaluation.jacobian_pattern()), jacobian);
    const std::vector<IndexPair> hessian{{0, 0}, {1, 0}, {2, 2}};
    EXPECT_EQ(entries_of(evaluation.hessian_pattern()), hessian);
}

TEST(EvaluationTest, Empty)
{
    biyos::Env env;
    const Model model = with_functions(2, 2);
    const Evaluation evaluation(env, model);
    EXPECT_EQ(evaluation.jacobian_pattern().starts, (std::vector<Offset>{0, 0, 0}));
    EXPECT_TRUE(evaluation.jacobian_pattern().variables.empty());
    EXPECT_TRUE(evaluation.hessian_pattern().rows.empty());
}

TEST(EvaluationTest, Rebuild)
{
    biyos::Env env;
    Model model = merged();
    add_linear(model, 1, 1, 3.0);
    add_quadratic(model, 1, {.row = 2, .column = 2, .coefficient = 1.0});
    const Evaluation evaluation(env, model);
    const std::vector<IndexPair> jacobian{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}};
    EXPECT_EQ(entries_of(evaluation.jacobian_pattern()), jacobian);
    const std::vector<IndexPair> hessian{{0, 0}, {1, 0}, {2, 2}};
    EXPECT_EQ(entries_of(evaluation.hessian_pattern()), hessian);
}
