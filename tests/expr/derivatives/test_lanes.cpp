#include "core/index.hpp"
#include "expr/derivatives/reverse.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "refusal.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Adjoints;
using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Tape;
using biyos::Workspace;
using biyos::test::every_operator;
using biyos::test::kBudgets;
using biyos::test::kRoots;
using biyos::test::picks;
using biyos::test::Random;
using biyos::test::random_graph;
using biyos::test::random_point;
using biyos::test::refusal;
using biyos::test::unary;
using biyos::test::values_of;

using Points = std::vector<std::vector<double>>;

constexpr std::array kLanes{std::size_t{1}, std::size_t{3}, std::size_t{16}};

std::size_t at(Offset value)
{
    return static_cast<std::size_t>(value);
}

Points random_points(Random &random, std::size_t n)
{
    Points points;
    for (std::size_t l = 0; l < n; ++l)
    {
        points.push_back(random_point(random));
    }
    return points;
}

std::vector<double> interleave(const Points &points)
{
    const std::size_t lanes = points.size();
    std::vector<double> items(points[0].size() * lanes);
    for (std::size_t l = 0; l < lanes; ++l)
    {
        for (std::size_t k = 0; k < points[l].size(); ++k)
        {
            items[k * lanes + l] = points[l][k];
        }
    }
    return items;
}

std::vector<double> lane(std::span<const double> items, std::size_t l, std::size_t lanes)
{
    std::vector<double> mine;
    for (std::size_t k = l; k < items.size(); k += lanes)
    {
        mine.push_back(items[k]);
    }
    return mine;
}

void expect_bits(std::span<const double> found, std::span<const double> wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        SCOPED_TRACE(k);
        if (std::isnan(wanted[k]))
        {
            EXPECT_TRUE(std::isnan(found[k])) << found[k];
            continue;
        }
        EXPECT_EQ(std::bit_cast<std::uint64_t>(found[k]), std::bit_cast<std::uint64_t>(wanted[k]))
            << found[k] << " against " << wanted[k];
    }
}

void expect_values(const Tape &tape, Workspace &workspace, const Points &points)
{
    const std::size_t lanes = points.size();
    std::vector<double> values(at(tape.roots()) * lanes);
    biyos::forward(tape, interleave(points), workspace, values);
    for (std::size_t l = 0; l < lanes; ++l)
    {
        SCOPED_TRACE(l);
        expect_bits(lane(values, l, lanes), values_of(tape, points[l]));
    }
}

std::string refused(const Tape &tape,
                    Workspace &workspace,
                    std::span<const double> point,
                    std::span<double> values)
{
    return refusal([&] { biyos::forward(tape, point, workspace, values); });
}

Tape shared_tape(Graph &graph)
{
    const Offset shared = unary(graph, Operator::Exp, graph.add_variable(1));
    const std::array roots{
        unary(graph, Operator::Sin, shared),
        unary(graph, Operator::Cos, shared),
    };
    return {graph, roots, 3};
}

}

TEST(LaneTest, Sizes)
{
    Graph graph;
    const Tape tape = shared_tape(graph);
    ASSERT_EQ(tape.pinned(), 1);
    const Workspace workspace(tape, 3);
    EXPECT_EQ(workspace.lanes, 3U);
    EXPECT_EQ(workspace.shared.size(), 3 * at(tape.pinned()));
    EXPECT_EQ(workspace.slots.size(), 3 * tape.width());
    EXPECT_EQ(workspace.partials.size(), 3 * at(tape.partials()));
    EXPECT_EQ(workspace.result.size(), 3U);
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

TEST(LaneTest, Values)
{
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kRoots);
        const Tape tape(graph, roots, kBudgets[random.below(kBudgets.size())]);
        for (const std::size_t lanes : kLanes)
        {
            SCOPED_TRACE(lanes);
            Workspace workspace(tape, lanes);
            expect_values(tape, workspace, random_points(random, lanes));
            expect_values(tape, workspace, random_points(random, lanes));
        }
    }
}

TEST(LaneTest, Operators)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    const Tape tape(graph, roots);
    Random random(5);
    for (const std::size_t lanes : kLanes)
    {
        SCOPED_TRACE(lanes);
        Workspace workspace(tape, lanes);
        for (int k = 0; k < 4; ++k)
        {
            expect_values(tape, workspace, random_points(random, lanes));
        }
    }
}

TEST(LaneTest, Refused)
{
    Graph graph;
    const Tape tape = shared_tape(graph);
    Workspace workspace(tape, 3);
    std::vector<double> values(6);
    const std::vector<double> point(6, 0.5);
    EXPECT_EQ(refused(tape, workspace, std::span(point).first(5), values),
              "invalid argument: point: size 5 is not a multiple of 3 lanes");
    EXPECT_EQ(refused(tape, workspace, std::span(point).first(3), values),
              "invalid argument: point: size 3 does not cover variable 1 in 3 lanes");
    EXPECT_EQ(refused(tape, workspace, point, std::span(values).first(2)),
              "invalid argument: values: size 2, not the root count 2 in 3 lanes");
    std::vector<double> more(7);
    EXPECT_EQ(refused(tape, workspace, point, more),
              "invalid argument: values: size 7, not the root count 2 in 3 lanes");
    const std::string other = "invalid argument: workspace: sized for another tape";
    workspace.result.pop_back();
    EXPECT_EQ(refused(tape, workspace, point, values), other);
    workspace = Workspace(tape);
    workspace.lanes = 3;
    EXPECT_EQ(refused(tape, workspace, point, values), other);
    workspace = Workspace(tape, 3);
    workspace.slots.pop_back();
    EXPECT_EQ(refused(tape, workspace, point, values), other);
    workspace = Workspace(tape, 3);
    workspace.shared.pop_back();
    EXPECT_EQ(refused(tape, workspace, point, values), other);
    workspace.lanes = 0;
    EXPECT_EQ(refused(tape, workspace, point, values), "invalid argument: lanes: zero");
}
