#include "core/index.hpp"
#include "expr/compact.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace
{

using biyos::compact;
using biyos::Compaction;
using biyos::Graph;
using biyos::kDead;
using biyos::live;
using biyos::Offset;
using biyos::Operator;
using biyos::test::refusal;

std::size_t at(Offset value)
{
    return static_cast<std::size_t>(value);
}

std::vector<Offset> ids(std::span<const Offset> nodes)
{
    return {nodes.begin(), nodes.end()};
}

std::vector<Offset> mapped(std::span<const Offset> nodes, const std::vector<Offset> &map)
{
    std::vector<Offset> images;
    for (const Offset node : nodes)
    {
        images.push_back(map[at(node)]);
    }
    return images;
}

std::vector<Offset> kept(const std::vector<Offset> &map)
{
    std::vector<Offset> images;
    for (const Offset image : map)
    {
        if (image != kDead)
        {
            images.push_back(image);
        }
    }
    return images;
}

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

std::vector<std::uint64_t> bits(std::span<const double> values)
{
    std::vector<std::uint64_t> words;
    for (const double value : values)
    {
        words.push_back(bits(value));
    }
    return words;
}

void expect_copy(const Compaction &result, const Graph &graph, Offset node)
{
    const Graph &found = result.graph;
    const Offset image = result.map[at(node)];
    ASSERT_NE(image, kDead);
    EXPECT_EQ(found.op(image), graph.op(node));
    EXPECT_EQ(ids(found.children(image)), mapped(graph.children(node), result.map));
    EXPECT_EQ(bits(found.coefficients(image)), bits(graph.coefficients(node)));
    if (graph.op(node) == Operator::Variable)
    {
        EXPECT_EQ(found.variable(image), graph.variable(node));
        return;
    }
    EXPECT_EQ(bits(found.constant(image)), bits(graph.constant(node)));
}

void expect_order(const Graph &graph)
{
    for (Offset node = 0; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            EXPECT_LT(child, node);
        }
    }
}

Offset unary(Graph &graph, Operator op, Offset child)
{
    return graph.add(op, std::span(&child, 1));
}

Offset binary(Graph &graph, Operator op, Offset left, Offset right)
{
    const std::array children{left, right};
    return graph.add(op, children);
}

}

TEST(CompactTest, Marks)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset shared = unary(graph, Operator::Exp, x);
    const Offset sine = unary(graph, Operator::Sin, y);
    const Offset root = unary(graph, Operator::Cos, shared);
    unary(graph, Operator::Cos, sine);
    const Offset z = graph.add_variable(2);
    const std::array roots{z, root, z};
    const std::vector<bool> wanted{true, false, true, false, true, false, true};
    EXPECT_EQ(live(graph, roots), wanted);
}

TEST(CompactTest, MissingRoot)
{
    Graph graph;
    graph.add_variable(0);
    const Offset ahead = 1;
    const Offset negative = -1;
    EXPECT_EQ(refusal([&] { live(graph, std::span(&ahead, 1)); }),
              "invalid argument: expression: node 1 is unknown");
    EXPECT_EQ(refusal([&] { live(graph, std::span(&negative, 1)); }),
              "invalid argument: expression: node -1 is unknown");
}

TEST(CompactTest, DeadNodes)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset z = graph.add_variable(2);
    const double four = 4.0;
    const Offset unused = graph.add_sum(5.0, std::span(&z, 1), std::span(&four, 1));
    const Offset exponential = unary(graph, Operator::Exp, x);
    const std::array terms{exponential, y};
    const std::array weights{2.0, -1.0};
    const Offset sum = graph.add_sum(1.0, terms, weights);
    const std::array factors{unused, x};
    const Offset product = graph.add(Operator::Product, 3.0, factors);
    const Offset power = graph.add(Operator::ConstantPower, 2.5, std::span(&x, 1));
    const std::array roots{sum, power};
    const Compaction result = compact(graph, roots);
    ASSERT_EQ(result.graph.size(), 5);
    for (const Offset node : {z, unused, product})
    {
        EXPECT_EQ(result.map[at(node)], kDead);
    }
    for (const Offset node : {x, y, exponential, sum, power})
    {
        expect_copy(result, graph, node);
    }
}

