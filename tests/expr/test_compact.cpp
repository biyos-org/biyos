#include "core/index.hpp"
#include "expr/compact.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <span>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::live;
using biyos::Offset;
using biyos::Operator;
using biyos::test::refusal;

Offset unary(Graph &graph, Operator op, Offset child)
{
    return graph.add(op, std::span(&child, 1));
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
