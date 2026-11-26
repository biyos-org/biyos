#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/tape/tape.hpp"
#include "expr/threads/levels.hpp"
#include "graphs.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace
{

using biyos::Block;
using biyos::Graph;
using biyos::level;
using biyos::Levels;
using biyos::levels_of;
using biyos::Offset;
using biyos::Tape;
using biyos::to_size;
using biyos::test::kBudgets;
using biyos::test::kRoots;
using biyos::test::picks;
using biyos::test::Random;
using biyos::test::random_graph;

std::vector<std::size_t> depths_of(const Levels &levels, std::size_t blocks)
{
    std::vector<std::size_t> depths(blocks, blocks);
    for (std::size_t k = 0; k + 1 < levels.starts.size(); ++k)
    {
        for (const std::size_t b : level(levels, k))
        {
            depths[b] = k;
        }
    }
    return depths;
}

std::size_t producer(std::span<const Block> prepass, Offset load)
{
    const auto owns = [load](const Block &block) { return load < block.begin; };
    return static_cast<std::size_t>(std::ranges::find_if(prepass, owns) - prepass.begin()) - 1;
}

}

TEST(LevelTest, Topological)
{
    std::size_t deep = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const Tape tape(graph, picks(random, graph, kRoots), kBudgets[seed % kBudgets.size()]);
        const std::span<const Block> prepass = tape.blocks().first(to_size(tape.prepass()));
        const Levels levels = levels_of(tape);
        std::vector<std::size_t> sorted = levels.blocks;
        std::ranges::sort(sorted);
        ASSERT_EQ(sorted.size(), prepass.size());
        EXPECT_TRUE(std::ranges::equal(sorted, std::views::iota(std::size_t{0}, sorted.size())));
        const std::vector<std::size_t> depths = depths_of(levels, prepass.size());
        for (std::size_t b = 0; b < prepass.size(); ++b)
        {
            for (const Offset load : prepass[b].loads)
            {
                EXPECT_LT(depths[producer(prepass, load)], depths[b]);
            }
        }
        if (levels.starts.size() > 3)
        {
            ++deep;
        }
    }
    EXPECT_GT(deep, 0);
}

TEST(LevelTest, Empty)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Levels levels = levels_of(Tape(graph, std::span(&x, 1)));
    EXPECT_TRUE(levels.blocks.empty());
    EXPECT_EQ(levels.starts, std::vector<Offset>{0});
}
