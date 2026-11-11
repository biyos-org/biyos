#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "expr/parents.hpp"

#include <gtest/gtest.h>

#include <array>
#include <span>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Parents;

std::vector<Offset> ids(std::span<const Offset> nodes)
{
    return {nodes.begin(), nodes.end()};
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

TEST(ParentTest, SharedNode)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset shared = unary(graph, Operator::Exp, x);
    const Offset sine = unary(graph, Operator::Sin, shared);
    const Offset cosine = unary(graph, Operator::Cos, shared);
    const Parents parents(graph);
    EXPECT_EQ(parents.size(), 4);
    EXPECT_EQ(ids(parents.of(x)), std::vector<Offset>{shared});
    EXPECT_EQ(ids(parents.of(shared)), (std::vector<Offset>{sine, cosine}));
}

TEST(ParentTest, Roots)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset ratio = binary(graph, Operator::Division, x, y);
    const Offset lone = graph.add(Operator::Constant, 2.0, {});
    const Parents parents(graph);
    EXPECT_TRUE(parents.of(ratio).empty());
    EXPECT_TRUE(parents.of(lone).empty());
    EXPECT_EQ(ids(parents.of(y)), std::vector<Offset>{ratio});
}

TEST(ParentTest, RepeatedChild)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset ratio = binary(graph, Operator::Division, x, x);
    const Parents parents(graph);
    EXPECT_EQ(ids(parents.of(x)), (std::vector<Offset>{ratio, ratio}));
}

TEST(ParentTest, EmptyGraph)
{
    const Graph graph;
    const Parents parents(graph);
    EXPECT_EQ(parents.size(), 0);
}

TEST(ParentTest, LaterNodes)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Parents parents(graph);
    unary(graph, Operator::Exp, x);
    EXPECT_EQ(parents.size(), 1);
    EXPECT_TRUE(parents.of(x).empty());
}
