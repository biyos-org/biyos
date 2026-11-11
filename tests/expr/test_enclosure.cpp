#include "biyos/expr.h"
#include "bounds.hpp"
#include "core/index.hpp"
#include "expr/builder.hpp"
#include "expr/enclosure.hpp"
#include "expr/graph.hpp"
#include "expr/interval.hpp"
#include "expr/node.hpp"
#include "expr/rounding.hpp"
#include "reference.hpp"
#include "refusal.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Builder;
using biyos::Graph;
using biyos::Interval;
using biyos::Offset;
using biyos::Operator;
using biyos::test::Box;
using biyos::test::box_of;
using biyos::test::Check;
using biyos::test::enclosures;
using biyos::test::Engine;
using biyos::test::exact;
using biyos::test::expect_sampled;
using biyos::test::holds;
using biyos::test::operator_graph;
using biyos::test::Parameters;
using biyos::test::Point;
using biyos::test::refusal;
using biyos::test::text;
using biyos::test::within;

constexpr std::uint64_t kSeed = 20261111;
constexpr int kFirst = BIYOS_OP_CONSTANT;
constexpr int kLast = BIYOS_OP_ERF;
constexpr int kBoxes = 300;
constexpr int kExact = 0;
constexpr int kLevels = 64;
constexpr int kLevelSteps = 4;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

constexpr Parameters kFixed{
    .constant = 2.5,
    .weights = {0.1, 0.2, 0.3},
    .exponent = 1.5,
};

Offset rational(Graph &graph)
{
    Builder build(graph);
    const std::array one{1.0};
    build.variable(0);
    build.apply(Operator::Sin, 1);
    build.variable(1);
    build.product(1.0, 2);
    build.apply(Operator::Exp, 1);
    build.variable(0);
    build.power(2.0);
    build.sum(1.0, one);
    build.apply(Operator::Division, 2);
    return build.finish();
}

Offset distance(Graph &graph)
{
    Builder build(graph);
    const std::array difference{1.0, -1.0};
    const std::array one{1.0};
    const std::array both{1.0, 1.0};
    build.variable(0);
    build.variable(1);
    build.sum(0.0, difference);
    build.apply(Operator::Abs, 1);
    build.sum(1.0, one);
    build.apply(Operator::Log, 1);
    build.variable(0);
    build.variable(1);
    build.constant(0.0);
    build.apply(Operator::Max, 3);
    build.apply(Operator::Sqrt, 1);
    build.sum(0.0, both);
    return build.finish();
}

Offset smooth(Graph &graph)
{
    Builder build(graph);
    const std::array weights{1.0, -1.0, 1.0};
    build.variable(0);
    build.power(2.0);
    build.apply(Operator::Entropy, 1);
    build.variable(0);
    build.variable(1);
    build.product(1.0, 2);
    build.apply(Operator::Tanh, 1);
    build.variable(2);
    build.apply(Operator::Erf, 1);
    build.variable(0);
    build.apply(Operator::Cosh, 1);
    build.apply(Operator::Division, 2);
    build.sum(0.0, weights);
    return build.finish();
}

Offset tower(Graph &graph)
{
    Builder build(graph);
    const std::array one{1.0};
    build.variable(0);
    build.apply(Operator::Abs, 1);
    build.sum(1.0, one);
    build.variable(1);
    build.apply(Operator::Sin, 1);
    build.apply(Operator::Power, 2);
    build.variable(0);
    build.variable(1);
    build.apply(Operator::Division, 2);
    build.apply(Operator::Atan, 1);
    build.product(1.0, 2);
    return build.finish();
}

Offset inverse(Graph &graph)
{
    Builder build(graph);
    const std::array difference{1.0, -1.0};
    const std::array one{1.0};
    const std::array weights{1.0, -1.0, 1.0, 1.0};
    build.variable(0);
    build.variable(1);
    build.sum(0.0, difference);
    build.sign_power(1.5);
    build.variable(2);
    build.apply(Operator::Asinh, 1);
    build.variable(0);
    build.power(2.0);
    build.sum(1.0, one);
    build.apply(Operator::Acosh, 1);
    build.variable(1);
    build.apply(Operator::Tanh, 1);
    build.apply(Operator::Atanh, 1);
    build.sum(0.0, weights);
    return build.finish();
}

Offset circle(Graph &graph)
{
    Builder build(graph);
    const std::array tenth{0.1};
    const std::array weights{1.0, 1.0, 1.0, 1.0};
    build.variable(0);
    build.apply(Operator::Sin, 1);
    build.apply(Operator::Asin, 1);
    build.variable(1);
    build.apply(Operator::Cos, 1);
    build.apply(Operator::Acos, 1);
    build.variable(0);
    build.sum(0.0, tenth);
    build.apply(Operator::Tan, 1);
    build.variable(0);
    build.variable(1);
    build.variable(2);
    build.apply(Operator::Min, 3);
    build.sum(0.0, weights);
    return build.finish();
}

