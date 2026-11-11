#include "allocations.hpp"
#include "core/index.hpp"
#include "expr/builder.hpp"
#include "expr/compact.hpp"
#include "expr/enclosure.hpp"
#include "expr/graph.hpp"
#include "expr/interval.hpp"
#include "expr/node.hpp"
#include "expr/tape.hpp"
#include "expr/value.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <new>
#include <span>
#include <vector>

namespace
{

using biyos::Builder;
using biyos::Graph;
using biyos::Interval;
using biyos::Offset;
using biyos::Operator;
using biyos::Tape;
using biyos::test::refusal;

constexpr std::size_t kAttempts = 100;

constexpr std::array kUnary{
    Operator::Negation, Operator::Exp,   Operator::Log,     Operator::Sqrt, Operator::Sin,
    Operator::Cos,      Operator::Tan,   Operator::Abs,     Operator::Asin, Operator::Acos,
    Operator::Atan,     Operator::Sinh,  Operator::Cosh,    Operator::Tanh, Operator::Asinh,
    Operator::Acosh,    Operator::Atanh, Operator::Entropy, Operator::Erf,
};

struct PendingFailure
{
    explicit PendingFailure(std::size_t successes) noexcept { biyos::test::fail_after(successes); }
    PendingFailure(const PendingFailure &) = delete;
    PendingFailure &operator=(const PendingFailure &) = delete;
    ~PendingFailure() { biyos::test::stop_failing(); }
};

std::vector<Offset> ids(std::span<const Offset> nodes)
{
    return {nodes.begin(), nodes.end()};
}

void expect_same(const Graph &found, Offset node, const Graph &wanted, Offset root)
{
    EXPECT_EQ(found.op(node), wanted.op(root));
    EXPECT_EQ(ids(found.children(node)), ids(wanted.children(root)));
    EXPECT_DOUBLE_EQ(found.constant(node), wanted.constant(root));
    const std::span<const double> coefficients = found.coefficients(node);
    const std::span<const double> expected = wanted.coefficients(root);
    ASSERT_EQ(coefficients.size(), expected.size());
    for (std::size_t k = 0; k < expected.size(); ++k)
    {
        EXPECT_DOUBLE_EQ(coefficients[k], expected[k]);
    }
}

void pending_sum(Builder &build)
{
    build.variable(0);
    build.variable(1);
    const std::array weights{1.0, 1.0};
    build.sum(0.0, weights);
    build.variable(2);
}

void pending_product(Builder &build)
{
    build.variable(0);
    build.variable(1);
    build.product(1.0, 2);
    build.variable(2);
}

void merge_sum(Builder &build)
{
    const std::array weights{2.0, 3.0};
    build.sum(1.0, weights);
}

void merge_product(Builder &build)
{
    build.product(2.0, 2);
}

bool runs_out(void (*call)(Builder &), Builder &build, std::size_t successes)
{
    const PendingFailure failure(successes);
    try
    {
        call(build);
    }
    catch (const std::bad_alloc &)
    {
        return true;
    }
    return false;
}

Offset finished(Graph &graph, void (*prepare)(Builder &), void (*call)(Builder &))
{
    Builder build(graph);
    prepare(build);
    call(build);
    return build.finish();
}

void expect_kept(void (*prepare)(Builder &), void (*call)(Builder &))
{
    Graph wanted;
    const Offset reference = finished(wanted, prepare, call);
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Graph graph;
        Builder build(graph);
        prepare(build);
        if (!runs_out(call, build, successes))
        {
            EXPECT_GT(successes, 0U);
            expect_same(graph, build.finish(), wanted, reference);
            return;
        }
        EXPECT_EQ(refusal([&] { build.finish(); }),
                  "invalid argument: expression: 2 operands left, not 1");
        call(build);
        expect_same(graph, build.finish(), wanted, reference);
    }
    ADD_FAILURE() << "the call never completed";
}

std::size_t compaction_cost(std::size_t width)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::vector<Offset> terms(width, x);
    const std::vector<double> weights(width, 1.0);
    const Offset root = graph.add_sum(0.0, terms, weights);
    const std::size_t start = biyos::test::allocations();
    biyos::compact(graph, std::span(&root, 1));
    return biyos::test::allocations() - start;
}

std::vector<Offset> products(Graph &graph)
{
    const std::array pair{graph.add_variable(0), graph.add_variable(1)};
    const Offset shared = graph.add_sum(1.0, pair, std::array{2.0, 3.0});
    std::vector<Offset> roots;
    for (std::size_t k = 0; k < 8; ++k)
    {
        const std::array factors{shared, pair[k % 2]};
        const Offset product = graph.add(Operator::Product, 1.5, factors);
        roots.push_back(graph.add(Operator::Max, std::array{product, pair[0]}));
    }
    return roots;
}

std::size_t sweep_cost(const Tape &tape)
{
    biyos::Workspace workspace(tape);
    std::vector<double> values(static_cast<std::size_t>(tape.roots()));
    const std::array point{0.5, 2.0};
    const std::size_t start = biyos::test::allocations();
    biyos::forward(tape, point, workspace, values);
    return biyos::test::allocations() - start;
}

Graph every_operator()
{
    Graph graph;
    const std::array x{graph.add_variable(0), graph.add_variable(1)};
    const std::span<const Offset> one(x.data(), 1);
    const std::array weights{2.0, -1.0};
    graph.add(Operator::Constant, 1.5, {});
    graph.add_sum(0.5, x, weights);
    graph.add(Operator::Product, 3.0, x);
    graph.add(Operator::Division, x);
    graph.add(Operator::ConstantPower, 2.0, one);
    graph.add(Operator::SignPower, 1.5, one);
    graph.add(Operator::Power, x);
    graph.add(Operator::Max, x);
    graph.add(Operator::Min, x);
    for (const Operator op : kUnary)
    {
        graph.add(op, one);
    }
    return graph;
}

}

TEST(AllocationTest, MergedSum)
{
    expect_kept(pending_sum, merge_sum);
}

TEST(AllocationTest, MergedProduct)
{
    expect_kept(pending_product, merge_product);
}

TEST(AllocationTest, CompactWorkspace)
{
    EXPECT_EQ(compaction_cost(2), compaction_cost(64));
}

TEST(AllocationTest, ForwardSweep)
{
    Graph graph;
    const std::vector<Offset> roots = products(graph);
    const Tape tape(graph, roots, 4);
    ASSERT_GT(tape.prepass(), 0);
    EXPECT_EQ(sweep_cost(tape), 0U);
}

TEST(AllocationTest, Intervals)
{
    const Graph graph = every_operator();
    std::vector<Interval> nodes(static_cast<std::size_t>(graph.size()), Interval::empty());
    const std::array box{Interval(-0.5, 2.0), Interval(-1.0, 3.0)};
    const std::size_t before = biyos::test::allocations();
    biyos::enclose(graph, box, nodes);
    EXPECT_EQ(biyos::test::allocations(), before);
    EXPECT_FALSE(nodes.back().is_empty());
}
