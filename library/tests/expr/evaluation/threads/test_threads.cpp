#include "core/env/env.hpp"
#include "core/env/pool.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/numeric/zero.hpp"
#include "core/timing/timer.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/hessian/hessian.hpp"
#include "expr/evaluation/threads/sweeps.hpp"
#include "expr/evaluation/threads/team.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace
{

using biyos::Adjoints;
using biyos::Env;
using biyos::Graph;
using biyos::Hessian;
using biyos::Index;
using biyos::is_zero;
using biyos::Offset;
using biyos::Operator;
using biyos::Pool;
using biyos::Real;
using biyos::Tape;
using biyos::Team;
using biyos::TimingTree;
using biyos::to_size;
using biyos::Tolerances;
using biyos::Worker;
using biyos::test::default_tolerances;
using biyos::test::kBudgets;
using biyos::test::kListed;
using biyos::test::listed;
using biyos::test::picks;
using biyos::test::Random;
using biyos::test::random_point;
using biyos::test::sum;
using biyos::test::unary;

constexpr Index kInputs = 24;
constexpr Offset kSize = 600;
constexpr std::size_t kOutputs = 40;
constexpr std::array<std::size_t, 3> kThreads{2, 3, 8};
constexpr Real kRounding = 1e-12;
constexpr std::array kBounded{Operator::Sin,
                              Operator::Cos,
                              Operator::Tanh,
                              Operator::Atan,
                              Operator::Erf};

struct Sweeps
{
    std::vector<Real> values;
    std::vector<Real> gradients;
    std::vector<Real> gradient;
};

struct Second
{
    std::vector<Real> values;
    std::vector<Real> product;
};

struct Case
{
    explicit Case(std::uint64_t seed);

    Random random;
    Graph graph;
    std::vector<Offset> roots;
    Tape tape;
    std::vector<Real> point;
    std::vector<Real> direction;
    std::vector<Real> weights;
};

Offset bounded(Graph &graph, Random &random)
{
    const std::vector<Offset> children = picks(random, graph, 2 + random.below(3));
    const std::array pair{children[0], children[1]};
    switch (random.below(4))
    {
    case 0:
        return unary(graph, kBounded[random.below(kBounded.size())], children[0]);
    case 1:
        return sum(graph, random, random.uniform(-1.0, 1.0), children);
    case 2:
        return graph.add(Operator::Product, random.uniform(-1.0, 1.0), pair);
    default:
        return listed(graph, kListed[random.below(kListed.size())], children);
    }
}

Graph sharing_graph(Random &random)
{
    Graph graph;
    for (Index k = 0; k < kInputs; ++k)
    {
        graph.add_variable(k);
    }
    while (graph.size() < kSize)
    {
        bounded(graph, random);
    }
    return graph;
}

std::vector<Real> draws(Random &random, std::size_t count, Real bound)
{
    std::vector<Real> found;
    for (std::size_t k = 0; k < count; ++k)
    {
        found.push_back(random.uniform(-bound, bound));
    }
    return found;
}

Case::Case(std::uint64_t seed)
    : random(seed), graph(sharing_graph(random)), roots(picks(random, graph, kOutputs)),
      tape(graph, roots, kBudgets[seed % kBudgets.size()]),
      point(draws(random, to_size(kInputs), 1.0)), direction(draws(random, to_size(kInputs), 1.0)),
      weights(draws(random, kOutputs, 2.0))
{
}

bool same(std::span<const Real> found, std::span<const Real> wanted)
{
    return std::ranges::equal(found, wanted, biyos::identical);
}

Sweeps sweep(const Case &test, std::size_t threads, Pool &pool)
{
    Team team(test.tape);
    team.fit(test.tape, threads);
    Sweeps found{.values = std::vector<Real>(kOutputs),
                 .gradients = std::vector<Real>(test.tape.pattern().variables.size()),
                 .gradient = std::vector<Real>(test.point.size())};
    std::vector<Real> values(kOutputs);
    const Tolerances tolerances = default_tolerances();
    forward(test.tape, test.point, tolerances, team, pool, values);
    jacobian(test.tape, test.point, tolerances, team, pool, found.values, found.gradients);
    EXPECT_TRUE(same(values, found.values));
    weighted_gradient(test.tape,
                      test.point,
                      test.weights,
                      tolerances,
                      team,
                      pool,
                      values,
                      found.gradient);
    return found;
}

Second second(const Case &test, std::size_t threads, Pool &pool)
{
    Env env;
    TimingTree tree;
    Hessian hessian(test.graph, test.roots, {}, {env.stream(), tree});
    hessian.fit(threads);
    Second found{.values = std::vector<Real>(hessian.pattern().rows.size()),
                 .product = std::vector<Real>(test.point.size())};
    const Tolerances tolerances = default_tolerances();
    hessian.values(test.point, test.weights, tolerances, found.values, pool, {env.stream(), tree});
    hessian.product(test.point, test.weights, test.direction, tolerances, found.product, pool);
    return found;
}

void expect_same(const Sweeps &found, const Sweeps &wanted)
{
    EXPECT_TRUE(same(found.values, wanted.values));
    EXPECT_TRUE(same(found.gradients, wanted.gradients));
    EXPECT_TRUE(same(found.gradient, wanted.gradient));
}

std::size_t expect_close(std::span<const Real> found, std::span<const Real> wanted)
{
    std::size_t checked = 0;
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        if (std::isfinite(wanted[k]))
        {
            EXPECT_NEAR(found[k], wanted[k], kRounding * (1.0 + std::fabs(wanted[k]))) << k;
            ++checked;
        }
    }
    return checked;
}

