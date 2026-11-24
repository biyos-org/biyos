#include "core/index.hpp"
#include "expr/derivatives/reverse.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>

namespace
{

using biyos::Adjoints;
using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Tape;
using biyos::Workspace;
using biyos::test::refusal;
using biyos::test::unary;

std::size_t at(Offset value)
{
    return static_cast<std::size_t>(value);
}

}

TEST(LaneTest, Sizes)
{
    Graph graph;
    const Offset shared = unary(graph, Operator::Exp, graph.add_variable(1));
    const std::array roots{
        unary(graph, Operator::Sin, shared),
        unary(graph, Operator::Cos, shared),
    };
    const Tape tape(graph, roots, 3);
    ASSERT_EQ(tape.pinned(), 1);
    const Workspace workspace(tape, 3);
    EXPECT_EQ(workspace.lanes, 3U);
    EXPECT_EQ(workspace.shared.size(), 3 * at(tape.pinned()));
    EXPECT_EQ(workspace.slots.size(), 3 * tape.width());
    EXPECT_EQ(workspace.partials.size(), 3 * at(tape.partials()));
    const Adjoints adjoints(tape, 3);
    EXPECT_EQ(adjoints.lanes, 3U);
    EXPECT_EQ(adjoints.slots.size(), 3 * tape.width());
    EXPECT_EQ(adjoints.shared.size(), 3 * at(tape.pinned()));
    EXPECT_EQ(adjoints.gradient.size(), 3 * at(tape.variables()));
    EXPECT_EQ(Workspace(tape).lanes, 1U);
    EXPECT_EQ(Adjoints(tape).lanes, 1U);
    EXPECT_EQ(refusal([&] { Workspace{tape, 0}; }), "invalid argument: lanes: zero");
    EXPECT_EQ(refusal([&] { Adjoints{tape, 0}; }), "invalid argument: lanes: zero");
}
