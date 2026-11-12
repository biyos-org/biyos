#include "core/error.hpp"
#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/layout.hpp"
#include "expr/node.hpp"
#include "expr/tape.hpp"
#include "refusal.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace
{

using biyos::Block;
using biyos::Graph;
using biyos::Index;
using biyos::kBudget;
using biyos::Offset;
using biyos::Operator;
using biyos::Tape;
using biyos::test::failure;
using biyos::test::refusal;
using biyos::test::values_of;

using Counts = std::vector<std::size_t>;
using Spans = std::vector<std::pair<std::uint32_t, std::uint32_t>>;
using Ranges = std::vector<std::pair<Offset, Offset>>;

std::size_t at(Offset value)
{
    return static_cast<std::size_t>(value);
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

Offset chain(Graph &graph, Offset start, Offset length)
{
    Offset node = start;
    for (Offset k = 0; k < length; ++k)
    {
        node = unary(graph, Operator::Exp, node);
    }
    return node;
}

std::span<const Block> partitions(const Tape &tape)
{
    return tape.blocks().subspan(at(tape.prepass()));
}

Counts sizes(const Tape &tape)
{
    Counts counts;
    for (const Block &block : partitions(tape))
    {
        counts.push_back(block.outputs.size());
    }
    return counts;
}

Counts lengths(const Tape &tape)
{
    Counts counts;
    for (const Block &block : partitions(tape))
    {
        counts.push_back(block.instructions.size());
    }
    return counts;
}

std::size_t count(const Tape &tape, Operator op)
{
    std::size_t total = 0;
    for (const Block &block : tape.blocks())
    {
        for (const biyos::Instruction &instruction : block.instructions)
        {
            total += instruction.op == op ? 1 : 0;
        }
    }
    return total;
}

std::vector<std::uint32_t> targets(const Block &block)
{
    std::vector<std::uint32_t> slots;
    for (const biyos::Instruction &instruction : block.instructions)
    {
        slots.push_back(instruction.target);
    }
    return slots;
}

Spans spans(const Tape &tape, Offset root)
{
    Spans found;
    for (const biyos::Run &run : tape.runs(root))
    {
        found.emplace_back(run.begin, run.end);
    }
    return found;
}

Spans runs_of(std::span<const biyos::Run> runs)
{
    Spans found;
    for (const biyos::Run &run : runs)
    {
        found.emplace_back(run.begin, run.end);
    }
    return found;
}

std::span<const biyos::Run> part(const std::vector<biyos::Run> &runs,
                                 const std::vector<Offset> &starts,
                                 std::size_t output)
{
    const Offset begin = starts[output];
    return std::span(runs).subspan(at(begin), at(starts[output + 1] - begin));
}

Spans spans_of(const Block &block, std::size_t output)
{
    return runs_of(part(block.runs, block.starts, output));
}

Spans reads_of(const Block &block, std::size_t output)
{
    return runs_of(part(block.reads, block.read_starts, output));
}

Ranges route(const Tape &tape, Offset root)
{
    Ranges found;
    for (const biyos::Range &run : tape.route(root))
    {
        found.emplace_back(run.begin, run.end);
    }
    return found;
}

std::vector<Offset> loads(const Block &block)
{
    return {block.loads.begin(), block.loads.end()};
}

}

TEST(TapeTest, Reuse)
{
    Graph line;
    const Offset end = chain(line, line.add_variable(0), 100);
    const Tape long_chain(line, std::span(&end, 1));
    EXPECT_EQ(long_chain.width(), 1U);
    EXPECT_EQ(long_chain.blocks()[0].instructions.size(), 101U);

    Graph pair;
    const Offset left = chain(pair, pair.add_variable(0), 3);
    const Offset right = chain(pair, pair.add_variable(1), 3);
    const Offset ratio = binary(pair, Operator::Division, left, right);
    const Tape two(pair, std::span(&ratio, 1));
    EXPECT_EQ(two.width(), 2U);
    EXPECT_EQ(targets(two.blocks()[0]), (std::vector<std::uint32_t>{0, 0, 0, 0, 1, 1, 1, 1, 1}));

    Graph graph;
    const Offset shared = unary(graph, Operator::Exp, graph.add_variable(0));
    const Offset sine = unary(graph, Operator::Sin, shared);
    const Offset cosine = unary(graph, Operator::Cos, shared);
    const std::array terms{sine, cosine};
    const Offset root = graph.add_sum(0.0, terms, std::array{1.0, 1.0});
    const Tape tape(graph, std::span(&root, 1));
    EXPECT_EQ(tape.width(), 2U);
    EXPECT_EQ(targets(tape.blocks()[0]), (std::vector<std::uint32_t>{0, 0, 1, 0, 0}));
}

TEST(TapeTest, Pinned)
{
    Graph graph;
    const std::array leaves{graph.add_variable(0), graph.add_variable(1)};
    const Offset inner = graph.add_sum(0.0, leaves, std::array{1.0, 1.0});
    const Offset shared = unary(graph, Operator::Exp, inner);
    const Offset sine = unary(graph, Operator::Sin, shared);
    const Offset cosine = unary(graph, Operator::Cos, shared);
    const std::array roots{sine, cosine};

    const Tape split(graph, roots, 5);
    EXPECT_EQ(sizes(split), (Counts{1, 1}));
    EXPECT_EQ(split.prepass(), 1);
    EXPECT_EQ(split.pinned(), 2);
    EXPECT_EQ(count(split, Operator::Exp), 1U);
    EXPECT_EQ(count(split, Operator::Sum), 1U);
    for (const Block &block : partitions(split))
    {
        EXPECT_EQ(loads(block), (std::vector<Offset>{1}));
    }

    const Tape whole(graph, roots);
    EXPECT_EQ(sizes(whole), (Counts{2}));
    EXPECT_EQ(whole.prepass(), 0);
    EXPECT_EQ(whole.pinned(), 0);
    EXPECT_EQ(count(whole, Operator::Exp), 1U);
}

TEST(TapeTest, SharedVariable)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    std::vector<Offset> roots;
    for (int k = 0; k < 4; ++k)
    {
        roots.push_back(unary(graph, Operator::Sin, x));
    }
    const Tape tape(graph, roots, 2);
    EXPECT_EQ(sizes(tape), (Counts{1, 1, 1, 1}));
    EXPECT_EQ(tape.pinned(), 0);
    EXPECT_EQ(tape.prepass(), 0);
    EXPECT_EQ(count(tape, Operator::Variable), 4U);
    for (const Block &block : partitions(tape))
    {
        EXPECT_TRUE(block.loads.empty());
    }
}

