#include "core/env/env.hpp"
#include "core/env/pool.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/timing/timer.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/hessian.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "refusal.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Bits;
using biyos::elements_of;
using biyos::Entry;
using biyos::Env;
using biyos::Graph;
using biyos::Hessian;
using biyos::Index;
using biyos::kLanes;
using biyos::Lower;
using biyos::Offset;
using biyos::Operator;
using biyos::Pool;
using biyos::Real;
using biyos::Tape;
using biyos::Timer;
using biyos::TimingTree;
using biyos::to_size;
using biyos::Tolerances;
using biyos::test::arrow;
using biyos::test::binary;
using biyos::test::branch;
using biyos::test::constant;
using biyos::test::default_tolerances;
using biyos::test::gradient_of;
using biyos::test::kBudgets;
using biyos::test::kSide;
using biyos::test::kVariables;
using biyos::test::kWide;
using biyos::test::picks;
using biyos::test::power;
using biyos::test::Random;
using biyos::test::random_graph;
using biyos::test::refusal;
using biyos::test::unary;
using biyos::test::variables;

using Values = std::vector<Real>;
using Rows = std::vector<Values>;

constexpr Real kStep = 1e-5;
constexpr Real kBound = 1e-6;
constexpr Real kRounding = 1e-12;
constexpr std::array kLaneCounts{std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{16}};
constexpr std::size_t kPicked = 4;

Values values_of(Hessian &hessian, std::span<const Real> point, std::span<const Real> weights)
{
    Values out(hessian.pattern().rows.size(), -1.0);
    Pool pool;
    hessian.values(point, weights, default_tolerances(), out, pool);
    return out;
}

Values product_of(Hessian &hessian,
                  std::span<const Real> point,
                  std::span<const Real> weights,
                  std::span<const Real> direction)
{
    Values out(point.size(), -1.0);
    Pool pool;
    hessian.product(point, weights, direction, default_tolerances(), out, pool);
    return out;
}

Rows dense(const Lower &lower, const Values &out, std::size_t size)
{
    Rows rows(size, Values(size, 0.0));
    for (std::size_t j = 0; j + 1 < lower.starts.size(); ++j)
    {
        for (Offset k = lower.starts[j]; k < lower.starts[j + 1]; ++k)
        {
            const auto i = static_cast<std::size_t>(lower.rows[to_size(k)]);
            rows[i][j] = out[to_size(k)];
            rows[j][i] = out[to_size(k)];
        }
    }
    return rows;
}

Rows differences(const Tape &tape, Values point, std::span<const Real> weights, Real step)
{
    Rows columns;
    for (std::size_t j = 0; j < point.size(); ++j)
    {
        const Real middle = point[j];
        const Real size = step * (std::max)(1.0, std::fabs(middle));
        point[j] = middle + size;
        const Values ahead = gradient_of(tape, point, weights);
        point[j] = middle - size;
        const Values behind = gradient_of(tape, point, weights);
        point[j] = middle;
        Values column(point.size());
        for (std::size_t i = 0; i < point.size(); ++i)
        {
            column[i] = (ahead[i] - behind[i]) / (2.0 * size);
        }
        columns.push_back(column);
    }
    return columns;
}

Real tolerance(Real difference)
{
    return kBound * (1.0 + std::fabs(difference));
}

bool converged(Real fine, Real coarse)
{
    return std::isfinite(fine) && std::fabs(fine - coarse) <= tolerance(fine);
}

std::size_t check_differences(const Graph &graph,
                              std::span<const Offset> roots,
                              const Values &point,
                              std::span<const Real> weights,
                              const Rows &rows)
{
    const Tape tape(graph, roots);
    const Rows fine = differences(tape, point, weights, kStep);
    const Rows coarse = differences(tape, point, weights, 2.0 * kStep);
    std::size_t checked = 0;
    for (std::size_t j = 0; j < point.size(); ++j)
    {
        for (std::size_t i = 0; i < point.size(); ++i)
        {
            SCOPED_TRACE(testing::Message() << i << ", " << j);
            if (converged(fine[j][i], coarse[j][i]) && std::isfinite(rows[i][j]))
            {
                EXPECT_NEAR(rows[i][j], fine[j][i], tolerance(fine[j][i]));
                ++checked;
            }
        }
    }
    return checked;
}

