#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/timing/clock.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace
{

using biyos::Env;
using biyos::Evaluation;
using biyos::Index;
using biyos::Lookup;
using biyos::Model;
using biyos::Offset;
using biyos::Option;
using biyos::Real;
using biyos::SystemClock;
using biyos::to_size;
using biyos::Variable;
using biyos::VariableKind;

constexpr Index kWidth = 1000;
constexpr std::array kThreads{1, 2, 3, 8, 1};
constexpr std::size_t kHardware = 8;

struct Outputs
{
    std::vector<Real> values;
    std::vector<Real> jacobian;
    std::vector<Real> hessian;
    std::vector<Real> product;
};

bool same(std::span<const Real> found, std::span<const Real> wanted)
{
    return std::ranges::equal(found, wanted, biyos::identical);
}

std::vector<Real> ramp(Real scale)
{
    std::vector<Real> ramp;
    for (Index k = 0; k < kWidth; ++k)
    {
        ramp.push_back(scale * std::sin(static_cast<Real>(k)));
    }
    return ramp;
}

Offset product(Model &model, Offset left, Offset right)
{
    const std::array children{left, right};
    return model.add_node(BIYOS_OP_PROD, children, std::array{1.0});
}

Offset sine(Model &model, Offset child)
{
    const std::array children{child};
    return model.add_node(BIYOS_OP_SIN, children, {});
}

Model chained()
{
    Model model;
    const Variable free{
        .kind = VariableKind::Continuous,
        .lower = -std::numeric_limits<Real>::infinity(),
        .upper = std::numeric_limits<Real>::infinity(),
    };
    model.add_variables(std::vector<Variable>(to_size(kWidth), free));
    const std::vector<Real> constants(to_size(kWidth), 0.0);
    model.add_functions(constants, std::vector<Offset>(to_size(kWidth) + 1, 0), {});
    std::vector<Offset> sines;
    for (Index k = 0; k < kWidth; ++k)
    {
        sines.push_back(sine(model, model.add_variable_node(k)));
    }
    std::vector<Offset> shared;
    for (std::size_t k = 0; k < sines.size(); ++k)
    {
        shared.push_back(sine(model, product(model, sines[k], sines[(k + 1) % sines.size()])));
    }
    std::vector<Index> functions;
    std::vector<Offset> roots;
    for (std::size_t k = 0; k < shared.size(); ++k)
    {
        functions.push_back(static_cast<Index>(k));
        roots.push_back(sine(model, product(model, shared[k], shared[(k + 1) % shared.size()])));
    }
    model.attach_expressions(functions, roots);
    return model;
}

Outputs outputs(Evaluation &evaluation, const Lookup &lookup)
{
    const std::vector<Real> x = ramp(1.0);
    const std::vector<Real> w = ramp(2.0);
    const std::vector<Real> v = ramp(-1.5);
    std::vector<Real> f(to_size(kWidth));
    std::vector<Real> g(evaluation.jacobian_pattern().variables.size());
    std::vector<Real> h(evaluation.hessian_pattern().rows.size());
    std::vector<Real> hv(to_size(kWidth));
    evaluation.values(lookup, x, f);
    evaluation.jacobian(lookup, x, g);
    evaluation.hessian(lookup, {.x = x, .w = w, .v = {}}, h);
    evaluation.product(lookup, {.x = x, .w = w, .v = v}, hv);
    return Outputs{.values = f, .jacobian = g, .hessian = h, .product = hv};
}

}

TEST(ThreadTest, Evaluation)
{
    Env env(std::make_unique<SystemClock>(), kHardware);
    const Model model = chained();
    Evaluation evaluation(model, env.stream(), env.pool());
    const Lookup lookup(env.options());
    const Outputs wanted = outputs(evaluation, lookup);
    for (const int threads : kThreads)
    {
        SCOPED_TRACE(threads);
        env.options().set_int(Option::Threads, threads);
        const Outputs found = outputs(evaluation, lookup);
        EXPECT_TRUE(same(found.values, wanted.values));
        EXPECT_TRUE(same(found.jacobian, wanted.jacobian));
        EXPECT_TRUE(same(found.hessian, wanted.hessian));
        EXPECT_TRUE(same(found.product, wanted.product));
    }
    EXPECT_EQ(env.pool().workers(), 7U);
}

TEST(ThreadTest, Values)
{
    Env env(std::make_unique<SystemClock>(), kHardware);
    env.options().set_int(Option::Threads, 8);
    const Model model = chained();
    Evaluation evaluation(model, env.stream(), env.pool());
    const std::vector<Real> x = ramp(1.0);
    std::vector<Real> f(to_size(kWidth));
    evaluation.values(Lookup(env.options()), x, f);
    EXPECT_EQ(env.pool().workers(), 7U);
}

TEST(ThreadTest, Capped)
{
    Env env(std::make_unique<SystemClock>(), 2);
    env.options().set_int(Option::Threads, 8);
    const Model model = chained();
    Evaluation evaluation(model, env.stream(), env.pool());
    const std::vector<Real> x = ramp(1.0);
    std::vector<Real> f(to_size(kWidth));
    evaluation.values(Lookup(env.options()), x, f);
    EXPECT_EQ(env.options().get_int(Option::Threads), 8);
    EXPECT_EQ(env.pool().workers(), 1U);
}
