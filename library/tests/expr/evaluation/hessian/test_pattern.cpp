#include "core/env/env.hpp"
#include "core/env/pool.hpp"
#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "core/timing/timer.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/hessian.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
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
#include <utility>
#include <vector>

namespace
{

using biyos::Elements;
using biyos::elements_of;
using biyos::Entry;
using biyos::Env;
using biyos::Graph;
using biyos::Hessian;
using biyos::Index;
using biyos::is_zero;
using biyos::Lower;
using biyos::Offset;
using biyos::Operator;
using biyos::Pool;
using biyos::Pusher;
using biyos::Real;
using biyos::Shape;
using biyos::Tape;
using biyos::TimingTree;
using biyos::to_size;
using biyos::test::arrow;
using biyos::test::binary;
using biyos::test::default_tolerances;
using biyos::test::kBudgets;
using biyos::test::kExponents;
using biyos::test::kHeights;
using biyos::test::kKinks;
using biyos::test::kVariables;
using biyos::test::kWide;
using biyos::test::picks;
using biyos::test::piecewise;
using biyos::test::power;
using biyos::test::Random;
using biyos::test::random_graph;
using biyos::test::sign_power;
using biyos::test::unary;
using biyos::test::variables;
using biyos::test::waves_of;

using Values = std::vector<Real>;
using Pairs = std::vector<std::pair<Index, Index>>;

constexpr std::size_t kPoints = 3;
constexpr std::size_t kPicked = 4;

Pairs pairs_of(const Tape &tape, Offset element, const Shape &shape)
{
    const std::span<const Index> indices = biyos::variables_of(tape.pattern(), element);
    Pairs pairs;
    for (const Entry entry : shape.entries)
    {
        pairs.emplace_back(indices[to_size(entry.row)], indices[to_size(entry.column)]);
    }
    return pairs;
}

Pairs exact(const Graph &graph, Offset root)
{
    const Tape tape(graph, std::span(&root, 1));
    Pusher pusher(tape);
    return pairs_of(tape, 0, pusher.shape(0));
}

Pairs nonzeros(const Graph &graph, Offset root, std::span<const Values> points)
{
    Env env;
    TimingTree tree;
    Hessian hessian(graph, std::span(&root, 1), {}, {env.stream(), tree});
    const Lower &lower = hessian.pattern();
    Pairs pairs;
    Pool pool;
    for (const Values &point : points)
    {
        Values out(lower.rows.size(), 0.0);
        hessian.values(point, std::array{1.0}, default_tolerances(), out, pool);
        for (std::size_t j = 0; j + 1 < lower.starts.size(); ++j)
        {
            for (auto k = to_size(lower.starts[j]); k < to_size(lower.starts[j + 1]); ++k)
            {
                if (std::isfinite(out[k]) && !is_zero(out[k]))
                {
                    pairs.emplace_back(lower.rows[k], static_cast<Index>(j));
                }
            }
        }
    }
    std::ranges::sort(pairs);
    pairs.erase(std::ranges::unique(pairs).begin(), pairs.end());
    return pairs;
}

std::vector<Values> points_of(Random &random, Real low, Real high)
{
    std::vector<Values> points(kPoints);
    for (Values &point : points)
    {
        for (Index k = 0; k < kVariables; ++k)
        {
            point.push_back(random.uniform(low, high));
        }
    }
    return points;
}

Offset scaled(Graph &graph, Real coefficient, Offset left, Offset right)
{
    return graph.add(Operator::Product, coefficient, std::array{left, right});
}

std::vector<Offset> every_rule(Graph &graph)
{
    const std::vector<Offset> x = variables(graph, kVariables);
    const Offset pwl = graph.add_piecewise_linear(kKinks, kHeights);
    const Offset inner = scaled(graph, 1.0, x[0], x[1]);
    std::vector<Offset> roots;
    for (const Operator op : biyos::test::kUnary)
    {
        const Real coefficient = op == Operator::Acosh ? 4.0 : 1.0;
        roots.push_back(unary(graph, op, scaled(graph, coefficient, x[0], x[1])));
    }
    for (const Real exponent : kExponents)
    {
        roots.push_back(power(graph, inner, exponent));
        roots.push_back(sign_power(graph, inner, std::fabs(exponent)));
    }
    roots.push_back(power(graph, inner, 1.0));
    roots.push_back(sign_power(graph, inner, 1.0));
    for (const Operator op : biyos::test::kBinary)
    {
        roots.push_back(binary(graph, op, x[0], x[1]));
    }
    roots.push_back(binary(graph, Operator::Division, inner, x[2]));
    for (const Operator op : biyos::test::kListed)
    {
        roots.push_back(binary(graph, op, inner, scaled(graph, 0.5, x[0], x[1])));
    }
    const std::array terms{inner, scaled(graph, 1.0, x[2], x[3])};
    roots.push_back(graph.add_sum(1.0, terms, std::array{2.0, -1.0}));
    roots.push_back(graph.add(Operator::Product, 2.0, std::array{x[0], x[1], x[2]}));
    roots.push_back(graph.add(Operator::Product, 1.0, std::array{x[0], x[0], x[1]}));
    roots.push_back(piecewise(graph, pwl, inner));
    return roots;
}

Offset band(Graph &graph, const std::vector<Offset> &x)
{
    std::vector<Offset> waves;
    for (std::size_t k = 0; k + 2 < x.size(); ++k)
    {
        const std::array three{x[k], x[k + 1], x[k + 2]};
        waves.push_back(unary(graph, Operator::Sin, graph.add_sum(0.0, three, Values(3, 1.0))));
    }
    const Offset total = graph.add_sum(0.0, waves, Values(waves.size(), 1.0));
    return unary(graph, Operator::Abs, total);
}

Pairs arrow_pairs(Index size)
{
    Pairs pairs{{0, 0}};
    for (Index k = 1; k < size; ++k)
    {
        pairs.emplace_back(k, 0);
        pairs.emplace_back(k, k);
    }
    std::ranges::sort(pairs);
    return pairs;
}

Pairs sorted(Pairs pairs)
{
    std::ranges::sort(pairs);
    return pairs;
}

}

