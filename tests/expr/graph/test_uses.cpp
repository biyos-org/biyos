#include "core/index.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/uses.hpp"

#include <gtest/gtest.h>

#include <array>
#include <span>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Uses;

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

TEST(UseTest, SharedNode)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset shared = unary(graph, Operator::Exp, x);
    const Offset sine = unary(graph, Operator::Sin, shared);
    const Offset cosine = unary(graph, Operator::Cos, shared);
    const Uses uses(graph);
    EXPECT_EQ(uses.size(), 4);
    EXPECT_FALSE(uses.shared(x));
    EXPECT_TRUE(uses.shared(shared));
    EXPECT_FALSE(uses.shared(sine));
    EXPECT_FALSE(uses.shared(cosine));
}

TEST(UseTest, RepeatedChild)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset ratio = binary(graph, Operator::Division, x, x);
    const Offset power = binary(graph, Operator::Power, y, ratio);
    const Uses uses(graph);
    EXPECT_TRUE(uses.shared(x));
    EXPECT_FALSE(uses.shared(y));
    EXPECT_FALSE(uses.shared(ratio));
    EXPECT_FALSE(uses.shared(power));
}

TEST(UseTest, EmptyGraph)
{
    const Graph graph;
    const Uses uses(graph);
    EXPECT_EQ(uses.size(), 0);
    EXPECT_EQ(Uses().size(), 0);
}

TEST(UseTest, Extended)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset exponential = unary(graph, Operator::Exp, x);
    Uses uses(graph);
    const Offset sine = unary(graph, Operator::Sin, exponential);
    EXPECT_EQ(uses.size(), 2);
    uses.extend(graph);
    EXPECT_EQ(uses.size(), 3);
    EXPECT_FALSE(uses.shared(exponential));
    unary(graph, Operator::Cos, exponential);
    unary(graph, Operator::Tan, sine);
    uses.extend(graph);
    EXPECT_EQ(uses.size(), 5);
    EXPECT_FALSE(uses.shared(x));
    EXPECT_TRUE(uses.shared(exponential));
    EXPECT_FALSE(uses.shared(sine));
}

TEST(UseTest, ManyParents)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    for (int k = 0; k < 256; ++k)
    {
        unary(graph, Operator::Exp, x);
    }
    unary(graph, Operator::Exp, y);
    Uses uses(graph);
    EXPECT_TRUE(uses.shared(x));
    for (int k = 0; k < 255; ++k)
    {
        unary(graph, Operator::Sin, y);
    }
    uses.extend(graph);
    EXPECT_TRUE(uses.shared(y));
}