std::size_t finite(std::span<const Real> values)
{
    const auto real = [](Real value) { return std::isfinite(value); };
    return static_cast<std::size_t>(std::ranges::count_if(values, real));
}

bool cleared(const Adjoints &adjoints)
{
    return std::ranges::all_of(adjoints.slots, is_zero) &&
           std::ranges::all_of(adjoints.shared, is_zero) &&
           std::ranges::all_of(adjoints.gradient, is_zero);
}

bool idle(const Worker &worker)
{
    return cleared(worker.adjoints);
}

bool cleared(const Team &team)
{
    const bool parts = std::ranges::all_of(team.targets.parts, is_zero);
    return parts && std::ranges::all_of(team.members, idle) &&
           std::ranges::all_of(team.pinned, is_zero);
}

}

TEST(ThreadTest, Identical)
{
    Pool pool(kThreads.back());
    std::size_t reals = 0;
    for (std::uint64_t seed = 1; seed <= 12; ++seed)
    {
        SCOPED_TRACE(seed);
        const Case test(seed);
        const Sweeps wanted = sweep(test, 1, pool);
        reals += finite(wanted.gradients) + finite(wanted.gradient);
        for (const std::size_t threads : kThreads)
        {
            SCOPED_TRACE(threads);
            expect_same(sweep(test, threads, pool), wanted);
        }
    }
    EXPECT_GT(reals, 0);
    EXPECT_EQ(pool.workers(), kThreads.back() - 1);
}

TEST(ThreadTest, Hessian)
{
    Pool pool(kThreads.back());
    std::size_t reals = 0;
    for (std::uint64_t seed = 1; seed <= 6; ++seed)
    {
        SCOPED_TRACE(seed);
        const Case test(seed);
        const Second wanted = second(test, 1, pool);
        reals += finite(wanted.values) + finite(wanted.product);
        for (const std::size_t threads : kThreads)
        {
            SCOPED_TRACE(threads);
            const Second found = second(test, threads, pool);
            EXPECT_TRUE(same(found.values, wanted.values));
            EXPECT_TRUE(same(found.product, wanted.product));
        }
    }
    EXPECT_GT(reals, 0);
    EXPECT_EQ(pool.workers(), kThreads.back() - 1);
}

TEST(ThreadTest, Serial)
{
    Pool pool(kThreads.back());
    std::size_t checked = 0;
    for (std::uint64_t seed = 1; seed <= 12; ++seed)
    {
        SCOPED_TRACE(seed);
        const Case test(seed);
        const Sweeps found = sweep(test, 3, pool);
        EXPECT_TRUE(same(found.values, biyos::test::values_of(test.tape, test.point)));
        EXPECT_TRUE(same(found.gradients, biyos::test::jacobian_of(test.tape, test.point)));
        const std::vector<Real> gradient =
            biyos::test::gradient_of(test.tape, test.point, test.weights);
        checked += expect_close(found.gradient, gradient);
    }
    const Tolerances tolerances = default_tolerances();
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = biyos::test::random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, biyos::test::kRoots);
        const Tape tape(graph, roots, kBudgets[seed % kBudgets.size()]);
        const std::vector<Real> point = random_point(random);
        const std::vector<Real> weights(roots.size(), random.uniform(-2.0, 2.0));
        Team team(tape);
        team.fit(tape, 3);
        std::vector<Real> values(roots.size());
        std::vector<Real> gradient(point.size());
        weighted_gradient(tape, point, weights, tolerances, team, pool, values, gradient);
        checked += expect_close(gradient, biyos::test::gradient_of(tape, point, weights));
    }
    EXPECT_GT(checked, 20);
}

TEST(ThreadTest, Cleared)
{
    Pool pool(kThreads.back());
    const Case test(3);
    ASSERT_GT(test.tape.prepass(), 1);
    Team team(test.tape);
    std::vector<Real> values(kOutputs);
    std::vector<Real> gradients(test.tape.pattern().variables.size());
    std::vector<Real> gradient(test.point.size());
    const Tolerances tolerances = default_tolerances();
    for (const std::size_t threads : {std::size_t{8}, std::size_t{2}})
    {
        team.fit(test.tape, threads);
        EXPECT_EQ(team.size(), threads);
        jacobian(test.tape, test.point, tolerances, team, pool, values, gradients);
        EXPECT_TRUE(cleared(team));
        weighted_gradient(test.tape,
                          test.point,
                          test.weights,
                          tolerances,
                          team,
                          pool,
                          values,
                          gradient);
        EXPECT_TRUE(cleared(team));
    }
}
