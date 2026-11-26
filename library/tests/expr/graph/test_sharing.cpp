#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/sharing.hpp"
#include "graphs.hpp"
#include "reference.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <vector>

namespace
{

using biyos::Bits;
using biyos::Candidate;
using biyos::Graph;
using biyos::hash_of;
using biyos::Index;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::same;
using biyos::share;
using biyos::Sharing;
using biyos::slot_of;
using biyos::to_size;
using biyos::test::binary;
using biyos::test::bits;
using biyos::test::constant;
using biyos::test::evaluate;
using biyos::test::every_operator;
using biyos::test::ids;
using biyos::test::ordered;
using biyos::test::piecewise;
using biyos::test::Random;
using biyos::test::random_graph;
using biyos::test::random_point;
using biyos::test::unary;

constexpr std::array kUnit{0.0, 1.0};
constexpr int kSpread = 1024;
constexpr std::size_t kSlots = 2048;
constexpr std::size_t kDistinct = 700;

std::vector<Offset> identity(Offset size)
{
    std::vector<Offset> map;
    for (Offset node = 0; node < size; ++node)
    {
        map.push_back(node);
    }
    return map;
}

Offset image(const Sharing &result, Offset node)
{
    return result.map[to_size(node)];
}

void expect_same(const Graph &found, const Graph &graph)
{
    ASSERT_EQ(found.size(), graph.size());
    for (Offset node = 0; node < graph.size(); ++node)
    {
        EXPECT_EQ(found.op(node), graph.op(node));
        EXPECT_EQ(ids(found.children(node)), ids(graph.children(node)));
        EXPECT_EQ(bits(found.coefficients(node)), bits(graph.coefficients(node)));
        EXPECT_EQ(found.variable(node), graph.variable(node));
        EXPECT_EQ(found.piecewise_linear(node), graph.piecewise_linear(node));
        EXPECT_EQ(bits(found.constant(node)), bits(graph.constant(node)));
    }
}

std::vector<Real> values_of(const Graph &graph, std::span<const Real> point)
{
    std::vector<Real> values(to_size(graph.size()));
    evaluate(graph, point, values);
    return values;
}

void expect_values(const Sharing &result, const Graph &graph, std::span<const Real> point)
{
    const std::vector<Real> before = values_of(graph, point);
    const std::vector<Real> after = values_of(result.graph, point);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        EXPECT_EQ(bits(after[to_size(image(result, node))]), bits(before[to_size(node)]));
    }
}

Offset weighted(Graph &graph, Real constant, std::span<const Offset> terms, Real weight)
{
    const std::vector<Real> weights(terms.size(), weight);
    return graph.add_sum(constant, terms, weights);
}

struct Pair
{
    Offset left;
    Offset right;
};

Candidate candidate_of(const Graph &graph, Offset node)
{
    return {.graph = graph, .node = node, .children = graph.children(node)};
}

std::size_t spread(const Graph &graph)
{
    std::set<std::size_t> slots;
    for (Offset node = 0; node < graph.size(); ++node)
    {
        slots.insert(slot_of(hash_of(candidate_of(graph, node)), kSlots));
    }
    return slots.size();
}

Real doubling(int k)
{
    return std::ldexp(1.0, k - kSpread / 2);
}

Real tenth(int k)
{
    return k / 10.0;
}

Real whole(int k)
{
    return Real(k);
}

template <typename Value> Graph constants_of(Value value)
{
    Graph graph;
    for (int k = 0; k < kSpread; ++k)
    {
        constant(graph, value(k));
    }
    return graph;
}

Graph variables()
{
    Graph graph;
    for (Index k = 0; k < kSpread; ++k)
    {
        graph.add_variable(k);
    }
    return graph;
}

bool collides(const Graph &graph)
{
    std::set<Bits> hashes;
    for (Offset node = 0; node < graph.size(); ++node)
    {
        hashes.insert(hash_of(candidate_of(graph, node)));
    }
    return hashes.size() < to_size(graph.size());
}