TEST(TapeTest, Budget)
{
    Graph own;
    std::vector<Offset> chains;
    const std::array steps{3, 3, 3, 10, 1, 7};
    for (std::size_t k = 0; k < steps.size(); ++k)
    {
        chains.push_back(chain(own, own.add_variable(static_cast<Index>(k)), steps[k]));
    }
    const Tape cut(own, chains, 10);
    EXPECT_EQ(sizes(cut), (Counts{2, 1, 1, 2}));
    EXPECT_EQ(lengths(cut), (Counts{8, 4, 11, 10}));

    Graph common;
    const Offset x = common.add_variable(0);
    std::vector<Offset> exps;
    for (int k = 0; k < 7; ++k)
    {
        exps.push_back(unary(common, Operator::Exp, x));
    }
    const Tape leaves(common, exps, 4);
    EXPECT_EQ(sizes(leaves), (Counts{3, 3, 1}));
    EXPECT_EQ(lengths(leaves), (Counts{4, 4, 2}));

    Graph moved;
    const std::array pair{moved.add_variable(0), moved.add_variable(1)};
    const std::array recounted{
        moved.add(Operator::Product, 1.0, pair),
        chain(moved, pair[0], 2),
        chain(moved, moved.add_variable(2), 1),
    };
    const Tape fresh(moved, recounted, 4);
    EXPECT_EQ(sizes(fresh), (Counts{1, 1, 1}));
    EXPECT_EQ(lengths(fresh), (Counts{3, 3, 2}));

    Graph heavy;
    const Offset first = chain(heavy, heavy.add_variable(0), 12);
    const Offset second = chain(heavy, heavy.add_variable(1), 1);
    EXPECT_EQ(sizes(Tape(heavy, std::array{first, second}, 10)), (Counts{1, 1}));

    Graph leaf;
    const Offset lone = leaf.add_variable(0);
    const Tape repeated(leaf, std::array{lone, lone, lone, lone}, 2);
    EXPECT_EQ(sizes(repeated), (Counts{4}));
    EXPECT_EQ(lengths(repeated), (Counts{1}));

    Graph large;
    std::vector<Offset> halves;
    for (Index k = 0; k < 3; ++k)
    {
        halves.push_back(chain(large, large.add_variable(k), kBudget / 2 - 1));
    }
    EXPECT_EQ(sizes(Tape(large, halves)), (Counts{2, 1}));
}