Offset shared(Graph &graph)
{
    Builder inner(graph);
    const std::array both{1.0, 1.0};
    const std::array one{1.0};
    const std::array weights{1.0, 1.0, -1.0};
    inner.variable(0);
    inner.apply(Operator::Sin, 1);
    inner.variable(1);
    inner.sum(0.0, both);
    const Offset common = inner.finish();
    Builder build(graph);
    build.node(common);
    build.node(common);
    build.product(1.0, 2);
    build.node(common);
    build.apply(Operator::Exp, 1);
    build.node(common);
    build.node(common);
    build.power(2.0);
    build.sum(1.0, one);
    build.apply(Operator::Division, 2);
    build.sum(0.0, weights);
    return build.finish();
}

using Composite = Offset (*)(Graph &);

constexpr std::array<Composite, 7> kComposites{
    rational,
    distance,
    smooth,
    tower,
    inverse,
    circle,
    shared,
};

Interval point_box(double value)
{
    return {value, value};
}

void expect_margin(Operator op, const Point &point, int steps)
{
    const Graph graph = operator_graph(op, kFixed);
    const std::array box{point_box(point[0]), point_box(point[1]), point_box(point[2])};
    const std::vector<Interval> nodes = enclosures(graph, box);
    std::vector<double> values(nodes.size());
    biyos::test::evaluate(graph, point, values);
    const Interval found = nodes.back();
    const double value = values.back();
    SCOPED_TRACE("operator " + std::to_string(static_cast<int>(op)) + " at " + text(point[0]));
    EXPECT_LE(found.lower(), biyos::shifted(value, -steps));
    EXPECT_GE(found.upper(), biyos::shifted(value, steps));
    if (steps == kExact)
    {
        EXPECT_GE(found.lower(), value);
        EXPECT_LE(found.upper(), value);
    }
}

}

TEST(IntervalTest, Sampled)
{
    for (int number = kFirst; number <= kLast; ++number)
    {
        const auto op = static_cast<Operator>(number);
        SCOPED_TRACE("operator " + std::to_string(number));
        Engine engine(kSeed + static_cast<std::uint64_t>(number));
        int reported = 0;
        for (int round = 0; round < kBoxes; ++round)
        {
            const Graph graph = operator_graph(op, biyos::test::parameters_of(op, engine));
            const Box box = box_of(engine);
            const std::vector<Interval> nodes = enclosures(graph, box);
            const Check check{.graph = graph, .box = box, .nodes = nodes, .reported = reported};
            expect_sampled(check, engine);
        }
    }
}

TEST(IntervalTest, Composed)
{
    Engine engine(kSeed);
    int reported = 0;
    for (std::size_t k = 0; k < kComposites.size(); ++k)
    {
        SCOPED_TRACE("composite " + std::to_string(k));
        for (int round = 0; round < kBoxes; ++round)
        {
            Graph graph;
            kComposites[k](graph);
            const Box box = box_of(engine);
            const std::vector<Interval> nodes = enclosures(graph, box);
            const Check check{.graph = graph, .box = box, .nodes = nodes, .reported = reported};
            expect_sampled(check, engine);
        }
    }
}

TEST(IntervalTest, EmptyBox)
{
    const std::array box{Interval::empty(), Interval::empty(), Interval::empty()};
    for (int number = kFirst; number <= kLast; ++number)
    {
        const auto op = static_cast<Operator>(number);
        const Graph graph = operator_graph(op, kFixed);
        const std::vector<Interval> nodes = enclosures(graph, box);
        for (Offset node = 0; node < graph.size(); ++node)
        {
            const bool constant = graph.op(node) == Operator::Constant;
            EXPECT_EQ(nodes[static_cast<std::size_t>(node)].is_empty(), !constant) << number;
        }
    }
}

TEST(IntervalTest, Margin)
{
    using biyos::kLibmSteps;
    using biyos::kRoundedSteps;
    expect_margin(Operator::Constant, {0.0}, kExact);
    expect_margin(Operator::Variable, {1.25, 2.5, 3.75}, kExact);
    expect_margin(Operator::Sum, {1.0, 1.0, 1.0}, kRoundedSteps);
    expect_margin(Operator::Product, {0.1, 0.7, 1.3}, kRoundedSteps);
    expect_margin(Operator::Negation, {1.5}, kExact);
    expect_margin(Operator::Division, {1.0, 3.0}, kRoundedSteps);
    expect_margin(Operator::ConstantPower, {2.0}, kLibmSteps);
    expect_margin(Operator::Power, {2.0, 0.5}, kLibmSteps);
    expect_margin(Operator::Exp, {1.0}, kLibmSteps);
    expect_margin(Operator::Log, {2.0}, kLibmSteps);
    expect_margin(Operator::Sqrt, {2.0}, kRoundedSteps);
    expect_margin(Operator::Sin, {1.0}, kLibmSteps);
    expect_margin(Operator::Cos, {1.0}, kLibmSteps);
    expect_margin(Operator::Tan, {1.0}, kLibmSteps);
    expect_margin(Operator::Abs, {-2.0}, kExact);
    expect_margin(Operator::Max, {1.0, 3.0, 2.0}, kExact);
    expect_margin(Operator::Min, {2.0, 1.0, 3.0}, kExact);
    expect_margin(Operator::Asin, {0.5}, kLibmSteps);
    expect_margin(Operator::Acos, {0.5}, kLibmSteps);
    expect_margin(Operator::Atan, {2.0}, kLibmSteps);
    expect_margin(Operator::Sinh, {1.0}, kLibmSteps);
    expect_margin(Operator::Cosh, {1.0}, kLibmSteps);
    expect_margin(Operator::Tanh, {0.5}, kLibmSteps);
    expect_margin(Operator::Asinh, {1.0}, kLibmSteps);
    expect_margin(Operator::Acosh, {2.0}, kLibmSteps);
    expect_margin(Operator::Atanh, {0.5}, kLibmSteps);
    expect_margin(Operator::Entropy, {2.0}, kLibmSteps);
    expect_margin(Operator::SignPower, {-2.0}, kLibmSteps);
    expect_margin(Operator::Erf, {0.3}, kLibmSteps);
}

