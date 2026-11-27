#include "allocations.hpp"
#include "core/env/pool.hpp"
#include "core/numbers.hpp"
#include "expr/analysis/curvature/curvature.hpp"
#include "expr/analysis/intervals/enclosure.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/evaluation/derivatives/derivative.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/hessian.hpp"
#include "expr/evaluation/threads/sweeps.hpp"
#include "expr/evaluation/threads/team.hpp"
#include "expr/graph/builder.hpp"
#include "expr/graph/compact.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/sharing.hpp"
#include "expr/tape/tape.hpp"
#include "utils/refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <new>
#include <span>
#include <vector>

namespace
{

using biyos::Adjoints;
using biyos::Builder;
using biyos::compact;
using biyos::enclose;
using biyos::forward;
using biyos::Graph;
using biyos::Hessian;
using biyos::Index;
using biyos::Interval;
using biyos::jacobian;
using biyos::kLanes;
using biyos::Offset;
using biyos::Operator;
using biyos::Pool;
using biyos::Real;
using biyos::Shapes;
using biyos::share;
using biyos::Tape;
using biyos::Team;
using biyos::to_size;
using biyos::weighted_gradient;
using biyos::Workspace;
using biyos::test::allocated_bytes;
using biyos::test::allocations;
using biyos::test::fail_after;
using biyos::test::refusal;
using biyos::test::stop_failing;

constexpr std::size_t kAttempts = 100;
constexpr Index kSteps = 1000;
constexpr std::size_t kGrowth = 8;
constexpr Index kTerms = 1000;
constexpr Offset kWhole = Offset{1} << 40;
constexpr Index kRing = 200;
constexpr std::size_t kThreads = 3;

constexpr std::array kUnary{
    Operator::Negation, Operator::Exp,   Operator::Log,     Operator::Sqrt, Operator::Sin,
    Operator::Cos,      Operator::Tan,   Operator::Abs,     Operator::Asin, Operator::Acos,
    Operator::Atan,     Operator::Sinh,  Operator::Cosh,    Operator::Tanh, Operator::Asinh,
    Operator::Acosh,    Operator::Atanh, Operator::Entropy, Operator::Erf,
};

struct PendingFailure
{
    explicit PendingFailure(std::size_t successes) noexcept { fail_after(successes); }
    PendingFailure(const PendingFailure &) = delete;
    PendingFailure &operator=(const PendingFailure &) = delete;
    ~PendingFailure() { stop_failing(); }
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
    const std::span<const Real> coefficients = found.coefficients(node);
    const std::span<const Real> expected = wanted.coefficients(root);
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

Graph wide_sum(std::size_t width)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::vector<Offset> terms(width, x);
    const std::vector<Real> weights(width, 1.0);
    graph.add_sum(0.0, terms, weights);
    return graph;
}

Graph chain(Offset length)
{
    Graph graph;
    Offset node = graph.add_variable(0);
    for (Offset k = 0; k < length; ++k)
    {
        node = graph.add(Operator::Exp, std::span(&node, 1));
    }
    return graph;
}

std::size_t compaction_cost(const Graph &graph)
{
    const Offset root = graph.size() - 1;
    const std::size_t start = allocations();
    compact(graph, std::span(&root, 1));
    return allocations() - start;
}

std::size_t sharing_cost(const Graph &graph)
{
    const std::size_t start = allocations();
    share(graph);
    return allocations() - start;
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

std::size_t hessian_cost(const Graph &graph, std::span<const Offset> roots, std::size_t lanes)
{
    Hessian hessian(graph, roots, {}, lanes, 4);
    std::vector<Real> out(hessian.pattern().rows.size());
    const std::vector<Real> weights(roots.size(), 1.0);
    const std::array point{0.5, 2.0};
    Pool pool;
    const std::size_t start = allocations();
    hessian.values(point, weights, out, pool);
    return allocations() - start;
}

std::size_t product_cost(const Graph &graph, std::span<const Offset> roots, std::size_t lanes)
{
    Hessian hessian(graph, roots, {}, lanes, 4);
    const std::array point{0.5, 2.0};
    const std::array direction{1.5, -0.5};
    std::vector<Real> out(point.size());
    const std::vector<Real> weights(roots.size(), 1.0);
    Pool pool;
    const std::size_t start = allocations();
    hessian.product(point, weights, direction, out, pool);
    return allocations() - start;
}

std::size_t jacobian_cost(const Tape &tape)
{
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    std::vector<Real> values(static_cast<std::size_t>(tape.roots()));
    std::vector<Real> gradients(tape.pattern().variables.size());
    const std::array point{0.5, 2.0};
    const std::size_t start = allocations();
    jacobian(tape, point, workspace, adjoints, values, gradients);
    return allocations() - start;
}

std::size_t gradient_cost(const Tape &tape)
{
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    std::vector<Real> values(static_cast<std::size_t>(tape.roots()));
    const std::vector<Real> weights(values.size(), 0.5);
    const std::array point{0.5, 2.0};
    std::vector<Real> gradient(point.size());
    const std::size_t start = allocations();
    weighted_gradient(tape, point, weights, workspace, adjoints, values, gradient);
    return allocations() - start;
}

std::size_t compile_bytes(Index steps)
{
    Graph graph;
    Offset total = graph.add_variable(0);
    std::vector<Offset> terms;
    for (Index k = 1; k <= steps; ++k)
    {
        total = graph.add(Operator::Product, 1.0, std::array{total, graph.add_variable(k)});
        terms.push_back(graph.add(Operator::Exp, std::span(&total, 1)));
    }
    const std::vector<Real> ones(terms.size(), 1.0);
    const Offset root = graph.add_sum(0.0, terms, ones);
    const std::size_t start = allocated_bytes();
    const Tape tape(graph, std::span(&root, 1));
    return allocated_bytes() - start;
}

std::size_t shared_bytes(Index terms)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    std::vector<Offset> parts;
    for (Index j = 0; j < terms; ++j)
    {
        const Offset turn = graph.add(Operator::Cos, std::span(&x, 1));
        const Offset inner = graph.add(Operator::Sin, std::span(&turn, 1));
        parts.push_back(graph.add(Operator::Product, 0.5, std::array{inner, inner}));
        parts.push_back(graph.add(Operator::Cos, std::span(&inner, 1)));
    }
    const std::vector<Real> ones(parts.size(), 1.0);
    const Offset total = graph.add_sum(0.0, parts, ones);
    std::vector<Offset> roots;
    for (Index j = 1; j <= terms; ++j)
    {
        roots.push_back(
            graph.add(Operator::Product, 1.0, std::array{total, graph.add_variable(j)}));
    }
    const std::size_t start = allocated_bytes();
    const Tape tape(graph, roots, kWhole);
    return allocated_bytes() - start;
}

std::size_t sweep_cost(const Tape &tape)
{
    Workspace workspace(tape);
    std::vector<Real> values(static_cast<std::size_t>(tape.roots()));
    const std::array point{0.5, 2.0};
    const std::size_t start = allocations();
    forward(tape, point, workspace, values);
    return allocations() - start;
}

std::vector<Offset> ring(Graph &graph)
{
    std::vector<Offset> sines;
    for (Index k = 0; k < kRing; ++k)
    {
        const Offset x = graph.add_variable(k);
        sines.push_back(graph.add(Operator::Sin, std::span(&x, 1)));
    }
    std::vector<Offset> roots;
    for (std::size_t k = 0; k < sines.size(); ++k)
    {
        const std::array pair{sines[k], sines[(k + 1) % sines.size()]};
        roots.push_back(graph.add(Operator::Product, 1.0, pair));
    }
    return roots;
}

struct Threaded
{
    Threaded(const Graph &graph, std::span<const Offset> roots);
    void run();