std::vector<Pair> distinct_pairs(Graph &graph)
{
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::array xy{x, y};
    const std::array yx{y, x};
    const Real step = std::nextafter(2.0, 3.0);
    graph.add_piecewise_linear(kUnit, kUnit);
    graph.add_piecewise_linear(kUnit, kUnit);
    return {
        {x, y},
        {unary(graph, Operator::Sin, x), unary(graph, Operator::Cos, x)},
        {unary(graph, Operator::Exp, x), unary(graph, Operator::Exp, y)},
        {graph.add(Operator::Max, xy), graph.add(Operator::Max, yx)},
        {constant(graph, 0.0), constant(graph, -0.0)},
        {constant(graph, 2.0), constant(graph, step)},
        {weighted(graph, 0.0, xy, 1.0), weighted(graph, -0.0, xy, 1.0)},
        {weighted(graph, 1.0, xy, 0.0), weighted(graph, 1.0, xy, -0.0)},
        {graph.add(Operator::Product, 0.0, xy), graph.add(Operator::Product, -0.0, xy)},
        {graph.add(Operator::ConstantPower, 2.0, std::span(&x, 1)),
         graph.add(Operator::ConstantPower, step, std::span(&x, 1))},
        {piecewise(graph, 0, x), piecewise(graph, 1, x)},
    };
}

std::vector<Pair> equal_pairs(Graph &graph)
{
    const Offset x = graph.add_variable(0);
    const std::array xx{x, x};
    graph.add_piecewise_linear(kUnit, kUnit);
    return {
        {x, x},
        {unary(graph, Operator::Sin, x), unary(graph, Operator::Sin, x)},
        {constant(graph, -0.0), constant(graph, -0.0)},
        {weighted(graph, -0.0, xx, 1.5), weighted(graph, -0.0, xx, 1.5)},
        {graph.add(Operator::Product, 2.0, xx), graph.add(Operator::Product, 2.0, xx)},
        {piecewise(graph, 0, x), piecewise(graph, 0, x)},
    };
}

}

TEST(SharingTest, Subtrees)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset first = unary(graph, Operator::Sin, unary(graph, Operator::Exp, x));
    const Offset second = unary(graph, Operator::Sin, unary(graph, Operator::Exp, x));
    const Offset ratio = binary(graph, Operator::Division, first, second);
    const Sharing result = share(graph);
    ASSERT_EQ(result.graph.size(), 4);
    EXPECT_EQ(image(result, second), image(result, first));
    const std::vector<Offset> twice(2, image(result, first));
    EXPECT_EQ(ids(result.graph.children(image(result, ratio))), twice);
    EXPECT_TRUE(ordered(result.graph));
}

TEST(SharingTest, Commuted)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::array xy{x, y};
    const std::array yx{y, x};
    weighted(graph, 0.0, xy, 1.0);
    weighted(graph, 0.0, yx, 1.0);
    graph.add(Operator::Product, 1.0, xy);
    graph.add(Operator::Product, 1.0, yx);
    graph.add(Operator::Max, xy);
    graph.add(Operator::Max, yx);
    const Sharing result = share(graph);
    EXPECT_EQ(result.map, identity(graph.size()));
}

TEST(SharingTest, SignedZero)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    constant(graph, 0.0);
    constant(graph, -0.0);
    weighted(graph, 0.0, std::span(&x, 1), 1.0);
    weighted(graph, -0.0, std::span(&x, 1), 1.0);
    weighted(graph, 1.0, std::span(&x, 1), 0.0);
    weighted(graph, 1.0, std::span(&x, 1), -0.0);
    graph.add(Operator::Product, 0.0, std::span(&x, 1));
    graph.add(Operator::Product, -0.0, std::span(&x, 1));
    const Sharing result = share(graph);
    EXPECT_EQ(result.map, identity(graph.size()));
}

TEST(SharingTest, NextUlp)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Real step = std::nextafter(2.0, 3.0);
    constant(graph, 2.0);
    constant(graph, step);
    weighted(graph, 2.0, std::span(&x, 1), 1.0);
    weighted(graph, step, std::span(&x, 1), 1.0);
    weighted(graph, 1.0, std::span(&x, 1), 2.0);
    weighted(graph, 1.0, std::span(&x, 1), step);
    graph.add(Operator::Product, 2.0, std::span(&x, 1));
    graph.add(Operator::Product, step, std::span(&x, 1));
    graph.add(Operator::ConstantPower, 2.0, std::span(&x, 1));
    graph.add(Operator::ConstantPower, step, std::span(&x, 1));
    graph.add(Operator::SignPower, 2.0, std::span(&x, 1));
    graph.add(Operator::SignPower, step, std::span(&x, 1));
    const Sharing result = share(graph);
    EXPECT_EQ(result.map, identity(graph.size()));
}

TEST(SharingTest, Payloads)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::array xx{x, x};
    constant(graph, -0.0);
    constant(graph, -0.0);
    weighted(graph, 1.5, xx, -0.0);
    weighted(graph, 1.5, xx, -0.0);
    graph.add(Operator::Product, 3.0, xx);
    graph.add(Operator::Product, 3.0, xx);
    graph.add(Operator::SignPower, 1.5, std::span(&x, 1));
    graph.add(Operator::SignPower, 1.5, std::span(&x, 1));
    graph.add(Operator::Min, xx);
    graph.add(Operator::Min, xx);
    const Sharing result = share(graph);
    EXPECT_EQ(result.map, (std::vector<Offset>{0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5}));
    expect_same(share(result.graph).graph, result.graph);
}