TEST(IntervalTest, Propagated)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset root = graph.add(Operator::Log, std::span(&x, 1));
    const Offset grown = graph.add(Operator::Exp, std::span(&root, 1));
    const std::array terms{grown, x};
    const std::array weights{1.0, 1.0};
    graph.add_sum(0.0, terms, weights);
    const std::array box{Interval(-2.0, -1.0)};
    const std::vector<Interval> nodes = enclosures(graph, box);
    EXPECT_FALSE(nodes[0].is_empty());
    EXPECT_TRUE(nodes[1].is_empty());
    EXPECT_TRUE(nodes[2].is_empty());
    EXPECT_TRUE(nodes[3].is_empty());
}

TEST(IntervalTest, ZeroWeights)
{
    Graph graph;
    const std::array x{graph.add_variable(0), graph.add_variable(1)};
    const std::array weights{1.0, 0.0};
    const Offset sum = graph.add_sum(3.0, x, weights);
    const Offset product = graph.add(Operator::Product, 2.0, x);
    const Offset scaled = graph.add(Operator::Product, 0.0, std::span(&x[1], 1));
    const std::array box{Interval(0.0, 0.0), Interval::entire()};
    std::vector<Interval> nodes = enclosures(graph, box);
    EXPECT_TRUE(exact(nodes[static_cast<std::size_t>(sum)], 3.0, 3.0));
    EXPECT_TRUE(exact(nodes[static_cast<std::size_t>(product)], 0.0, 0.0));
    EXPECT_TRUE(exact(nodes[static_cast<std::size_t>(scaled)], 0.0, 0.0));
    const std::array half{Interval(-kInfinity, 0.0), Interval(1.0, 2.0)};
    nodes = enclosures(graph, half);
    EXPECT_TRUE(exact(nodes[static_cast<std::size_t>(sum)], -kInfinity, 3.0));
}

TEST(IntervalTest, Shared)
{
    Graph graph;
    Offset node = graph.add_variable(0);
    const std::array weights{1.0, 1.0};
    for (int level = 0; level < kLevels; ++level)
    {
        const std::array terms{node, node};
        node = graph.add_sum(0.0, terms, weights);
    }
    ASSERT_EQ(graph.size(), kLevels + 1);
    const std::array box{Interval(1.0, 2.0)};
    const std::vector<Interval> nodes = enclosures(graph, box);
    for (int level = 0; level <= kLevels; ++level)
    {
        const double low = std::ldexp(1.0, level);
        const double high = 2 * low;
        const Interval found = nodes[static_cast<std::size_t>(level)];
        const int steps = kLevelSteps * level;
        EXPECT_TRUE(holds(found, low, high)) << level;
        EXPECT_TRUE(within(found, biyos::shifted(low, -steps), biyos::shifted(high, steps)))
            << level;
    }
}

TEST(IntervalTest, Sizes)
{
    Graph graph;
    const Offset x = graph.add_variable(3);
    graph.add(Operator::Exp, std::span(&x, 1));
    const std::vector<Interval> box(4, Interval(0.0, 1.0));
    std::vector<Interval> small(1, Interval::empty());
    EXPECT_EQ(refusal([&] { biyos::enclose(graph, box, small); }),
              "invalid argument: nodes: size 1, not the node count 2");
    std::vector<Interval> nodes(2, Interval::empty());
    const std::array short_box{Interval(0.0, 1.0)};
    EXPECT_EQ(refusal([&] { biyos::enclose(graph, short_box, nodes); }),
              "invalid argument: box: size 1 does not cover variable 3");
    EXPECT_TRUE(nodes[0].is_empty());
    EXPECT_TRUE(nodes[1].is_empty());
    biyos::enclose(graph, box, nodes);
    EXPECT_TRUE(holds(nodes[1], 1.0, std::exp(1.0)));
}