std::size_t check_product(const Rows &rows, std::span<const Real> direction, const Values &found)
{
    std::size_t checked = 0;
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        Real wanted = 0.0;
        Real size = 0.0;
        for (std::size_t j = 0; j < rows.size(); ++j)
        {
            wanted += rows[i][j] * direction[j];
            size += std::fabs(rows[i][j] * direction[j]);
        }
        if (std::isfinite(wanted))
        {
            SCOPED_TRACE(i);
            EXPECT_NEAR(found[i], wanted, kBound * (1.0 + size));
            ++checked;
        }
    }
    return checked;
}

void expect_rows(const Rows &found, const Rows &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t i = 0; i < wanted.size(); ++i)
    {
        for (std::size_t j = 0; j < wanted.size(); ++j)
        {
            SCOPED_TRACE(testing::Message() << i << ", " << j);
            EXPECT_NEAR(found[i][j], wanted[i][j], kRounding * (1.0 + std::fabs(wanted[i][j])));
        }
    }
}

void expect_same(const Values &found, const Values &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        SCOPED_TRACE(k);
        EXPECT_EQ(std::bit_cast<Bits>(found[k]), std::bit_cast<Bits>(wanted[k]))
            << found[k] << " against " << wanted[k];
    }
}

Real entry_of(Hessian &hessian, std::span<const Real> point, Index row, Index column)
{
    const Values out = values_of(hessian, point, std::array{1.0});
    const Lower &lower = hessian.pattern();
    for (Offset k = lower.starts[to_size(column)]; k < lower.starts[to_size(column) + 1]; ++k)
    {
        if (lower.rows[to_size(k)] == row)
        {
            return out[to_size(k)];
        }
    }
    ADD_FAILURE() << "no entry " << row << ", " << column;
    return std::nan("");
}

void expect_rows_at(Hessian &hessian, std::span<const Real> point, const Rows &wanted)
{
    const Values out = values_of(hessian, point, std::array{1.0});
    expect_rows(dense(hessian.pattern(), out, point.size()), wanted);
}

void expect_zero(Real value)
{
    EXPECT_EQ(std::bit_cast<Bits>(value), std::bit_cast<Bits>(0.0)) << value;
}

Offset vanished(Graph &graph, Offset inner, Offset factor)
{
    return graph.add(Operator::Product, 1.0, std::array{power(graph, inner, 0.0), factor});
}

Values interior_point(Random &random)
{
    Values point;
    for (Index k = 0; k < kVariables; ++k)
    {
        point.push_back(random.uniform(0.1, 2.0));
    }
    return point;
}

Values random_weights(Random &random, std::size_t size)
{
    Values weights;
    for (std::size_t k = 0; k < size; ++k)
    {
        weights.push_back(random.uniform(-2.0, 2.0));
    }
    return weights;
}

Offset wide(Graph &graph, const std::vector<Offset> &x)
{
    const std::vector<Real> ones(x.size(), 1.0);
    const Offset total = graph.add_sum(0.0, x, ones);
    const Offset wave = unary(graph, Operator::Sin, total);
    const Offset product = graph.add(Operator::Product, 0.5, x);
    return graph.add(Operator::Product, 1.0, std::array{wave, product});
}

std::size_t arrow_entries(Index size)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const Offset root = arrow(graph, variables(graph, size));
    const Hessian hessian(graph, std::span(&root, 1), {}, {env.stream(), tree});
    return hessian.pattern().rows.size();
}

Values ramp(Index size, Real first, Real step)
{
    Values values;
    for (Index k = 0; k < size; ++k)
    {
        values.push_back(first + step * static_cast<Real>(k));
    }
    return values;
}

}

TEST(HessianTest, Seeded)
{
    Env env;
    TimingTree tree;
    std::size_t checked = 0;
    std::size_t entries = 0;
    Offset pinned = 0;
    for (std::uint64_t seed = 1; seed <= 30; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kPicked);
        const Offset budget = kBudgets[random.below(kBudgets.size())];
        Hessian hessian(graph, roots, {}, {env.stream(), tree}, kLanes, budget);
        pinned += Tape(graph, elements_of(graph, roots).nodes, budget).pinned();
        for (int k = 0; k < 3; ++k)
        {
            const Values point = interior_point(random);
            const Values weights = random_weights(random, roots.size());
            const Values out = values_of(hessian, point, weights);
            const Rows rows = dense(hessian.pattern(), out, point.size());
            checked += check_differences(graph, roots, point, weights, rows);
            entries += point.size() * point.size();
        }
    }
    EXPECT_GT(pinned, 0);
    EXPECT_GT(2 * checked, entries);
}

