#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/plan.hpp"
#include "graphs.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Index;
using biyos::kUnset;
using biyos::Offset;
using biyos::Operator;
using biyos::Reach;
using biyos::reach_of;
using biyos::to_index;
using biyos::to_offset;
using biyos::to_size;
using biyos::test::cone_of;
using biyos::test::kBatchRoots;
using biyos::test::pairs_of;
using biyos::test::picks;
using biyos::test::Random;
using biyos::test::Ranges;
using biyos::test::unary;

constexpr Index kNames = 20;
constexpr Offset kGraphSize = 1500;
constexpr std::size_t kArity = 4;
constexpr std::uint64_t kSeeds = 8;

std::vector<Offset> children_of(Random &random, Offset size)
{
    const std::size_t window = 1 + random.below(to_size(size));
    std::vector<Offset> children(1 + random.below(kArity));
    for (Offset &child : children)
    {
        child = size - 1 - to_offset(random.below(window));
    }
    return children;
}

Graph windowed(Random &random)
{
    Graph graph;
    for (Index k = 0; k < kNames; ++k)
    {
        graph.add_variable(k);
    }
    while (graph.size() < kGraphSize)
    {
        const std::vector<Offset> children = children_of(random, graph.size());
        if (children.size() == 1)
        {
            graph.add(Operator::Sin, children);
            continue;
        }
        graph.add_sum(0.0, children, std::vector(children.size(), 1.0));
    }
    return graph;
}

std::vector<Offset> ladder(Graph &graph)
{
    std::vector<Offset> rungs{graph.add_variable(0)};
    while (rungs.size() < kBatchRoots)
    {
        const Offset wave = unary(graph, Operator::Sin, graph.add_variable(to_index(rungs.size())));
        const std::array steps{rungs.back(), wave};
        rungs.push_back(graph.add_sum(0.0, steps, std::array{1.0, 1.0}));
    }
    return rungs;
}

std::vector<Offset> waves(Graph &graph, std::span<const Offset> rungs)
{
    std::vector<Offset> roots;
    for (const Offset rung : rungs)
    {
        roots.push_back(unary(graph, Operator::Sin, rung));
    }
    return roots;
}

std::vector<Offset> spaced(const Graph &graph)
{
    std::vector<Offset> positions;
    for (Offset node = 0; node < graph.size(); ++node)
    {
        positions.push_back(2 * node);
    }
    return positions;
}

std::vector<Offset> touching(const Graph &graph)
{
    std::vector<Offset> positions = spaced(graph);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (graph.op(node) != Operator::Variable)
        {
            ++positions[to_size(node)];
        }
    }
    return positions;
}

std::vector<Offset> shuffled(Random &random, const Graph &graph)
{
    std::vector<Offset> positions(to_size(graph.size()));
    std::iota(positions.begin(), positions.end(), Offset{0});
    for (std::size_t k = positions.size(); k > 1; --k)
    {
        std::swap(positions[k - 1], positions[random.below(k)]);
    }
    for (Offset &position : positions)
    {
        if (random.below(4) == 0)
        {
            position = kUnset;
        }
    }
    return positions;
}

Ranges runs_of(std::vector<Offset> positions)
{
    std::ranges::sort(positions);
    Ranges runs;
    for (const Offset position : positions)
    {
        if (!runs.empty() && runs.back().second == position)
        {
            ++runs.back().second;
        }
        else
        {
            runs.emplace_back(position, position + 1);
        }
    }
    return runs;
}

template <typename T>
std::vector<T> part(const std::vector<T> &items, std::span<const Offset> starts, std::size_t k)
{
    return {items.begin() + starts[k], items.begin() + starts[k + 1]};
}

void expect_cone(const Graph &graph,
                 std::span<const Offset> positions,
                 Offset root,
                 const Reach &reach,
                 std::size_t k)
{
    std::vector<Index> variables;
    std::vector<Offset> reached;
    for (const Offset node : cone_of(graph, root))
    {
        if (graph.op(node) == Operator::Variable)
        {
            variables.push_back(graph.variable(node));
        }
        if (positions[to_size(node)] != kUnset)
        {
            reached.push_back(positions[to_size(node)]);
        }
    }
    std::ranges::sort(variables);
    EXPECT_EQ(part(reach.pattern.variables, reach.pattern.starts, k), variables);
    EXPECT_EQ(pairs_of(part(reach.route.runs, reach.route.starts, k)), runs_of(reached));
}

void expect_reach(const Graph &graph,
                  std::span<const Offset> positions,
                  std::span<const Offset> roots)
{
    const Reach reach = reach_of(graph, positions, roots);
    ASSERT_EQ(reach.pattern.starts.size(), roots.size() + 1);
    ASSERT_EQ(reach.route.starts.size(), roots.size() + 1);
    for (std::size_t k = 0; k < roots.size(); ++k)
    {
        SCOPED_TRACE(k);
        expect_cone(graph, positions, roots[k], reach, k);
    }
}

}

TEST(ReachTest, Shuffled)
{
    for (std::uint64_t seed = 1; seed <= kSeeds; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = windowed(random);
        expect_reach(graph, shuffled(random, graph), picks(random, graph, kBatchRoots));
    }
}

TEST(ReachTest, Ladder)
{
    Graph graph;
    const std::vector<Offset> rungs = ladder(graph);
    expect_reach(graph, spaced(graph), rungs);
}

TEST(ReachTest, Waves)
{
    Graph graph;
    const std::vector<Offset> rungs = ladder(graph);
    const std::vector<Offset> roots = waves(graph, rungs);
    expect_reach(graph, spaced(graph), roots);
}

TEST(ReachTest, Touching)
{
    Graph graph;
    const std::vector<Offset> rungs = ladder(graph);
    expect_reach(graph, touching(graph), rungs);
}
