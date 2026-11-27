#include "core/numbers.hpp"
#include "expr/evaluation/hessian/coloring.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace
{

using biyos::Coloring;
using biyos::Entry;
using biyos::Graph;
using biyos::Index;
using biyos::Offset;
using biyos::Pusher;
using biyos::Read;
using biyos::Run;
using biyos::star_coloring;
using biyos::star_work;
using biyos::Tape;
using biyos::to_index;
using biyos::to_size;
using biyos::variables_of;
using biyos::test::arrow;
using biyos::test::band;
using biyos::test::kWide;
using biyos::test::products;
using biyos::test::Random;
using biyos::test::variables;

using Neighbours = std::vector<std::vector<Index>>;

constexpr Index kLong = 100000;
constexpr std::array kSizes{15, 16, 17, 18, 24, 33};
constexpr std::uint64_t kSeeds = 1000;
constexpr std::size_t kLargest = 40;

struct Sample
{
    std::vector<Entry> entries;
    std::size_t size;
};

Sample sample_of(const Graph &graph, Offset root)
{
    const Tape tape(graph, std::span(&root, 1));
    Pusher pusher(tape);
    return {.entries = pusher.shape(0).entries, .size = variables_of(tape.pattern(), 0).size()};
}

std::vector<Sample> shapes()
{
    std::vector<Sample> samples;
    Random random(11);
    for (const Index size : kSizes)
    {
        Graph graph;
        const std::vector<Offset> x = variables(graph, size);
        samples.push_back(sample_of(graph, arrow(graph, x)));
        samples.push_back(sample_of(graph, band(graph, x)));
        samples.push_back(sample_of(graph, products(graph, x, random)));
    }
    return samples;
}

Sample path_of(std::size_t size)
{
    Sample sample{.entries = {}, .size = size};
    for (Index k = 1; k < to_index(size); ++k)
    {
        sample.entries.push_back({k, k - 1});
    }
    return sample;
}

Sample random_sample(Random &random)
{
    const std::size_t size = 1 + random.below(kLargest);
    const std::size_t density = 1 + random.below(8);
    Sample sample{.entries = {}, .size = size};
    for (std::size_t column = 0; column < size; ++column)
    {
        for (std::size_t row = column; row < size; ++row)
        {
            if (random.below(2 * density) == 0 || (row == column && random.below(2) == 0))
            {
                sample.entries.push_back({to_index(row), to_index(column)});
            }
        }
    }
    return sample;
}

Neighbours neighbours_of(const Sample &sample)
{
    Neighbours neighbours(sample.size);
    for (const Entry entry : sample.entries)
    {
        if (entry.row != entry.column)
        {
            neighbours[to_size(entry.row)].push_back(entry.column);
            neighbours[to_size(entry.column)].push_back(entry.row);
        }
    }
    return neighbours;
}

bool proper(const Sample &sample, std::span<const Index> colors)
{
    for (const Entry entry : sample.entries)
    {
        if (entry.row != entry.column &&
            colors[to_size(entry.row)] == colors[to_size(entry.column)])
        {
            return false;
        }
    }
    return true;
}

bool bicolored(const Neighbours &neighbours, std::span<const Index> colors, Index v, Index w)
{
    for (const Index u : neighbours[to_size(v)])
    {
        for (const Index x : neighbours[to_size(w)])
        {
            if (u != w && x != v && colors[to_size(u)] == colors[to_size(w)] &&
                colors[to_size(x)] == colors[to_size(v)])
            {
                return true;
            }
        }
    }
    return false;
}

bool star(const Sample &sample, std::span<const Index> colors)
{
    const Neighbours neighbours = neighbours_of(sample);
    for (Index v = 0; v < to_index(sample.size); ++v)
    {
        for (const Index w : neighbours[to_size(v)])
        {
            if (bicolored(neighbours, colors, v, w))
            {
                return false;
            }
        }
    }
    return true;
}

std::vector<Offset> compressed(const Sample &sample, const Coloring &coloring)
{
    const std::size_t size = sample.size;
    std::vector<Offset> sums(size * size, 0);
    for (std::size_t k = 0; k < sample.entries.size(); ++k)
    {
        const auto row = to_size(sample.entries[k].row);
        const auto column = to_size(sample.entries[k].column);
        const auto value = static_cast<Offset>(k + 1);
        sums[row * size + to_size(coloring.colors[column])] += value;
        if (row != column)
        {
            sums[column * size + to_size(coloring.colors[row])] += value;
        }
    }
    return sums;
}

void expect_reads(const Sample &sample, const Coloring &coloring)
{
    ASSERT_EQ(coloring.reads.size(), sample.entries.size());
    const std::vector<Offset> sums = compressed(sample, coloring);
    const auto directions = to_size(coloring.directions);
    std::vector<bool> seen(sample.entries.size(), false);
    Index last = 0;
    for (const Read read : coloring.reads)
    {
        EXPECT_FALSE(seen[to_size(read.part)]);
        seen[to_size(read.part)] = true;
        EXPECT_LE(last, read.color);
        last = read.color;
        EXPECT_LT(to_size(read.color), directions);
        EXPECT_EQ(sums[to_size(read.row) * sample.size + to_size(read.color)], read.part + 1);
    }
    EXPECT_EQ(coloring.reads.empty() ? 0 : coloring.reads.back().color + 1, coloring.directions);
}

Offset instructions(const Tape &tape)
{
    Offset count = 0;
    for (const Run run : tape.runs(0))
    {
        count += run.end - run.begin;
    }
    return count;
}

void expect_star(const Sample &sample)
{
    const Coloring coloring = star_coloring(sample.entries, sample.size);
    ASSERT_EQ(coloring.colors.size(), sample.size);
    EXPECT_TRUE(proper(sample, coloring.colors));
    EXPECT_TRUE(star(sample, coloring.colors));
    expect_reads(sample, coloring);
}

TEST(ColoringTest, Arrow)
{
    Graph graph;
    const Sample sample = sample_of(graph, arrow(graph, variables(graph, kWide)));
    const Coloring coloring = star_coloring(sample.entries, sample.size);
    EXPECT_EQ(coloring.directions, 2);
    EXPECT_EQ(coloring.colors.front(), 1);
    for (std::size_t k = 1; k < sample.size; ++k)
    {
        EXPECT_EQ(coloring.colors[k], 0) << k;
    }
    expect_reads(sample, coloring);
}

TEST(ColoringTest, Long)
{
    Graph graph;
    const Offset root = arrow(graph, variables(graph, kLong));
    const Sample sample = sample_of(graph, root);
    const Offset work = star_work(sample.entries, sample.size);
    EXPECT_EQ(work, kLong - 1);
    EXPECT_LT(work, instructions(Tape(graph, std::span(&root, 1))));
    EXPECT_EQ(star_coloring(sample.entries, sample.size).directions, 2);
}

TEST(ColoringTest, Work)
{
    const std::vector<Entry> path{{1, 0}, {1, 1}, {2, 1}, {3, 2}};
    EXPECT_EQ(star_work(path, 4), 4);
    const std::vector<Entry> clique{{1, 0}, {2, 0}, {2, 1}, {3, 0}, {3, 1}, {3, 2}};
    EXPECT_EQ(star_work(clique, 4), 18);
    EXPECT_EQ(star_work({}, 3), 0);
}

TEST(ColoringTest, Empty)
{
    const Coloring none = star_coloring({}, 0);
    EXPECT_EQ(none.directions, 0);
    EXPECT_TRUE(none.colors.empty());
    const Coloring flat = star_coloring({}, 3);
    EXPECT_EQ(flat.directions, 0);
    EXPECT_EQ(flat.colors, std::vector<Index>(3, 0));
    EXPECT_TRUE(flat.reads.empty());
}

TEST(ColoringTest, Diagonal)
{
    const Sample sample{.entries = {{0, 0}, {2, 2}}, .size = 3};
    const Coloring coloring = star_coloring(sample.entries, sample.size);
    EXPECT_EQ(coloring.directions, 1);
    expect_reads(sample, coloring);
}

TEST(ColoringTest, Path)
{
    for (std::size_t size = 4; size <= kLargest; ++size)
    {
        SCOPED_TRACE(size);
        const Sample sample = path_of(size);
        EXPECT_EQ(star_coloring(sample.entries, sample.size).directions, 3);
        expect_star(sample);
    }
}

TEST(ColoringTest, Shapes)
{
    for (const Sample &sample : shapes())
    {
        expect_star(sample);
    }
}

TEST(ColoringTest, Random)
{
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed)
    {
        Random random(seed);
        SCOPED_TRACE(seed);
        expect_star(random_sample(random));
    }
}

}