TEST(HessianTest, Weights)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::array roots{
        unary(graph, Operator::Log, x),
        graph.add(Operator::Product, 1.0, std::array{x, y}),
        unary(graph, Operator::Sqrt, y),
    };
    Hessian hessian(graph, roots, {}, {env.stream(), tree});
    const std::array point{-1.0, 4.0};
    const Rows quiet =
        dense(hessian.pattern(), values_of(hessian, point, std::array{0.0, 2.0, 3.0}), 2);
    expect_rows(quiet, {{0.0, 2.0}, {2.0, -0.75 / 8.0}});
    const Rows loud =
        dense(hessian.pattern(), values_of(hessian, point, std::array{1.0, 2.0, 3.0}), 2);
    EXPECT_TRUE(std::isnan(loud[0][0]));
    EXPECT_FALSE(std::isnan(loud[1][1]));
}

TEST(HessianTest, Comparison)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset root = unary(graph, Operator::Sqrt, x[0]);
    const Offset below = binary(graph, Operator::LessEqual, root, constant(graph, 1.0));
    const Offset product = graph.add(Operator::Product, 3.0, std::array{below, x[1], x[1]});
    Hessian hessian(graph, std::span(&product, 1), {}, {env.stream(), tree});
    expect_rows_at(hessian, std::array{0.0, 0.5}, {{0.0, 0.0}, {0.0, 6.0}});
    expect_rows_at(hessian, std::array{4.0, 0.5}, {{0.0, 0.0}, {0.0, 0.0}});
}

TEST(HessianTest, Indicator)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset below = binary(graph, Operator::LessEqual, x[0], x[1]);
    const Offset product = graph.add(Operator::Product, 1.0, std::array{below, x[0], x[1]});
    Hessian hessian(graph, std::span(&product, 1), {}, {env.stream(), tree});
    expect_rows_at(hessian, std::array{1.0, 2.0}, {{0.0, 1.0}, {1.0, 0.0}});
    expect_rows_at(hessian, std::array{2.0, 1.0}, {{0.0, 0.0}, {0.0, 0.0}});
}

TEST(HessianTest, Connective)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset root = unary(graph, Operator::Sqrt, x[0]);
    const Offset below = binary(graph, Operator::LessEqual, root, constant(graph, 1.0));
    const Offset positive = binary(graph, Operator::Greater, x[1], constant(graph, 0.0));
    const Offset both = graph.add(Operator::And, std::array{below, positive});
    const Offset either = graph.add(Operator::Or, std::array{below, positive});
    const Offset outside = unary(graph, Operator::Not, below);
    const std::array weights{3.0, 2.0, 1.0};
    const std::array conditions{both, either, outside};
    std::vector<Offset> products;
    for (std::size_t k = 0; k < conditions.size(); ++k)
    {
        products.push_back(
            graph.add(Operator::Product, weights[k], std::array{conditions[k], x[1], x[1]}));
    }
    Hessian hessian(graph, products, {}, {env.stream(), tree});
    const std::array ones{1.0, 1.0, 1.0};
    expect_rows(dense(hessian.pattern(), values_of(hessian, std::array{0.0, 0.5}, ones), 2),
                {{0.0, 0.0}, {0.0, 10.0}});
    expect_rows(dense(hessian.pattern(), values_of(hessian, std::array{4.0, 0.5}, ones), 2),
                {{0.0, 0.0}, {0.0, 6.0}});
    expect_rows(dense(hessian.pattern(), values_of(hessian, std::array{4.0, -0.5}, ones), 2),
                {{0.0, 0.0}, {0.0, 2.0}});
    const Offset undefined = graph.add(Operator::Or, std::array{positive, below});
    Hessian nan(graph, std::span(&undefined, 1), {}, {env.stream(), tree});
    EXPECT_TRUE(std::isnan(entry_of(nan, std::array{-1.0, 2.0}, 0, 0)));
    EXPECT_DOUBLE_EQ(entry_of(nan, std::array{1.0, 2.0}, 0, 0), 0.0);
}