TEST(TapeTest, Runs)
{
    Graph graph;
    const Offset shared = unary(graph, Operator::Exp, graph.add_variable(0));
    const Offset sine = unary(graph, Operator::Sin, graph.add_variable(1));
    const Offset ratio = binary(graph, Operator::Division, shared, sine);
    const Offset negation = unary(graph, Operator::Negation, shared);
    const std::array roots{ratio, negation, shared};

    const Tape whole(graph, roots);
    EXPECT_EQ(spans(whole, 0), (Spans{{0, 5}}));
    EXPECT_EQ(spans(whole, 1), (Spans{{0, 2}, {5, 6}}));
    EXPECT_EQ(spans(whole, 2), (Spans{{0, 2}}));
    const Tape inner(graph, std::array{ratio, sine});
    EXPECT_EQ(spans(inner, 1), (Spans{{2, 4}}));

    Graph nested;
    const Offset x = nested.add_variable(0);
    const Offset rise = unary(nested, Operator::Exp, nested.add_variable(1));
    const Offset total = nested.add_sum(0.0, std::array{x, rise}, std::array{1.0, 1.0});
    const Offset wave = unary(nested, Operator::Sin, total);
    const Offset quotient = binary(nested, Operator::Division, wave, rise);
    const Tape joined(nested, std::array{wave, quotient});
    EXPECT_EQ(spans(joined, 0), (Spans{{0, 5}}));
    EXPECT_EQ(spans(joined, 1), (Spans{{0, 6}}));

    const Tape split(graph, roots, 5);
    EXPECT_EQ(sizes(split), (Counts{1, 2}));
    EXPECT_EQ(spans(split, 0), (Spans{{0, 3}}));
    EXPECT_EQ(spans(split, 1), (Spans{{0, 1}}));
    EXPECT_TRUE(spans(split, 2).empty());
    EXPECT_EQ(refusal([&] { split.runs(3); }), "invalid argument: tape: root 3 is unknown");
    EXPECT_EQ(refusal([&] { split.runs(-1); }), "invalid argument: tape: root -1 is unknown");

    Graph lattice;
    Offset top = lattice.add_variable(0);
    for (int k = 0; k < 64; ++k)
    {
        const std::array twice{top, top};
        top = lattice.add(Operator::Product, 1.0, twice);
    }
    const Tape deep(lattice, std::span(&top, 1));
    EXPECT_EQ(spans(deep, 0), (Spans{{0, 65}}));
}

