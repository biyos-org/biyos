#include "allocations.hpp"
#include "core/index.hpp"
#include "expr/builder.hpp"
#include "expr/graph.hpp"
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
using biyos::Offset;
using biyos::test::refusal;

constexpr std::size_t kAttempts = 100;

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

}

TEST(AllocationTest, MergedSum)
{
    expect_kept(pending_sum, merge_sum);
}

TEST(AllocationTest, MergedProduct)
{
    expect_kept(pending_product, merge_product);
}