TEST(HessianTest, Branches)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset zero = constant(graph, 0.0);
    const Offset positive = binary(graph, Operator::GreaterEqual, x[0], zero);
    const Offset root = unary(graph, Operator::Sqrt, x[0]);
    const Offset scaled = graph.add(Operator::Product, 1.0, std::array{root, x[1]});
    const Offset square = graph.add(Operator::Product, 1.0, std::array{x[1], x[1]});
    const Offset domain = branch(graph, positive, scaled, square);
    const Offset grown = unary(graph, Operator::Exp, x[0]);
    const Offset taken = branch(graph, positive, square, grown);
    const Offset shared = graph.add_sum(0.0, std::array{taken, grown}, std::array{1.0, 2.0});
    const Offset lifted = graph.add(Operator::Product, 1.0, std::array{taken, x[0]});
    const Offset rooted = binary(graph, Operator::GreaterEqual, root, zero);
    const Offset undefined = branch(graph, rooted, square, grown);
    const Real high = std::exp(0.5);
    const Real low = std::exp(-0.5);
    for (const std::size_t lanes : kLaneCounts)
    {
        SCOPED_TRACE(lanes);
        Hessian first(graph, std::span(&domain, 1), {}, {env.stream(), tree}, lanes);
        expect_rows(
            dense(first.pattern(), values_of(first, std::array{-1.0, 0.5}, std::array{1.0}), 2),
            {{0.0, 0.0}, {0.0, 2.0}});
        expect_rows(
            dense(first.pattern(), values_of(first, std::array{4.0, 0.5}, std::array{1.0}), 2),
            {{-1.0 / 64.0, 0.25}, {0.25, 0.0}});
        Hessian second(graph, std::span(&shared, 1), {}, {env.stream(), tree}, lanes);
        expect_rows(
            dense(second.pattern(), values_of(second, std::array{0.5, 0.3}, std::array{1.0}), 2),
            {{2.0 * high, 0.0}, {0.0, 2.0}});
        expect_rows(
            dense(second.pattern(), values_of(second, std::array{-0.5, 0.3}, std::array{1.0}), 2),
            {{3.0 * low, 0.0}, {0.0, 0.0}});
        Hessian third(graph, std::span(&lifted, 1), {}, {env.stream(), tree}, lanes);
        expect_rows(
            dense(third.pattern(), values_of(third, std::array{0.5, 0.3}, std::array{1.0}), 2),
            {{0.0, 0.6}, {0.6, 1.0}});
        expect_rows(
            dense(third.pattern(), values_of(third, std::array{-0.5, 0.3}, std::array{1.0}), 2),
            {{1.5 * low, 0.0}, {0.0, 0.0}});
        Hessian fourth(graph, std::span(&undefined, 1), {}, {env.stream(), tree}, lanes);
        const Values found = values_of(fourth, std::array{-1.0, 0.5}, std::array{1.0});
        EXPECT_FALSE(found.empty());
        for (const Real value : found)
        {
            EXPECT_TRUE(std::isnan(value));
        }
    }
}

TEST(HessianTest, Switch)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset one = constant(graph, 1.0);
    const Offset curved = graph.add(Operator::Product, 1.0, std::array{x[0], x[0], x[1]});
    const Offset line = graph.add_sum(0.0, x, std::array{3.0, 1.0});
    const Offset above = binary(graph, Operator::GreaterEqual, x[0], one);
    const Offset root = branch(graph, above, curved, line);
    const Values point{1.0, 2.0};
    const std::array weights{1.0};
    Hessian hessian(graph, std::span(&root, 1), {}, {env.stream(), tree});
    const Rows rows = dense(hessian.pattern(), values_of(hessian, point, weights), 2);
    expect_rows(rows, {{4.0, 2.0}, {2.0, 0.0}});
    const Tape tape(graph, std::span(&root, 1));
    const Values here = gradient_of(tape, point, weights);
    const Values ahead = gradient_of(tape, Values{1.0 + kSide, 2.0}, weights);
    for (std::size_t i = 0; i < point.size(); ++i)
    {
        const Real slope = (ahead[i] - here[i]) / kSide;
        EXPECT_NEAR(rows[i][0], slope, tolerance(slope)) << i;
    }
}

