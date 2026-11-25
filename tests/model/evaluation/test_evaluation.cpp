#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/index.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "expr/forms/terms.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/sparsity/reach.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
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
using biyos::test::failure;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

constexpr double kExact = 1e-15;
constexpr double kStale = 7.0;
constexpr double kStep = 1e-6;
constexpr double kCentral = 1e-8;
const std::vector<double> kPoint{0.3, -0.7, 1.1};

int user_values(const Request &request, std::span<double> out, void *)
{
    out[0] = request.x[0] * request.x[0];
    out[1] = 3.0 * request.x[2];
    return 0;
}

int user_gradients(const Request &request, std::span<double> out, void *)
{
    out[0] = 3.0;
    out[1] = request.x[0];
    out[2] = request.x[0];
    return 0;
}

int user_hessian(const Request &request, std::span<double> out, void *)
{
    out[0] = 2.0 * request.w[0];
    return 0;
}

int returning(const Request &, std::span<double> out, void *data)
{
    std::ranges::fill(out, 1.0);
    return *static_cast<const int *>(data);
}

int unknown(const Request &, std::span<double> out, void *)
{
    std::ranges::fill(out, 1.0);
    out.back() = std::numeric_limits<double>::quiet_NaN();
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
    model.set_callback_function(callback, user_values);
    const std::array<Index, 3> functions{0, 1, 1};
    const std::array<Index, 3> variables{2, 0, 0};
    model.set_callback_jacobian(callback, functions, variables, user_gradients);
    const std::array<Index, 1> first{0};
    const std::array<Index, 1> second{0};
    model.set_callback_hessian(callback, first, second, user_hessian);
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

std::vector<double> values_at(Evaluation &evaluation,
                              const Model &model,
                              const biyos::Lookup &lookup,
                              std::vector<double> x)
{
    std::vector<double> f(static_cast<std::size_t>(model.n_functions()), kStale);
    evaluation.values(lookup, x, f);
    return f;
}

std::vector<double> central_jacobian(Evaluation &evaluation,
                                     const Model &model,
                                     const biyos::Lookup &lookup,
                                     const std::vector<double> &x)
{
    const biyos::Pattern &pattern = evaluation.jacobian_pattern();
    std::vector<double> g(pattern.variables.size());
    for (std::size_t row = 0; row + 1 < pattern.starts.size(); ++row)
    {
        for (Offset k = pattern.starts[row]; k < pattern.starts[row + 1]; ++k)
        {
            const auto column =
                static_cast<std::size_t>(pattern.variables[static_cast<std::size_t>(k)]);
            std::vector<double> up = x;
            std::vector<double> down = x;
            up[column] += kStep;
            down[column] -= kStep;
            const double rise = values_at(evaluation, model, lookup, up)[row];
            const double fall = values_at(evaluation, model, lookup, down)[row];
            g[static_cast<std::size_t>(k)] = (rise - fall) / (2.0 * kStep);
        }
    }
    return g;
}

void expect_near(std::span<const double> found, std::span<const double> wanted, double bound)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        EXPECT_NEAR(found[k], wanted[k], bound) << "entry " << k;
    }
}

Model returning_model(int &status)
{
    Model model = with_functions(2, 2);
    const std::array covered{1};
    const Index callback = model.add_callback(covered, &status);
    model.set_callback_function(callback, returning);
    const std::array<Index, 1> functions{1};
    const std::array<Index, 1> variables{0};
    model.set_callback_jacobian(callback, functions, variables, returning);
    return model;
}

Model unknown_model()
{
    Model model = with_functions(2, 1);
    const std::array covered{0};
    const Index callback = model.add_callback(covered, nullptr);
    model.set_callback_function(callback, unknown);
    return model;
}