TEST(CompactTest, Order)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset exponential = unary(graph, Operator::Exp, x);
    const Offset y = graph.add_variable(1);
    unary(graph, Operator::Sin, y);
    const Offset ratio = binary(graph, Operator::Division, y, exponential);
    const std::array roots{ratio, exponential};
    const Compaction result = compact(graph, roots);
    EXPECT_EQ(kept(result.map), (std::vector<Offset>{0, 1, 2, 3}));
    expect_order(result.graph);
}

TEST(CompactTest, SharedNode)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset shared = unary(graph, Operator::Exp, x);
    unary(graph, Operator::Tan, shared);
    const Offset sine = unary(graph, Operator::Sin, shared);
    const Offset cosine = unary(graph, Operator::Cos, shared);
    const Offset ratio = binary(graph, Operator::Division, sine, cosine);
    const Compaction result = compact(graph, std::span(&ratio, 1));
    ASSERT_EQ(result.graph.size(), 5);
    const std::vector<Offset> image{result.map[at(shared)]};
    EXPECT_EQ(ids(result.graph.children(result.map[at(sine)])), image);
    EXPECT_EQ(ids(result.graph.children(result.map[at(cosine)])), image);
}

TEST(CompactTest, Map)
{
    Graph graph;
    const Offset two = graph.add(Operator::Constant, 2.0, {});
    const Offset x = graph.add_variable(0);
    const Offset dead = unary(graph, Operator::Exp, two);
    const Offset y = graph.add_variable(1);
    const Offset ratio = binary(graph, Operator::Division, x, y);
    unary(graph, Operator::Cos, dead);
    const Compaction result = compact(graph, std::span(&ratio, 1));
    const std::vector<Offset> wanted{kDead, 0, kDead, 1, 2, kDead};
    EXPECT_EQ(result.map, wanted);
}

TEST(CompactTest, Unchanged)
{
    Graph graph;
    const Offset two = graph.add(Operator::Constant, 2.5, {});
    const Offset x = graph.add_variable(3);
    const std::array terms{two, x};
    const std::array weights{2.0, -0.0};
    const Offset sum = graph.add_sum(1.5, terms, weights);
    const std::array factors{x, sum};
    const Offset product = graph.add(Operator::Product, 4.0, factors);
    const Offset power = graph.add(Operator::ConstantPower, 3.0, std::span(&product, 1));
    const Offset sign = graph.add(Operator::SignPower, 1.5, std::span(&power, 1));
    const Offset error = unary(graph, Operator::Erf, sign);
    const Offset ratio = binary(graph, Operator::Division, error, x);
    const Compaction result = compact(graph, std::span(&ratio, 1));
    ASSERT_EQ(result.graph.size(), graph.size());
    for (Offset node = 0; node < graph.size(); ++node)
    {
        EXPECT_EQ(result.map[at(node)], node);
        expect_copy(result, graph, node);
    }
}

TEST(CompactTest, NoRoots)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    unary(graph, Operator::Exp, x);
    const Compaction result = compact(graph, {});
    EXPECT_EQ(result.graph.size(), 0);
    EXPECT_EQ(result.map, (std::vector<Offset>{kDead, kDead}));
    const Compaction empty = compact(Graph(), {});
    EXPECT_EQ(empty.graph.size(), 0);
    EXPECT_TRUE(empty.map.empty());
}

TEST(CompactTest, SameGraph)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const double one = 1.0;
    const Offset sum = graph.add_sum(0.0, std::span(&x, 1), std::span(&one, 1));
    EXPECT_EQ(refusal([&] { biyos::copy(graph, sum, std::span(&x, 1), graph); }),
              "invalid argument: expression: source and target are the same graph");
    EXPECT_EQ(graph.size(), 2);
}

TEST(CompactTest, VariableArity)
{
    Graph source;
    const Offset x = source.add_variable(0);
    Graph target;
    const Offset y = target.add_variable(1);
    EXPECT_EQ(refusal([&] { biyos::copy(source, x, std::span(&y, 1), target); }),
              "invalid argument: expression: operator 2 takes 0 children, not 1");
    EXPECT_EQ(target.size(), 1);
}