TEST(HessianTest, Apart)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset grown = unary(graph, Operator::Exp, x[2]);
    const Offset cubic = graph.add(Operator::Product, 1.0, x);
    const Offset then = graph.add_sum(0.0, std::array{cubic, grown}, std::array{1.0, 1.0});
    const Offset wave = unary(graph, Operator::Sin, x[0]);
    const Offset otherwise = graph.add(Operator::Product, 1.0, std::array{wave, x[2], x[2]});
    const Offset below = binary(graph, Operator::Less, x[0], x[1]);
    const Offset root = branch(graph, below, then, otherwise);
    Hessian hessian(graph, std::span(&root, 1), {}, {env.stream(), tree});
    const std::array weights{1.5};
    std::size_t checked = 0;
    for (const Values &point : {Values{0.2, 0.9, -0.4}, Values{0.9, 0.2, -0.4}})
    {
        const Rows rows = dense(hessian.pattern(), values_of(hessian, point, weights), 3);
        checked += check_differences(graph, std::span(&root, 1), point, weights, rows);
    }
    EXPECT_EQ(checked, 18U);
}

TEST(HessianTest, Masked)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset log = unary(graph, Operator::Log, x[0]);
    const std::array inner{
        log,
        graph.add_sum(0.0, std::array{log, x[0]}, std::array{2.0, 1.0}),
        binary(graph, Operator::Max, log, x[0]),
    };
    for (const Offset node : inner)
    {
        const Offset root = vanished(graph, node, x[1]);
        Hessian hessian(graph, std::span(&root, 1), {}, {env.stream(), tree});
        expect_zero(entry_of(hessian, std::array{-1.0, 2.0}, 1, 1));
    }
    const Offset grown = unary(graph, Operator::Exp, x[1]);
    const Offset fall = unary(graph, Operator::Log, unary(graph, Operator::Negation, grown));
    const Offset root = vanished(graph, fall, x[0]);
    Hessian hessian(graph, std::span(&root, 1), {}, {env.stream(), tree});
    expect_zero(entry_of(hessian, std::array{2.0, 0.5}, 0, 0));
    expect_zero(entry_of(hessian, std::array{2.0, 0.5}, 1, 0));
}

TEST(HessianTest, Undefined)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset top = binary(graph, Operator::Max, x[1], unary(graph, Operator::Log, x[0]));
    const Offset root = vanished(graph, top, x[1]);
    Hessian hessian(graph, std::span(&root, 1), {}, {env.stream(), tree});
    EXPECT_TRUE(std::isnan(entry_of(hessian, std::array{-1.0, 2.0}, 1, 1)));
    const Offset compared =
        binary(graph, Operator::LessEqual, unary(graph, Operator::Log, x[0]), x[1]);
    Hessian below(graph, std::span(&compared, 1), {}, {env.stream(), tree});
    EXPECT_TRUE(std::isnan(entry_of(below, std::array{-1.0, 2.0}, 0, 0)));
    EXPECT_DOUBLE_EQ(entry_of(below, std::array{1.0, 2.0}, 0, 0), 0.0);
}

TEST(HessianTest, Stale)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset root = unary(graph, Operator::Abs, unary(graph, Operator::Log, x));
    Hessian fresh(graph, std::span(&root, 1), {}, {env.stream(), tree});
    const Values wanted = values_of(fresh, std::array{2.0}, std::array{1.0});
    Hessian used(graph, std::span(&root, 1), {}, {env.stream(), tree});
    EXPECT_TRUE(std::isnan(values_of(used, std::array{-1.0}, std::array{1.0})[0]));
    expect_same(values_of(used, std::array{2.0}, std::array{1.0}), wanted);
}

TEST(HessianTest, Shared)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset inner = graph.add(Operator::Product, 1.0, std::array{x[0], x[1]});
    const Offset wave = unary(graph, Operator::Sin, inner);
    const Offset square = graph.add(Operator::Product, 1.0, std::array{wave, wave});
    const Offset grown = unary(graph, Operator::Exp, wave);
    const Offset scaled = graph.add(Operator::Product, 1.0, std::array{grown, x[2]});
    const Offset first = graph.add_sum(1.0, std::array{square, scaled}, std::array{1.5, -0.5});
    const Offset second = binary(graph, Operator::Division, wave, x[2]);
    const std::array roots{first, second};
    const Values point{0.7, 1.3, 0.9};
    const std::array weights{1.25, -0.75};
    for (const Offset budget : kBudgets)
    {
        SCOPED_TRACE(budget);
        Hessian hessian(graph, roots, {}, {env.stream(), tree}, kLanes, budget);
        const Rows rows = dense(hessian.pattern(), values_of(hessian, point, weights), 3);
        EXPECT_EQ(check_differences(graph, roots, point, weights, rows), 9U);
    }
    EXPECT_GT(Tape(graph, std::array{square, scaled, second}, 1).pinned(), 0);
}