    Tape tape;
    Team team;
    Hessian hessian;
    Pool pool{kThreads};
    std::vector<Real> point;
    std::vector<Real> weights;
    std::vector<Real> values;
    std::vector<Real> gradients;
    std::vector<Real> gradient;
    std::vector<Real> entries;
};

Threaded::Threaded(const Graph &graph, std::span<const Offset> roots)
    : tape(graph, roots, 4), team(tape), hessian(graph, roots, {}), point(to_size(kRing), 0.5),
      weights(roots.size(), 1.0), values(roots.size()), gradients(tape.pattern().variables.size()),
      gradient(point.size()), entries(hessian.pattern().rows.size())
{
    team.fit(tape, kThreads);
    hessian.fit(kThreads);
}

void Threaded::run()
{
    forward(tape, point, team, pool, values);
    jacobian(tape, point, team, pool, values, gradients);
    weighted_gradient(tape, point, weights, team, pool, values, gradient);
    hessian.values(point, weights, entries, pool);
    hessian.product(point, weights, point, gradient, pool);
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
    graph.add(Operator::Atan2, x);
    graph.add(Operator::Max, x);
    graph.add(Operator::Min, x);
    for (const Operator op : kUnary)
    {
        graph.add(op, one);
    }
    const std::array kinks{-1.0, 0.5, 2.0};
    const std::array heights{1.0, 0.0, 3.0};
    const Offset pwl = graph.add_piecewise_linear(kinks, heights);
    graph.add_piecewise(pwl, one);
    graph.add_piecewise(pwl, std::span(x).subspan(1));
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
    EXPECT_EQ(compaction_cost(wide_sum(2)), compaction_cost(wide_sum(64)));
}

TEST(AllocationTest, SharingWorkspace)
{
    EXPECT_EQ(sharing_cost(wide_sum(2)), sharing_cost(wide_sum(64)));
}

TEST(AllocationTest, SharingTable)
{
    for (const Offset length : {16, 4096})
    {
        const Graph graph = chain(length);
        EXPECT_EQ(sharing_cost(graph), compaction_cost(graph));
    }
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
    const std::size_t before = allocations();
    enclose(graph, box, nodes);
    EXPECT_EQ(allocations(), before);
    EXPECT_FALSE(nodes.back().is_empty());
}

TEST(AllocationTest, JacobianSweep)
{
    Graph graph;
    const std::vector<Offset> roots = products(graph);
    const Tape tape(graph, roots, 4);
    ASSERT_GT(tape.prepass(), 0);
    EXPECT_EQ(jacobian_cost(tape), 0U);
}

TEST(AllocationTest, GradientSweep)
{
    Graph graph;
    const std::vector<Offset> roots = products(graph);
    const Tape tape(graph, roots, 4);
    ASSERT_GT(tape.prepass(), 0);
    EXPECT_EQ(gradient_cost(tape), 0U);
}

TEST(AllocationTest, HessianSweep)
{
    Graph graph;
    const std::vector<Offset> roots = products(graph);
    ASSERT_GT(Tape(graph, roots, 4).prepass(), 0);
    EXPECT_EQ(hessian_cost(graph, roots, 1), 0U);
    EXPECT_EQ(hessian_cost(graph, roots, kLanes), 0U);
}

TEST(AllocationTest, HessianProduct)
{
    Graph graph;
    const std::vector<Offset> roots = products(graph);
    EXPECT_EQ(product_cost(graph, roots, 1), 0U);
    EXPECT_EQ(product_cost(graph, roots, kLanes), 0U);
}

TEST(AllocationTest, Threads)
{
    Graph graph;
    const std::vector<Offset> roots = ring(graph);
    Threaded threaded(graph, roots);
    ASSERT_GT(threaded.tape.prepass(), 0);
    threaded.run();
    const std::size_t start = allocations();
    threaded.run();
    EXPECT_EQ(allocations(), start);
    EXPECT_EQ(threaded.pool.workers(), kThreads - 1);
}

TEST(AllocationTest, Curvature)
{
    const Graph graph = every_operator();
    std::vector<Interval> nodes(static_cast<std::size_t>(graph.size()), Interval::empty());
    const std::array box{Interval(-0.5, 2.0), Interval(-1.0, 3.0)};
    enclose(graph, box, nodes);
    Shapes shapes(graph);
    const std::size_t before = allocations();
    shapes.classify(graph, nodes);
    EXPECT_EQ(allocations(), before);
    EXPECT_EQ(shapes.curvature(0), biyos::Curvature::Affine);
    EXPECT_EQ(shapes.curvature(graph.size() - 1), biyos::Curvature::Convex);
}

TEST(AllocationTest, RunningProduct)
{
    EXPECT_LT(compile_bytes(4 * kSteps), kGrowth * compile_bytes(kSteps));
}

TEST(AllocationTest, SharedSum)
{
    EXPECT_LT(shared_bytes(4 * kTerms), kGrowth * shared_bytes(kTerms));
}