TEST(PatternTest, Operators)
{
    Graph graph;
    const std::vector<Offset> roots = every_rule(graph);
    Random random(7);
    const std::vector<Values> points = points_of(random, 0.6, 0.9);
    for (std::size_t k = 0; k < roots.size(); ++k)
    {
        SCOPED_TRACE(k);
        const Pairs found = sorted(exact(graph, roots[k]));
        EXPECT_FALSE(found.empty());
        EXPECT_EQ(found, nonzeros(graph, roots[k], points));
    }
}

TEST(PatternTest, Seeded)
{
    std::size_t checked = 0;
    for (std::uint64_t seed = 1; seed <= 30; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kPicked);
        const Elements elements = elements_of(graph, roots);
        const Tape tape(graph, elements.nodes, kBudgets[random.below(kBudgets.size())]);
        const std::vector<Values> points = points_of(random, 0.1, 2.0);
        Pusher pusher(tape);
        for (Offset e = 0; e < tape.roots(); ++e)
        {
            const Pairs found = sorted(pairs_of(tape, e, pusher.shape(e)));
            const Pairs wanted = nonzeros(graph, elements.nodes[to_size(e)], points);
            EXPECT_TRUE(std::ranges::includes(found, wanted)) << "element " << e;
            checked += wanted.size();
        }
    }
    EXPECT_GT(checked, 0U);
}

TEST(PatternTest, Arrow)
{
    Graph graph;
    const Offset root = arrow(graph, variables(graph, kWide));
    const Pairs found = sorted(exact(graph, root));
    EXPECT_EQ(found.size(), to_size(2 * kWide - 1));
    EXPECT_EQ(found, arrow_pairs(kWide));
}