TEST(HessianTest, Nested)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset cross = graph.add(Operator::Product, 1.0, std::array{x[0], x[1]});
    const Offset square = power(graph, x[1], 2.0);
    const Offset inner = graph.add_sum(4.0, std::array{cross, square}, std::array{3.0, -1.0});
    const Offset grown = unary(graph, Operator::Exp, x[0]);
    const Offset outer = graph.add_sum(1.0, std::array{inner, grown}, std::array{2.0, 0.5});
    const Offset product = graph.add(Operator::Product, 2.0, std::array{x[0], x[0], x[1]});
    const std::array roots{outer, product};
    Hessian hessian(graph, roots, {}, {env.stream(), tree});
    const std::array point{0.4, -1.1};
    const Rows rows = dense(hessian.pattern(), values_of(hessian, point, std::array{1.0, 3.0}), 2);
    const Real e = std::exp(0.4);
    expect_rows(rows, {{0.5 * e + 12.0 * -1.1, 6.0 + 12.0 * 0.4}, {6.0 + 12.0 * 0.4, -4.0}});
}

TEST(HessianTest, Chunks)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 7);
    const std::array roots{wide(graph, x), unary(graph, Operator::Cos, x[3])};
    const Values point{0.9, 1.1, 0.8, 1.2, 0.7, 1.3, 1.05};
    const std::array weights{0.8, -1.5};
    Hessian whole(graph, roots, {}, {env.stream(), tree}, 8);
    const Values once = values_of(whole, point, weights);
    for (const std::size_t lanes : {1U, 2U, 3U, 4U, 16U})
    {
        SCOPED_TRACE(lanes);
        Hessian split(graph, roots, {}, {env.stream(), tree}, lanes);
        expect_same(values_of(split, point, weights), once);
    }
    const Rows rows = dense(whole.pattern(), once, point.size());
    EXPECT_EQ(check_differences(graph, roots, point, weights, rows), 49U);
}

TEST(HessianTest, Classes)
{
    Env env;
    TimingTree tree;
    for (std::uint64_t seed = 1; seed <= 10; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kPicked);
        const Offset budget = kBudgets[random.below(kBudgets.size())];
        const Values point = interior_point(random);
        const Values weights = random_weights(random, roots.size());
        const Values direction = random_weights(random, point.size());
        Hessian widest(graph, roots, {}, {env.stream(), tree}, kLanes, budget);
        const Values wanted = values_of(widest, point, weights);
        const Values moved = product_of(widest, point, weights, direction);
        for (std::size_t lanes = 1; lanes < kLanes; ++lanes)
        {
            SCOPED_TRACE(lanes);
            Hessian hessian(graph, roots, {}, {env.stream(), tree}, lanes, budget);
            expect_same(values_of(hessian, point, weights), wanted);
            expect_same(product_of(hessian, point, weights, direction), moved);
        }
    }
}

TEST(HessianTest, Threshold)
{
    const auto lanes = static_cast<Index>(kLanes);
    EXPECT_EQ(arrow_entries(lanes), kLanes * (kLanes + 1) / 2);
    EXPECT_EQ(arrow_entries(lanes + 1), 2 * kLanes + 1);
}

TEST(HessianTest, Arrow)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, kWide);
    const std::array roots{arrow(graph, x),
                           graph.add(Operator::Product, 1.0, std::array{x[1], x.back()})};
    const std::array declared{Entry{.row = kWide - 1, .column = 2}};
    Hessian hessian(graph, roots, declared, {env.stream(), tree});
    EXPECT_EQ(hessian.pattern().rows.size(), to_size(2 * kWide + 1));
    const Values point = ramp(kWide, 0.1, 0.05);
    const std::array weights{1.5, -2.0};
    const Values out = values_of(hessian, point, weights);
    const Rows rows = dense(hessian.pattern(), out, point.size());
    EXPECT_EQ(check_differences(graph, roots, point, weights, rows), to_size(kWide * kWide));
    const Values direction = ramp(kWide, -1.0, 0.1);
    const Values found = product_of(hessian, point, weights, direction);
    EXPECT_EQ(check_product(rows, direction, found), to_size(kWide));
    for (const std::size_t lanes : {1U, 4U, 32U})
    {
        SCOPED_TRACE(lanes);
        Hessian split(graph, roots, declared, {env.stream(), tree}, lanes);
        expect_same(values_of(split, point, weights), out);
    }
}

