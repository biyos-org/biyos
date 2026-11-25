#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/sparsity/reach.hpp"
#include "graphs.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace
{

using biyos::Entry;
using biyos::Graph;
using biyos::Index;
using biyos::Lower;
using biyos::Map;
using biyos::Offset;
using biyos::Operator;
using biyos::Pattern;
using biyos::to_size;
using biyos::test::binary;
using biyos::test::refusal;
using biyos::test::unary;

Pattern pattern_of(const std::vector<std::vector<Index>> &rows)
{
    Pattern pattern{.variables = {}, .starts = {0}};
    for (const std::vector<Index> &row : rows)
    {
        pattern.variables.insert(pattern.variables.end(), row.begin(), row.end());
        pattern.starts.push_back(static_cast<Offset>(pattern.variables.size()));
    }
    return pattern;
}

std::vector<Entry> entries_of(const Lower &lower)
{
    std::vector<Entry> entries;
    for (std::size_t j = 0; j + 1 < lower.starts.size(); ++j)
    {
        for (Offset k = lower.starts[j]; k < lower.starts[j + 1]; ++k)
        {
            entries.push_back({.row = lower.rows[to_size(k)], .column = static_cast<Index>(j)});
        }
    }
    return entries;
}

void expect_entries(const Lower &lower, const std::vector<std::array<Index, 2>> &wanted)
{
    const std::vector<Entry> found = entries_of(lower);
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        SCOPED_TRACE(k);
        EXPECT_EQ(found[k].row, wanted[k][0]);
        EXPECT_EQ(found[k].column, wanted[k][1]);
    }
}

void expect_at(const Lower &lower, Offset place, Index row, Index column)
{
    ASSERT_GE(place, lower.starts[to_size(column)]);
    ASSERT_LT(place, lower.starts[to_size(column) + 1]);
    EXPECT_EQ(lower.rows[to_size(place)], row);
}

void expect_blocks(const Lower &lower, const Map &map, const Pattern &blocks)
{
    for (std::size_t e = 0; e + 1 < blocks.starts.size(); ++e)
    {
        SCOPED_TRACE(e);
        const auto variables = std::span(blocks.variables)
                                   .subspan(to_size(blocks.starts[e]),
                                            to_size(blocks.starts[e + 1] - blocks.starts[e]));
        Offset k = map.starts[e];
        for (std::size_t l = 0; l < variables.size(); ++l)
        {
            for (std::size_t a = l; a < variables.size(); ++a)
            {
                expect_at(lower, map.blocks[to_size(k)], variables[a], variables[l]);
                ++k;
            }
        }
        EXPECT_EQ(k, map.starts[e + 1]);
    }
}

}

TEST(ElementTest, Nested)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset quotient = binary(graph, Operator::Division, x, y);
    const Offset sine = unary(graph, Operator::Sin, y);
    const Offset inner = graph.add_sum(1.0, std::array{quotient, sine}, std::array{3.0, -1.0});
    const Offset exp = unary(graph, Operator::Exp, x);
    const Offset outer = graph.add_sum(4.0, std::array{inner, exp, x}, std::array{2.0, 0.5, 7.0});
    const Offset cosine = unary(graph, Operator::Cos, x);
    const std::array roots{outer, cosine, inner};
    const biyos::Elements elements = biyos::elements_of(graph, roots);
    EXPECT_EQ(elements.nodes, (std::vector{quotient, sine, exp, x, cosine, quotient, sine}));
    EXPECT_EQ(elements.roots, (std::vector<Offset>{0, 0, 0, 0, 1, 2, 2}));
    EXPECT_EQ(elements.weights, (std::vector{6.0, -2.0, 0.5, 7.0, 1.0, 3.0, -1.0}));
}

TEST(ElementTest, Refused)
{
    Graph graph;
    graph.add_variable(0);
    const std::array roots{Offset{0}, Offset{1}};
    EXPECT_EQ(refusal([&] { biyos::elements_of(graph, roots); }),
              "invalid argument: expression: node 1 is unknown");
    const std::array declared{Entry{.row = 1, .column = 0}, Entry{.row = 0, .column = -1}};
    EXPECT_EQ(refusal([&] { biyos::lower_of(pattern_of({}), declared); }),
              "invalid argument: entries: entry 1 has a negative index");
}

TEST(ElementTest, Union)
{
    const Pattern blocks = pattern_of({{0, 3}, {1, 3}, {}, {2}, {0, 3}});
    const Lower lower = biyos::lower_of(blocks, {});
    expect_entries(lower, {{0, 0}, {3, 0}, {1, 1}, {3, 1}, {2, 2}, {3, 3}});
    const Map map = biyos::map_of(lower, blocks, {});
    EXPECT_EQ(map.starts, (std::vector<Offset>{0, 3, 6, 6, 7, 10}));
    expect_blocks(lower, map, blocks);
    EXPECT_TRUE(map.declared.empty());
}

TEST(ElementTest, Declared)
{
    const Pattern blocks = pattern_of({{1, 2}});
    const std::array declared{
        Entry{.row = 0, .column = 2},
        Entry{.row = 2, .column = 1},
        Entry{.row = 5, .column = 4},
        Entry{.row = 2, .column = 0},
    };
    const Lower lower = biyos::lower_of(blocks, declared);
    expect_entries(lower, {{2, 0}, {1, 1}, {2, 1}, {2, 2}, {5, 4}});
    EXPECT_EQ(lower.starts.size(), 7U);
    const Map map = biyos::map_of(lower, blocks, declared);
    EXPECT_EQ(map.declared, (std::vector<Offset>{0, 2, 4, 0}));
    expect_blocks(lower, map, blocks);
}
