#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "expr/reach.hpp"
#include "expr/tape.hpp"
#include "graphs.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Index;
using biyos::Offset;
using biyos::Operator;
using biyos::Tape;
using biyos::test::kBudgets;
using biyos::test::kRoots;
using biyos::test::picks;
using biyos::test::Random;
using biyos::test::random_graph;
using biyos::test::unary;

std::size_t at(Offset value)
{
    return static_cast<std::size_t>(value);
}

std::vector<Index> reached(const Graph &graph, Offset root)
{
    std::vector<bool> seen(at(graph.size()), false);
    std::vector<Offset> stack{root};
    std::vector<Index> variables;
    while (!stack.empty())
    {
        const Offset node = stack.back();
        stack.pop_back();
        if (seen[at(node)])
        {
            continue;
        }
        seen[at(node)] = true;
        if (graph.op(node) == Operator::Variable)
        {
            variables.push_back(graph.variable(node));
        }
        const auto children = graph.children(node);
        stack.insert(stack.end(), children.begin(), children.end());
    }
    std::ranges::sort(variables);
    return variables;
}

std::vector<Index> pattern_of(const Tape &tape, std::size_t root)
{
    const biyos::Pattern &pattern = tape.pattern();
    const auto first = pattern.variables.begin();
    return {first + pattern.starts[root], first + pattern.starts[root + 1]};
}

std::vector<Offset> ladder(Graph &graph, Index size)
{
    Offset sum = graph.add_variable(0);
    std::vector<Offset> waves;
    for (Index k = 1; k <= size; ++k)
    {
        const std::array terms{sum, graph.add_variable(k)};
        sum = graph.add_sum(0.0, terms, std::array{1.0, 1.0});
        waves.push_back(unary(graph, Operator::Sin, sum));
    }
    const std::vector<double> ones(waves.size(), 1.0);
    return {graph.add_sum(0.0, waves, ones), sum};
}

std::vector<Offset> recurrence(Graph &graph, Index size)
{
    Offset state = graph.add_variable(0);
    for (Index k = 1; k <= size; ++k)
    {
        const std::array pair{unary(graph, Operator::Sin, state),
                              unary(graph, Operator::Cos, state)};
        const std::array terms{graph.add(Operator::Product, 1.0, pair), graph.add_variable(k)};
        state = graph.add_sum(0.0, terms, std::array{1.0, 1.0});
    }
    return {state};
}

std::vector<Offset> prefixes(Graph &graph, Index size)
{
    Offset link = graph.add_variable(0);
    std::vector<Offset> roots;
    for (Index k = 1; k <= size; ++k)
    {
        link = unary(graph, Operator::Sin, link);
        const std::array pair{link, graph.add_variable(k)};
        roots.push_back(graph.add(Operator::Product, 1.0, pair));
    }
    roots.push_back(link);
    return roots;
}

std::vector<Offset> products(Graph &graph, Index size)
{
    Offset total = graph.add_variable(0);
    std::vector<Offset> terms;
    for (Index k = 1; k <= size; ++k)
    {
        const std::array pair{total, graph.add_variable(k)};
        total = graph.add(Operator::Product, 1.0, pair);
        terms.push_back(unary(graph, Operator::Exp, total));
    }
    const std::vector<double> ones(terms.size(), 1.0);
    return {graph.add_sum(0.0, terms, ones), total, terms[2]};
}

Offset sum_of(Graph &graph, Index first, Index count)
{
    std::vector<Offset> terms;
    for (Index k = first; k < first + count; ++k)
    {
        terms.push_back(graph.add_variable(k));
    }
    const std::vector<double> ones(terms.size(), 1.0);
    return graph.add_sum(0.0, terms, ones);
}

std::vector<Offset> powers(Graph &graph, Index size)
{
    const Offset sum = sum_of(graph, 0, size);
    Offset power = unary(graph, Operator::Sin, sum);
    std::vector<Offset> roots;
    for (Index k = 0; k < size; ++k)
    {
        const Offset product = graph.add(Operator::Product, 1.0 / size, std::array{power, sum});
        power = unary(graph, Operator::Sin, product);
        const std::array pair{power, graph.add_variable(size + k)};
        roots.push_back(graph.add(Operator::Product, 1.0, pair));
    }
    return roots;
}

std::vector<Offset> lattice(Graph &graph, Index size)
{
    const std::array sums{sum_of(graph, 0, size), sum_of(graph, size, size)};
    std::vector<Offset> level;
    for (Index j = 0; j < size; ++j)
    {
        level.push_back(unary(graph, Operator::Sin, sums[at(j % 2)]));
    }
    std::vector<Offset> next;
    for (Index i = 0; i < size; ++i)
    {
        next.clear();
        for (Index j = 0; j < size; ++j)
        {
            const std::array pair{level[at(j)], level[at((j + 1) % size)]};
            next.push_back(graph.add_sum(0.0, pair, std::array{0.5, 0.5}));
        }
        level.swap(next);
    }
    std::vector<Offset> roots;
    for (const Offset top : level)
    {
        roots.push_back(unary(graph, Operator::Sin, top));
    }
    return roots;
}

std::vector<Offset> inherited(Graph &graph, Index size)
{
    std::vector<Offset> terms;
    for (Index k = 0; k < size; ++k)
    {
        terms.push_back(graph.add_variable(k));
    }
    const std::vector<double> ones(terms.size(), 1.0);
    Offset link = graph.add_sum(0.0, terms, ones);
    std::vector<Offset> roots;
    for (Index k = 0; k < size; ++k)
    {
        link = unary(graph, Operator::Sin, link);
        const std::array pair{link, graph.add_variable(size + k)};
        roots.push_back(graph.add(Operator::Product, 1.0, pair));
    }
    return roots;
}

}

TEST(DerivativeTest, Patterns)
{
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kRoots);
        const Tape tape(graph, roots, kBudgets[random.below(kBudgets.size())]);
        ASSERT_EQ(tape.pattern().starts.size(), roots.size() + 1);
        for (std::size_t r = 0; r < roots.size(); ++r)
        {
            SCOPED_TRACE(r);
            EXPECT_EQ(pattern_of(tape, r), reached(graph, roots[r]));
        }
    }
}

TEST(DerivativeTest, Chains)
{
    constexpr std::array shapes{ladder, recurrence, prefixes, products, inherited, powers, lattice};
    for (const Offset budget : {Offset{1}, Offset{3}, biyos::kBudget})
    {
        SCOPED_TRACE(budget);
        for (const Index size : {6, 100})
        {
            SCOPED_TRACE(size);
            for (const auto shape : shapes)
            {
                Graph graph;
                const std::vector<Offset> roots = shape(graph, size);
                const Tape tape(graph, roots, budget);
                for (std::size_t r = 0; r < roots.size(); ++r)
                {
                    SCOPED_TRACE(r);
                    EXPECT_EQ(pattern_of(tape, r), reached(graph, roots[r]));
                }
            }
        }
    }
}