TEST(HessianTest, Timed)
{
    Env env;
    Graph graph;
    const std::vector<Offset> x = variables(graph, kWide);
    const Offset wide = arrow(graph, x);
    const Offset narrow = arrow(graph, std::vector(x.begin(), x.begin() + 2));
    TimingTree tree;
    {
        const Timer outer(env.stream(), tree, "model", "evaluation");
        const Hessian hessian(graph, std::span(&wide, 1), {}, {env.stream(), tree});
    }
    const std::span<const TimingTree::Node> nodes = tree.nodes();
    ASSERT_EQ(nodes.size(), 3U);
    EXPECT_EQ(nodes[0].name, "evaluation");
    EXPECT_EQ(nodes[1].name, "pattern");
    EXPECT_EQ(nodes[1].parent, 0U);
    EXPECT_EQ(nodes[2].name, "coloring");
    EXPECT_EQ(nodes[2].parent, 0U);
    TimingTree small;
    const Hessian hessian(graph, std::span(&narrow, 1), {}, {env.stream(), small});
    EXPECT_TRUE(small.nodes().empty());
}

TEST(HessianTest, Wide)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::array roots{wide(graph, variables(graph, kWide))};
    Hessian hessian(graph, roots, {}, {env.stream(), tree});
    EXPECT_EQ(hessian.pattern().rows.size(), to_size(kWide * (kWide + 1) / 2));
    const Values point = ramp(kWide, 0.9, 0.01);
    const std::array weights{0.7};
    const Rows rows = dense(hessian.pattern(), values_of(hessian, point, weights), point.size());
    EXPECT_EQ(check_differences(graph, roots, point, weights, rows), to_size(kWide * kWide));
}

TEST(HessianTest, Product)
{
    Env env;
    TimingTree tree;
    std::size_t checked = 0;
    std::size_t entries = 0;
    for (std::uint64_t seed = 1; seed <= 30; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kPicked);
        const Offset budget = kBudgets[random.below(kBudgets.size())];
        Hessian hessian(graph, roots, {}, {env.stream(), tree}, kLanes, budget);
        const Values point = interior_point(random);
        const Values weights = random_weights(random, roots.size());
        const Values direction = random_weights(random, point.size());
        const Rows rows =
            dense(hessian.pattern(), values_of(hessian, point, weights), point.size());
        checked += check_product(rows, direction, product_of(hessian, point, weights, direction));
        entries += point.size();
    }
    EXPECT_GT(2 * checked, entries);
}

TEST(HessianTest, Direction)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 7);
    const std::array roots{wide(graph, x), unary(graph, Operator::Cos, x[3])};
    const Values point{0.9, 1.1, 0.8, 1.2, 0.7, 1.3, 1.05, 4.0};
    const Values direction{0.5, -1.0, 0.25, 2.0, -0.75, 1.5, -0.5, 3.0};
    const std::array weights{0.8, -1.5};
    for (const std::size_t lanes : {1U, 2U, 8U})
    {
        SCOPED_TRACE(lanes);
        Hessian hessian(graph, roots, {}, {env.stream(), tree}, lanes);
        const Rows rows = dense(hessian.pattern(), values_of(hessian, point, weights), 8);
        const Values found = product_of(hessian, point, weights, direction);
        EXPECT_EQ(check_product(rows, direction, found), 8U);
        EXPECT_EQ(std::bit_cast<Bits>(found[7]), std::bit_cast<Bits>(0.0));
    }
}

TEST(HessianTest, Declared)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const std::array roots{graph.add(Operator::Product, 1.0, std::array{x[1], x[2]})};
    const std::array declared{
        Entry{.row = 0, .column = 2},
        Entry{.row = 2, .column = 1},
        Entry{.row = 4, .column = 4},
    };
    Hessian hessian(graph, roots, declared, {env.stream(), tree});
    const Lower &lower = hessian.pattern();
    EXPECT_EQ(lower.starts, (std::vector<Offset>{0, 1, 3, 4, 4, 5}));
    EXPECT_EQ(lower.rows, (std::vector<Index>{2, 1, 2, 2, 4}));
    EXPECT_EQ(std::vector(hessian.declared().begin(), hessian.declared().end()),
              (std::vector<Offset>{0, 2, 4}));
    const Values out = values_of(hessian, std::array{1.0, 2.0, 3.0}, std::array{2.0});
    expect_same(out, {0.0, 0.0, 2.0, 0.0, 0.0});
}

