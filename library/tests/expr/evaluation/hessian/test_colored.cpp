#include "core/env/env.hpp"
#include "core/env/pool.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/timing/timer.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/hessian.hpp"
#include "expr/evaluation/hessian/sweep.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace
{

using biyos::Env;
using biyos::Graph;
using biyos::Hessian;
using biyos::Index;
using biyos::kBudget;
using biyos::kLanes;
using biyos::Lower;
using biyos::Offset;
using biyos::Operator;
using biyos::Pass;
using biyos::Pool;
using biyos::Real;
using biyos::Tape;
using biyos::TimingTree;
using biyos::to_index;
using biyos::to_size;
using biyos::Tolerances;
using biyos::test::arrow;
using biyos::test::band;
using biyos::test::clique;
using biyos::test::default_tolerances;
using biyos::test::products;
using biyos::test::Random;
using biyos::test::unary;
using biyos::test::variables;

using Values = std::vector<Real>;
using Rows = std::vector<Values>;

constexpr Index kLong = 100000;
constexpr std::array kSizes{15, 16, 17, 18, 24, 33, 64, 200};
constexpr Index kGuarded = 1000;
constexpr Index kNarrow = 24;
constexpr Index kClique = 6;
constexpr Offset kSplit = 10;
constexpr Index kSmall = 3;
constexpr std::uint64_t kPoints = 4;
constexpr std::size_t kShapes = 3;
constexpr std::array kWidths{std::size_t{3}, kLanes};

std::array<Offset, kShapes> roots_of(Graph &graph, Index size, Random &random)
{
    const std::vector<Offset> x = variables(graph, size);
    return {arrow(graph, x), band(graph, x), products(graph, x, random)};
}

Hessian hessian_of(const Graph &graph,
                   std::span<const Offset> roots,
                   std::size_t lanes = kLanes,
                   Offset budget = kBudget)
{
    Env env;
    TimingTree tree;
    return Hessian(graph, roots, {}, {env.stream(), tree}, lanes, budget);
}

Values column_of(const Tape &tape,
                 std::span<const Real> point,
                 const Tolerances &tolerances,
                 Index variable)
{
    Pass pass(tape, 1);
    pass.seeds[to_size(variable) * pass.lanes] = 1.0;
    sweep(tape, 0, point, tolerances, pass);
    Values column;
    for (std::size_t i = 0; i < to_size(tape.variables()); ++i)
    {
        column.push_back(pass.second.gradient[i * pass.lanes]);
    }
    return column;
}

Rows reference(const Graph &graph, Offset root, std::span<const Real> point)
{
    const Tape tape(graph, std::span(&root, 1));
    const Tolerances tolerances = default_tolerances();
    Rows columns;
    for (Index variable = 0; variable < tape.variables(); ++variable)
    {
        columns.push_back(column_of(tape, point, tolerances, variable));
    }
    return columns;
}

Values values_of(Hessian &hessian, std::span<const Real> point)
{
    Values out(hessian.pattern().rows.size(), -1.0);
    Pool pool;
    hessian.values(point, std::array{1.0}, default_tolerances(), out, pool);
    return out;
}

void expect_agree(Hessian &hessian, const Graph &graph, Offset root, std::span<const Real> point)
{
    const Values out = values_of(hessian, point);
    const Rows columns = reference(graph, root, point);
    const Tolerances tolerances = default_tolerances();
    const Lower &lower = hessian.pattern();
    for (std::size_t j = 0; j + 1 < lower.starts.size(); ++j)
    {
        for (Offset k = lower.starts[j]; k < lower.starts[j + 1]; ++k)
        {
            const auto i = to_size(lower.rows[to_size(k)]);
            EXPECT_TRUE(tolerances.is_zero(out[to_size(k)] - columns[j][i])) << i << " " << j;
        }
    }
}

Values point_of(Random &random, Index size)
{
    Values point;
    for (Index k = 0; k < size; ++k)
    {
        point.push_back(random.uniform(0.2, 1.2));
    }
    return point;
}

bool colored(const Graph &graph, Offset root)
{
    return hessian_of(graph, std::span(&root, 1)).colored(0);
}

bool cheap(Index wide)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, kNarrow);
    return colored(graph, clique(graph, x, arrow(graph, x), wide));
}

TEST(ColoringTest, Gradients)
{
    Graph graph;
    EXPECT_TRUE(colored(graph, arrow(graph, variables(graph, kLong))));
}

TEST(ColoringTest, Guard)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, kGuarded);
    const Offset total = graph.add_sum(0.0, x, Values(x.size(), 1.0));
    const std::array root{unary(graph, Operator::Exp, total)};
    const Hessian hessian = hessian_of(graph, root);
    EXPECT_FALSE(hessian.colored(0));
    EXPECT_FALSE(hessian.pushed(0));
}

TEST(ColoringTest, Threshold)
{
    Graph narrow;
    EXPECT_FALSE(colored(narrow, arrow(narrow, variables(narrow, to_index(kLanes)))));
    Graph wide;
    EXPECT_TRUE(colored(wide, arrow(wide, variables(wide, to_index(kLanes) + 1))));
    EXPECT_TRUE(cheap(kClique));
    EXPECT_FALSE(cheap(kClique + 1));
}

TEST(ColoringTest, Mixed)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, to_index(kLanes) + 1);
    const std::vector<Offset> narrow(x.begin(), x.begin() + kSmall);
    const std::array roots{arrow(graph, x), arrow(graph, narrow)};
    const Hessian hessian = hessian_of(graph, roots);
    EXPECT_TRUE(hessian.colored(0));
    EXPECT_FALSE(hessian.colored(1));
}

TEST(ColoringTest, Routed)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, kNarrow);
    const Offset tail = arrow(graph, x);
    const std::array roots{clique(graph, x, tail, kClique), unary(graph, Operator::Cos, tail)};
    EXPECT_TRUE(hessian_of(graph, roots, kLanes, kSplit).colored(0));
}

TEST(ColoringTest, Agree)
{
    std::array<std::size_t, kShapes> found{};
    for (const std::size_t lanes : kWidths)
    {
        for (const Index size : kSizes)
        {
            for (std::uint64_t seed = 0; seed < kPoints; ++seed)
            {
                Random random(seed);
                Graph graph;
                const std::array<Offset, kShapes> roots = roots_of(graph, size, random);
                const Values point = point_of(random, size);
                for (std::size_t r = 0; r < roots.size(); ++r)
                {
                    SCOPED_TRACE(testing::Message()
                                 << lanes << " " << size << " " << seed << " " << r);
                    Hessian hessian = hessian_of(graph, std::span(&roots[r], 1), lanes);
                    expect_agree(hessian, graph, roots[r], point);
                    found[r] += hessian.colored(0) ? 1U : 0U;
                }
            }
        }
    }
    for (const std::size_t count : found)
    {
        EXPECT_GT(count, 0U);
    }
}

}