TEST(SharingTest, Piecewise)
{
    Graph graph;
    graph.add_piecewise_linear(kUnit, kUnit);
    graph.add_piecewise_linear(kUnit, kUnit);
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    piecewise(graph, 0, x);
    piecewise(graph, 0, x);
    piecewise(graph, 1, x);
    piecewise(graph, 0, y);
    const Sharing result = share(graph);
    EXPECT_EQ(result.map, (std::vector<Offset>{0, 1, 2, 2, 3, 4}));
    EXPECT_EQ(result.graph.points().size(), 2);
}

TEST(SharingTest, Unchanged)
{
    Graph graph;
    every_operator(graph);
    const Sharing result = share(graph);
    EXPECT_EQ(result.map, identity(graph.size()));
    expect_same(result.graph, graph);
    EXPECT_EQ(result.graph.points().size(), graph.points().size());
}

TEST(SharingTest, Map)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset two = constant(graph, 2.0);
    const Offset y = graph.add_variable(1);
    const Offset ratio = binary(graph, Operator::Division, x, two);
    constant(graph, 2.0);
    binary(graph, Operator::Division, x, two);
    binary(graph, Operator::Division, x, y);
    const Offset again = binary(graph, Operator::Division, x, two);
    binary(graph, Operator::Power, ratio, again);
    const Sharing result = share(graph);
    const std::vector<Offset> wanted{0, 1, 2, 3, 1, 3, 4, 3, 5};
    EXPECT_EQ(result.map, wanted);
    EXPECT_EQ(result.graph.size(), 6);
}

TEST(SharingTest, Empty)
{
    const Sharing result = share(Graph());
    EXPECT_EQ(result.graph.size(), 0);
    EXPECT_TRUE(result.map.empty());
}

TEST(SharingTest, Values)
{
    Offset merged = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const Sharing result = share(graph);
        EXPECT_TRUE(ordered(result.graph));
        expect_same(share(result.graph).graph, result.graph);
        for (int k = 0; k < 4; ++k)
        {
            expect_values(result, graph, random_point(random));
        }
        merged += graph.size() - result.graph.size();
    }
    EXPECT_GT(merged, 0);
}

TEST(SharingTest, Equality)
{
    Graph distinct;
    for (const auto [left, right] : distinct_pairs(distinct))
    {
        EXPECT_FALSE(same(candidate_of(distinct, left), distinct, right));
        EXPECT_FALSE(same(candidate_of(distinct, right), distinct, left));
    }
    Graph equal;
    for (const auto [left, right] : equal_pairs(equal))
    {
        EXPECT_TRUE(same(candidate_of(equal, left), equal, right));
    }
}

TEST(SharingTest, Hash)
{
    Graph distinct;
    for (const auto [left, right] : distinct_pairs(distinct))
    {
        EXPECT_NE(hash_of(candidate_of(distinct, left)), hash_of(candidate_of(distinct, right)));
    }
    Graph equal;
    for (const auto [left, right] : equal_pairs(equal))
    {
        EXPECT_EQ(hash_of(candidate_of(equal, left)), hash_of(candidate_of(equal, right)));
    }
}

TEST(SharingTest, Collision)
{
    Graph graph;
    for (Index k = 0; k < 8; ++k)
    {
        graph.add_variable(k);
    }
    for (Bits k = 0; k < 256; ++k)
    {
        constant(graph, std::bit_cast<Real>(k));
    }
    ASSERT_TRUE(collides(graph));
    EXPECT_EQ(share(graph).map, identity(graph.size()));
}

TEST(SharingTest, Slot)
{
    const Bits top = Bits{1} << 63;
    EXPECT_EQ(slot_of(~Bits{0}, 1), 0);
    EXPECT_EQ(slot_of(top, 2), 1);
    EXPECT_EQ(slot_of(1, 2), 0);
    EXPECT_EQ(slot_of(top | (top >> 2), 8), 5);
}

TEST(SharingTest, Spread)
{
    EXPECT_GT(spread(constants_of(doubling)), kDistinct);
    EXPECT_GT(spread(constants_of(tenth)), kDistinct);
    EXPECT_GT(spread(constants_of(whole)), kDistinct);
    EXPECT_GT(spread(variables()), kDistinct);
}