TEST(HessianTest, Pattern)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 4);
    const std::array roots{
        graph.add_sum(0.0,
                      std::array{binary(graph, Operator::Division, x[3], x[0]),
                                 unary(graph, Operator::Exp, x[2])},
                      std::array{1.0, 1.0}),
        graph.add(Operator::Product, 1.0, std::array{x[3], x[1]}),
    };
    Hessian hessian(graph, roots, {}, {env.stream(), tree});
    const Lower &lower = hessian.pattern();
    EXPECT_EQ(lower.starts, (std::vector<Offset>{0, 2, 4, 5, 6}));
    EXPECT_EQ(lower.rows, (std::vector<Index>{0, 3, 1, 3, 2, 3}));
}

TEST(HessianTest, Widened)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const std::array roots{unary(graph, Operator::Exp, x[0])};
    Hessian hessian(graph, roots, {}, {env.stream(), tree});
    hessian.widen(3);
    EXPECT_EQ(hessian.pattern().starts, (std::vector<Offset>{0, 1, 1, 1}));
    EXPECT_EQ(hessian.pattern().rows, (std::vector<Index>{0}));
    hessian.widen(1);
    EXPECT_EQ(hessian.pattern().starts.size(), 4U);
    const Values out = values_of(hessian, std::array{0.0, 0.0, 0.0}, std::array{2.0});
    expect_same(out, {2.0});
}

TEST(HessianTest, Refused)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const std::array roots{graph.add(Operator::Product, 1.0, std::array{x[0], x[1]})};
    EXPECT_EQ(refusal([&] { Hessian(graph, roots, {}, {env.stream(), tree}, 0); }),
              "invalid argument: lanes: zero");
    Hessian hessian(graph, roots, {}, {env.stream(), tree});
    Values out(3);
    const std::array point{1.0, 2.0};
    const std::array weights{1.0};
    Pool pool;
    const Tolerances tolerances = default_tolerances();
    EXPECT_EQ(refusal([&] { hessian.values(std::array{1.0}, weights, tolerances, out, pool); }),
              "invalid argument: point: size 1 does not cover variable 1");
    EXPECT_EQ(refusal([&] { hessian.values(point, std::array{1.0, 1.0}, tolerances, out, pool); }),
              "invalid argument: weights: size 2, not the root count 1");
    EXPECT_EQ(
        refusal([&] { hessian.values(point, weights, tolerances, std::span(out).first(2), pool); }),
        "invalid argument: out: size 2, not the pattern size 3");
    const std::span<Real> two = std::span(out).first(2);
    EXPECT_EQ(
        refusal([&] { hessian.product(point, weights, std::array{1.0}, tolerances, two, pool); }),
        "invalid argument: direction: size 1, not the point size 2");
    const std::span<Real> one = std::span(out).first(1);
    EXPECT_EQ(refusal([&] { hessian.product(point, weights, point, tolerances, one, pool); }),
              "invalid argument: out: size 1, not the point size 2");
}

TEST(LaneTest, Widest)
{
    Env env;
    TimingTree tree;
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const std::array roots{graph.add(Operator::Product, 1.0, std::array{x[0], x[1]})};
    constexpr std::size_t kWidest = std::size_t{1}
                                    << (std::numeric_limits<std::size_t>::digits - 1);
    const std::string above = " above " + std::to_string(kWidest);
    for (const std::size_t lanes : {kWidest + 1, (std::numeric_limits<std::size_t>::max)()})
    {
        SCOPED_TRACE(lanes);
        EXPECT_EQ(refusal([&] { Hessian(graph, roots, {}, {env.stream(), tree}, lanes); }),
                  "invalid argument: lanes: " + std::to_string(lanes) + above);
    }
    Hessian widest(graph, roots, {}, {env.stream(), tree}, kWidest);
    Hessian usual(graph, roots, {}, {env.stream(), tree});
    const std::array point{1.5, -2.0};
    expect_same(values_of(widest, point, std::array{1.0}),
                values_of(usual, point, std::array{1.0}));
}