TEST(TapeTest, Reads)
{
    Graph graph;
    const std::array leaves{graph.add_variable(0), graph.add_variable(1)};
    const Offset inner = graph.add_sum(0.0, leaves, std::array{1.0, 1.0});
    const Offset shared = unary(graph, Operator::Exp, inner);
    const std::array roots{
        unary(graph, Operator::Sin, shared),
        unary(graph, Operator::Cos, shared),
    };

    const Tape joined(graph, roots, 5);
    ASSERT_EQ(joined.prepass(), 1);
    const Block &both = joined.blocks()[0];
    EXPECT_EQ(spans_of(both, 0), (Spans{{0, 3}}));
    EXPECT_EQ(spans_of(both, 1), (Spans{{3, 4}}));
    EXPECT_TRUE(both.reads.empty());
    for (const Block &block : partitions(joined))
    {
        EXPECT_EQ(spans_of(block, 0), (Spans{{0, 1}}));
        EXPECT_EQ(reads_of(block, 0), (Spans{{0, 1}}));
    }

    const Tape split(graph, roots, 3);
    ASSERT_EQ(split.prepass(), 2);
    const std::span<const Block> blocks = split.blocks();
    EXPECT_EQ(spans_of(blocks[0], 0), (Spans{{0, 3}}));
    EXPECT_TRUE(reads_of(blocks[0], 0).empty());
    EXPECT_EQ(spans_of(blocks[1], 0), (Spans{{0, 1}}));
    EXPECT_EQ(reads_of(blocks[1], 0), (Spans{{0, 1}}));
    EXPECT_EQ(loads(blocks[1]), (std::vector<Offset>{0}));

    Graph wide;
    std::vector<Offset> waves;
    for (Index k = 0; k < 6; ++k)
    {
        waves.push_back(unary(wide, Operator::Sin, wide.add_variable(k)));
    }
    const std::vector<double> ones(waves.size(), 1.0);
    const Offset total = wide.add_sum(0.0, waves, ones);
    const std::array sums{total, unary(wide, Operator::Cos, wide.add_sum(1.0, waves, ones))};
    const Tape cut(wide, sums, 13);
    ASSERT_EQ(cut.pinned(), 6);
    for (const Block &block : partitions(cut))
    {
        EXPECT_EQ(reads_of(block, 0), (Spans{{0, 6}}));
    }

    Graph pair;
    const Offset wave = unary(pair, Operator::Sin, pair.add_variable(0));
    const Offset swell = unary(pair, Operator::Cos, pair.add_variable(1));
    const Offset joint = pair.add(Operator::Product, 1.0, std::array{wave, swell});
    const std::array ends{unary(pair, Operator::Exp, joint), unary(pair, Operator::Log, joint)};
    const Tape apart(pair, ends, 1);
    ASSERT_EQ(apart.prepass(), 3);
    EXPECT_EQ(reads_of(apart.blocks()[2], 0), (Spans{{0, 2}}));
}

TEST(TapeTest, Route)
{
    Graph graph;
    const std::array leaves{graph.add_variable(0), graph.add_variable(1)};
    const Offset inner = graph.add_sum(0.0, leaves, std::array{1.0, 1.0});
    const Offset shared = unary(graph, Operator::Exp, inner);
    const Offset sine = unary(graph, Operator::Sin, shared);
    const Offset other = unary(graph, Operator::Cos, graph.add_variable(2));
    const std::array roots{sine, other, shared};

    const Tape split(graph, roots, 5);
    EXPECT_EQ(sizes(split), (Counts{1, 2}));
    EXPECT_EQ(route(split, 0), (Ranges{{0, 2}}));
    EXPECT_TRUE(route(split, 1).empty());
    EXPECT_EQ(route(split, 2), (Ranges{{0, 2}}));
    const biyos::Pattern &pattern = split.pattern();
    EXPECT_EQ(pattern.variables, (std::vector<Index>{0, 1, 2, 0, 1}));
    EXPECT_EQ(pattern.starts, (std::vector<Offset>{0, 2, 3, 5}));

    const Tape whole(graph, roots);
    for (Offset root = 0; root < 3; ++root)
    {
        EXPECT_TRUE(route(whole, root).empty());
    }
    EXPECT_EQ(whole.pattern().variables, pattern.variables);

    Graph line;
    const Offset end = chain(line, line.add_variable(0), 20);
    std::vector<Offset> rows;
    for (Index k = 1; k <= 3; ++k)
    {
        const std::array pair{end, line.add_variable(k)};
        rows.push_back(line.add(Operator::Product, 1.0, pair));
    }
    const Tape long_route(line, rows, 3);
    ASSERT_EQ(long_route.pinned(), 20);
    for (Offset root = 0; root < 3; ++root)
    {
        EXPECT_EQ(route(long_route, root), (Ranges{{0, 20}}));
    }
    EXPECT_EQ(long_route.pattern().variables, (std::vector<Index>{0, 1, 0, 2, 0, 3}));
}