biyos::Lookup at_level(biyos::Env &env, biyos::Checking checking)
{
    env.options().set_int(biyos::Option::Checking, static_cast<int>(checking));
    return biyos::Lookup(env.options());
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

TEST(EvaluationTest, Values)
{
    biyos::Env env;
    const Model model = merged();
    Evaluation evaluation(env, model);
    const std::vector<double> f =
        values_at(evaluation, model, biyos::Lookup(env.options()), kPoint);
    const double f0 = 2.0 * 1.1 + 0.5 * 0.3 * -0.7 + 3.0 * 1.1;
    const double f1 = std::sin(1.1) + 0.3 * 0.3;
    expect_near(f, std::vector{f0, f1}, kExact);
}

TEST(EvaluationTest, Jacobian)
{
    biyos::Env env;
    const Model model = merged();
    Evaluation evaluation(env, model);
    const biyos::Lookup lookup(env.options());
    std::vector<double> g(evaluation.jacobian_pattern().variables.size());
    evaluation.jacobian(lookup, kPoint, g);
    expect_near(g, central_jacobian(evaluation, model, lookup, kPoint), kCentral);
    const std::vector<double> exact{0.5 * -0.7, 0.5 * 0.3, 5.0, 0.6, std::cos(1.1)};
    expect_near(g, exact, kExact);
}

TEST(EvaluationTest, Positive)
{
    biyos::Env env;
    int status = 1;
    const Model model = returning_model(status);
    Evaluation evaluation(env, model);
    const biyos::Lookup lookup(env.options());
    const std::vector<double> f = values_at(evaluation, model, lookup, {1.0, 2.0});
    EXPECT_NEAR(f[0], 0.0, kExact);
    EXPECT_TRUE(std::isnan(f[1]));
    std::vector<double> g(1);
    evaluation.jacobian(lookup, std::vector{1.0, 2.0}, g);
    EXPECT_TRUE(std::isnan(g[0]));
}

TEST(EvaluationTest, Negative)
{
    biyos::Env env;
    int status = -1;
    const Model model = returning_model(status);
    Evaluation evaluation(env, model);
    const biyos::Lookup lookup(env.options());
    std::vector<double> f(2);
    EXPECT_EQ(failure([&] { evaluation.values(lookup, std::vector{1.0, 2.0}, f); },
                      biyos::ErrorKind::Callback),
              "callback: callback 0: function returned -1");
    std::vector<double> g(1);
    EXPECT_EQ(failure([&] { evaluation.jacobian(lookup, std::vector{1.0, 2.0}, g); },
                      biyos::ErrorKind::Callback),
              "callback: callback 0: Jacobian returned -1");
}

TEST(EvaluationTest, Unknown)
{
    biyos::Env env;
    const Model model = unknown_model();
    Evaluation evaluation(env, model);
    std::vector<double> f(1);
    const biyos::Lookup cheap = at_level(env, biyos::Checking::Cheap);
    EXPECT_EQ(failure([&] { evaluation.values(cheap, std::vector{1.0, 2.0}, f); },
                      biyos::ErrorKind::Callback),
              "callback: callback 0: check 'finite output' failed: function output 0 is nan");
    const biyos::Lookup none = at_level(env, biyos::Checking::None);
    evaluation.values(none, std::vector{1.0, 2.0}, f);
    EXPECT_TRUE(std::isnan(f[0]));
}

TEST(EvaluationTest, Missing)
{
    biyos::Env env;
    const Model model = unknown_model();
    Evaluation evaluation(env, model);
    std::vector<double> g;
    EXPECT_EQ(
        failure([&]
                { evaluation.jacobian(biyos::Lookup(env.options()), std::vector{1.0, 2.0}, g); },
                biyos::ErrorKind::Unsupported),
        "unsupported: callback 0: has no Jacobian");
}

TEST(EvaluationTest, Sizes)
{
    biyos::Env env;
    const Model model = merged();
    Evaluation evaluation(env, model);
    const biyos::Lookup lookup(env.options());
    std::vector<double> f(2);
    EXPECT_EQ(refusal([&] { evaluation.values(lookup, std::vector{1.0}, f); }),
              "invalid argument: argument 'x': size 1, not 3");
    std::vector<double> g(4);
    EXPECT_EQ(refusal([&] { evaluation.jacobian(lookup, kPoint, g); }),
              "invalid argument: argument 'g': size 4, not 5");
}

TEST(EvaluationTest, Constant)
{
    biyos::Env env;
    Model model = with_functions(2, 2);
    model.set_constants(std::vector<Index>{0, 1}, std::vector{1.5, -2.0});
    Evaluation evaluation(env, model);
    const biyos::Lookup lookup(env.options());
    expect_near(values_at(evaluation, model, lookup, {1.0, 2.0}), std::vector{1.5, -2.0}, kExact);
    std::vector<double> g;
    evaluation.jacobian(lookup, std::vector{1.0, 2.0}, g);
    EXPECT_TRUE(g.empty());
}