TEST(PatternTest, Band)
{
    Graph graph;
    const Pairs found = exact(graph, band(graph, variables(graph, kWide)));
    EXPECT_EQ(found.size(), to_size(3 * kWide - 3));
    for (const auto &[row, column] : found)
    {
        EXPECT_LE(column, row);
        EXPECT_LE(row - column, 2);
    }
}

TEST(PatternTest, Ordered)
{
    Graph graph;
    const Offset root = band(graph, variables(graph, kWide));
    const Tape tape(graph, std::span(&root, 1));
    Pusher pusher(tape);
    const std::vector<Entry> entries = pusher.shape(0).entries;
    for (std::size_t k = 1; k < entries.size(); ++k)
    {
        const Entry last = entries[k - 1];
        const Entry next = entries[k];
        EXPECT_TRUE(last.column < next.column ||
                    (last.column == next.column && last.row < next.row))
            << k;
    }
}

TEST(PatternTest, Pushes)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset root = unary(graph, Operator::Sin, scaled(graph, 1.0, x[0], x[1]));
    const Tape tape(graph, std::span(&root, 1));
    Pusher pusher(tape);
    const Shape shape = pusher.shape(0);
    EXPECT_EQ(shape.pushes, 5);
    EXPECT_EQ(sorted(pairs_of(tape, 0, shape)), (Pairs{{0, 0}, {1, 0}, {1, 1}}));
    EXPECT_EQ(pusher.shape(0).pushes, 5);
}

TEST(PatternTest, Repeated)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset inner = scaled(graph, 1.0, x[0], x[1]);
    const Offset root =
        scaled(graph, 1.0, unary(graph, Operator::Exp, inner), unary(graph, Operator::Exp, inner));
    const Tape tape(graph, std::span(&root, 1));
    Pusher pusher(tape);
    const Shape shape = pusher.shape(0);
    EXPECT_EQ(shape.pushes, 9);
    EXPECT_EQ(sorted(pairs_of(tape, 0, shape)), (Pairs{{0, 0}, {1, 0}, {1, 1}}));
}

TEST(PatternTest, Pinned)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, kWide);
    const Offset total = waves_of(graph, x);
    const std::array roots{
        scaled(graph, 1.0, unary(graph, Operator::Exp, x[0]), total),
        band(graph, x),
        unary(graph, Operator::Atan, total),
    };
    const Tape whole(graph, roots);
    Pusher once(whole);
    std::vector<Pairs> wanted;
    for (Offset e = 0; e < whole.roots(); ++e)
    {
        wanted.push_back(pairs_of(whole, e, once.shape(e)));
    }
    EXPECT_EQ(sorted(wanted[0]), arrow_pairs(kWide));
    EXPECT_EQ(wanted[2].size(), to_size(kWide * (kWide + 1) / 2));
    for (const Offset budget : kBudgets)
    {
        SCOPED_TRACE(budget);
        const Tape split(graph, roots, budget);
        Pusher pusher(split);
        for (Offset e = split.roots(); e > 0; --e)
        {
            EXPECT_EQ(pairs_of(split, e - 1, pusher.shape(e - 1)), wanted[to_size(e - 1)]);
        }
    }
    EXPECT_GT(Tape(graph, roots, 1).pinned(), 0);
}

TEST(PatternTest, Linear)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset root = power(graph, scaled(graph, 1.0, x[0], x[1]), 1.0);
    EXPECT_EQ(sorted(exact(graph, root)), (Pairs{{1, 0}}));
    EXPECT_TRUE(exact(graph, power(graph, x[0], 0.0)).empty());
    const Offset linear = graph.add_sum(0.0, std::array{x[0], x[1]}, std::array{1.0, 2.0});
    const Offset shifted = unary(graph, Operator::Negation, unary(graph, Operator::Abs, linear));
    EXPECT_TRUE(exact(graph, shifted).empty());
}