TEST(TapeTest, Places)
{
    Graph line;
    const Offset x = line.add_variable(0);
    const Offset y = line.add_variable(1);
    const Offset z = line.add_variable(2);
    const Offset product = line.add(Operator::Product, 2.0, std::array{x, y, z});
    const Offset ratio = binary(line, Operator::Division, unary(line, Operator::Sin, product), y);
    const Offset sum = line.add_sum(1.0, std::array{ratio, x}, std::array{1.0, 2.0});
    const Offset root = binary(line, Operator::Max, unary(line, Operator::Negation, sum), z);
    const Tape whole(line, std::span(&root, 1));
    const std::vector<std::uint32_t> places{0, 0, 0, 0, 3, 4, 6, 7, 8, 9};
    EXPECT_EQ(whole.blocks()[0].places, places);
    EXPECT_EQ(whole.blocks()[0].origin, 0);
    EXPECT_EQ(whole.partials(), 9);

    Graph graph;
    const std::array pair{graph.add_variable(0), graph.add_variable(1)};
    const Offset shared = unary(graph, Operator::Exp, graph.add(Operator::Product, 1.0, pair));
    const std::array roots{unary(graph, Operator::Sin, shared),
                           unary(graph, Operator::Cos, shared)};
    const Tape split(graph, roots, 3);
    ASSERT_EQ(split.prepass(), 2);
    std::vector<Offset> origins;
    for (const Block &block : split.blocks())
    {
        origins.push_back(block.origin);
    }
    EXPECT_EQ(origins, (std::vector<Offset>{0, 2, 3, 3}));
    EXPECT_EQ(split.partials(), 4);
}

TEST(TapeTest, Recompile)
{
    Graph graph;
    const Offset first = unary(graph, Operator::Exp, graph.add_variable(0));
    const Tape before(graph, std::span(&first, 1));
    Offset second = first;
    for (int k = 0; k < 1000; ++k)
    {
        second = unary(graph, Operator::Sin, second);
    }
    const std::array roots{first, second};
    const Tape after(graph, roots);

    const std::array point{0.3};
    const double start = std::exp(point[0]);
    double sine = start;
    for (int k = 0; k < 1000; ++k)
    {
        sine = std::sin(sine);
    }
    const std::vector<double> old_values = values_of(before, point);
    ASSERT_EQ(old_values.size(), 1U);
    EXPECT_DOUBLE_EQ(old_values[0], start);
    const std::vector<double> new_values = values_of(after, point);
    ASSERT_EQ(new_values.size(), 2U);
    EXPECT_DOUBLE_EQ(new_values[0], start);
    EXPECT_DOUBLE_EQ(new_values[1], sine);
}

TEST(TapeTest, MissingRoot)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::array roots{x, x + 1};
    EXPECT_EQ(refusal([&] { const Tape tape(graph, roots); }),
              "invalid argument: expression: node 1 is unknown");
}

TEST(TapeTest, Bound)
{
    const std::size_t largest = (std::numeric_limits<std::uint32_t>::max)();
    EXPECT_EQ(biyos::narrow(largest), largest);
    EXPECT_EQ(failure([&] { biyos::narrow(largest + 1); }, biyos::ErrorKind::NoMemory),
              "out of memory: tape: a block is too large for 32-bit slots");
}

TEST(TapeTest, NoRoots)
{
    Graph graph;
    graph.add_variable(0);
    const Tape tape(graph, {});
    EXPECT_TRUE(tape.blocks().empty());
    EXPECT_EQ(tape.roots(), 0);
    EXPECT_EQ(tape.width(), 0U);
    EXPECT_EQ(refusal([&] { tape.runs(0); }), "invalid argument: tape: root 0 is unknown");
}
