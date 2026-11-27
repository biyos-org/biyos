#include "chain_rule.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/evaluation/derivatives/derivative.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "refusal.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Adjoints;
using biyos::Graph;
using biyos::Index;
using biyos::jacobian;
using biyos::kBudget;
using biyos::Offset;
using biyos::Operator;
using biyos::Pattern;
using biyos::Real;
using biyos::Tape;
using biyos::to_size;
using biyos::Tolerances;
using biyos::weighted_gradient;
using biyos::Workspace;
using biyos::test::binary;
using biyos::test::ChainRule;
using biyos::test::constant;
using biyos::test::default_tolerances;
using biyos::test::every_operator;
using biyos::test::gradient_of;
using biyos::test::jacobian_of;
using biyos::test::kBudgets;
using biyos::test::kRoots;
using biyos::test::kVariables;
using biyos::test::listed;
using biyos::test::picks;
using biyos::test::power;
using biyos::test::Random;
using biyos::test::random_graph;
using biyos::test::random_point;
using biyos::test::refusal;
using biyos::test::unary;
using biyos::test::values_of;

using Rows = std::vector<std::vector<Real>>;

constexpr Real kStep = 1e-5;
constexpr Real kBound = 1e-6;
constexpr Real kRounding = 1e-12;
constexpr Real kNan = (std::numeric_limits<Real>::quiet_NaN)();
constexpr Real kInfinity = (std::numeric_limits<Real>::infinity)();

std::vector<Index> reached(const Graph &graph, Offset root)
{
    std::vector<bool> seen(to_size(graph.size()), false);
    std::vector<Offset> stack{root};
    std::vector<Index> variables;
    while (!stack.empty())
    {
        const Offset node = stack.back();
        stack.pop_back();
        if (seen[to_size(node)])
        {
            continue;
        }
        seen[to_size(node)] = true;
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
    const Pattern &pattern = tape.pattern();
    const auto first = pattern.variables.begin();
    return {first + pattern.starts[root], first + pattern.starts[root + 1]};
}

Rows dense(const Tape &tape, const std::vector<Real> &gradients, std::size_t width)
{
    const Pattern &pattern = tape.pattern();
    Rows rows(to_size(tape.roots()), std::vector<Real>(width, 0.0));
    for (std::size_t r = 0; r < rows.size(); ++r)
    {
        for (Offset k = pattern.starts[r]; k < pattern.starts[r + 1]; ++k)
        {
            rows[r][to_size(pattern.variables[to_size(k)])] = gradients[to_size(k)];
        }
    }
    return rows;
}

std::vector<Real> differences(const Tape &tape,
                              std::vector<Real> point,
                              std::size_t variable,
                              Real step)
{
    const Real middle = point[variable];
    const Real size = step * (std::max)(1.0, std::fabs(middle));
    point[variable] = middle + size;
    const Real up = point[variable];
    std::vector<Real> slopes = values_of(tape, point);
    point[variable] = middle - size;
    const Real down = point[variable];
    const std::vector<Real> below = values_of(tape, point);
    for (std::size_t k = 0; k < slopes.size(); ++k)
    {
        slopes[k] = (slopes[k] - below[k]) / (up - down);
    }
    return slopes;
}

Real tolerance(Real difference)
{
    return kBound * (1.0 + std::fabs(difference));
}

bool is_zero(Real value)
{
    return std::fpclassify(value) == FP_ZERO;
}

bool converged(Real fine, Real coarse)
{
    const bool finite = std::isfinite(fine) && std::isfinite(coarse);
    return finite && std::fabs(fine - coarse) <= tolerance(fine);
}

void expect_differences(const Tape &tape, const std::vector<Real> &point)
{
    const Rows rows = dense(tape, jacobian_of(tape, point), point.size());
    for (std::size_t j = 0; j < point.size(); ++j)
    {
        const std::vector<Real> wanted = differences(tape, point, j, kStep);
        for (std::size_t r = 0; r < rows.size(); ++r)
        {
            EXPECT_NEAR(rows[r][j], wanted[r], tolerance(wanted[r]))
                << "root " << r << ", variable " << j;
        }
    }
}

std::size_t check_converged(const Tape &tape, const std::vector<Real> &point, const Rows &rows)
{
    std::size_t checked = 0;
    for (std::size_t j = 0; j < point.size(); ++j)
    {
        const std::vector<Real> fine = differences(tape, point, j, kStep);
        const std::vector<Real> coarse = differences(tape, point, j, 2.0 * kStep);
        for (std::size_t r = 0; r < rows.size(); ++r)
        {
            if (converged(fine[r], coarse[r]) && std::isfinite(rows[r][j]))
            {
                ++checked;
                EXPECT_NEAR(rows[r][j], fine[r], tolerance(fine[r]))
                    << "root " << r << ", variable " << j;
            }
        }
    }
    return checked;
}

void expect_same(Real found, Real wanted, Real scale)
{
    if (std::isnan(wanted))
    {
        EXPECT_TRUE(std::isnan(found)) << found;
    }
    else if (std::isinf(wanted))
    {
        EXPECT_DOUBLE_EQ(found, wanted);
    }
    else
    {
        EXPECT_NEAR(found, wanted, kRounding * scale);
    }
}

void expect_agree(Real found, Real wanted, Real scale)
{
    if (std::isfinite(wanted))
    {
        EXPECT_NEAR(found, wanted, kRounding * scale);
        return;
    }
    EXPECT_FALSE(std::isfinite(found)) << found;
}

void expect_rule(const Graph &graph,
                 std::span<const Offset> roots,
                 const std::vector<Real> &point,
                 const Rows &rows)
{
    ChainRule rule(graph, point);
    for (std::size_t r = 0; r < roots.size(); ++r)
    {
        rule.differentiate(roots[r]);
        for (std::size_t j = 0; j < point.size(); ++j)
        {
            SCOPED_TRACE(testing::Message() << "root " << r << ", variable " << j);
            expect_agree(rows[r][j], rule.entry(j), rule.scale(j));
        }
    }
}

void expect_entries(const std::vector<Real> &found, const std::vector<Real> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        SCOPED_TRACE(k);
        expect_same(found[k], wanted[k], 0.0);
    }
}

struct Call
{
    const Tape *tape;
    std::span<const Real> point;
    std::span<const Real> weights;
    Workspace *workspace;
    Adjoints *adjoints;
    std::span<Real> values;
    std::span<Real> out;
};

void call_jacobian(const Call &call)
{
    Workspace &workspace = *call.workspace;
    Adjoints &adjoints = *call.adjoints;
    const Tolerances tolerances = default_tolerances();
    jacobian(*call.tape, call.point, tolerances, workspace, adjoints, call.values, call.out);
}

void call_weighted(const Call &call)
{
    const Tape &tape = *call.tape;
    Workspace &workspace = *call.workspace;
    Adjoints &adjoints = *call.adjoints;
    const Tolerances tolerances = default_tolerances();
    weighted_gradient(tape,
                      call.point,
                      call.weights,
                      tolerances,
                      workspace,
                      adjoints,
                      call.values,
                      call.out);
}

std::string refused(void (*sweep)(const Call &), const Call &call)
{
    return refusal([&] { sweep(call); });
}

bool cleared(const Adjoints &adjoints)
{
    return std::ranges::all_of(adjoints.slots, is_zero) &&
           std::ranges::all_of(adjoints.shared, is_zero) &&
           std::ranges::all_of(adjoints.gradient, is_zero);
}

void expect_close(const std::vector<Real> &found, const std::vector<Real> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_NEAR(found[k], wanted[k], kRounding * (1.0 + std::fabs(wanted[k]))) << k;
    }
}

void expect_weighted(const Tape &tape,
                     const std::vector<Real> &point,
                     const std::vector<Real> &weights,
                     const std::vector<Real> &found)
{
    const Rows rows = dense(tape, jacobian_of(tape, point), point.size());
    for (std::size_t j = 0; j < point.size(); ++j)
    {
        Real wanted = 0.0;
        Real scale = 1.0;
        for (std::size_t r = 0; r < rows.size(); ++r)
        {
            wanted += weights[r] * rows[r][j];
            scale += std::fabs(weights[r] * rows[r][j]);
        }
        if (std::isfinite(wanted))
        {
            EXPECT_NEAR(found[j], wanted, kRounding * scale) << j;
        }
        else
        {
            EXPECT_FALSE(std::isfinite(found[j])) << j;
        }
    }
}

std::vector<Real> random_weights(Random &random, std::size_t n)
{
    std::vector<Real> weights;
    for (std::size_t k = 0; k < n; ++k)
    {
        weights.push_back(random.uniform(-2.0, 2.0));
    }
    return weights;
}

std::vector<Real> interior_point(Random &random)
{
    std::vector<Real> point;
    for (Index k = 0; k < kVariables; ++k)
    {
        point.push_back(random.uniform(0.1, 2.0));
    }
    return point;
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
    const std::vector<Real> ones(waves.size(), 1.0);
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
    const std::vector<Real> ones(terms.size(), 1.0);
    return {graph.add_sum(0.0, terms, ones), total, terms[2]};
}

Offset sum_of(Graph &graph, Index start, Index size)
{
    std::vector<Offset> terms;
    for (Index k = start; k < start + size; ++k)
    {
        terms.push_back(graph.add_variable(k));
    }
    const std::vector<Real> ones(terms.size(), 1.0);
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
        level.push_back(unary(graph, Operator::Sin, sums[to_size(j % 2)]));
    }
    std::vector<Offset> next;
    for (Index i = 0; i < size; ++i)
    {
        next.clear();
        for (Index j = 0; j < size; ++j)
        {
            const std::array pair{level[to_size(j)], level[to_size((j + 1) % size)]};
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
    const std::vector<Real> ones(terms.size(), 1.0);
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

std::vector<Offset> shared_roots(Graph &graph)
{
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset z = graph.add_variable(2);
    const Offset product = graph.add(Operator::Product, 1.5, std::array{x, y});
    const Offset sine = unary(graph, Operator::Sin, product);
    const Offset sum = graph.add_sum(0.5, std::array{sine, product}, std::array{2.0, -1.0});
    const Offset ratio = binary(graph, Operator::Division, sum, z);
    return {
        graph.add(Operator::Product, 1.0, std::array{ratio, x}),
        unary(graph, Operator::Cos, sum),
        sum,
        unary(graph, Operator::Sqrt, z),
        listed(graph, Operator::Max, {sine, y}),
        y,
        constant(graph, 2.0),
        ratio,
    };
}

std::vector<Real> row(const Tape &tape, const std::vector<Real> &point, std::size_t root)
{
    return dense(tape, jacobian_of(tape, point), point.size())[root];
}

}

TEST(DerivativeTest, Operators)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    const Tape tape(graph, roots);
    const std::array points{
        std::vector{0.7, 1.9, -1.3, 0.3},
        std::vector{2.5, 1.25, 4.0, -0.8},
        std::vector{1.1, 3.0, -0.6, 0.95},
    };
    for (const std::vector<Real> &point : points)
    {
        SCOPED_TRACE(point[0]);
        expect_differences(tape, point);
    }
}

TEST(DerivativeTest, Seeded)
{
    std::size_t checked = 0;
    std::size_t entries = 0;
    Offset pinned = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kRoots);
        const Tape tape(graph, roots, kBudgets[random.below(kBudgets.size())]);
        pinned += tape.pinned();
        const Tolerances tolerances = default_tolerances();
        Workspace workspace(tape);
        Adjoints adjoints(tape);
        std::vector<Real> values(roots.size());
        std::vector<Real> gradients(tape.pattern().variables.size());
        for (int k = 0; k < 6; ++k)
        {
            const bool inside = k % 2 == 0;
            const std::vector<Real> point = inside ? interior_point(random) : random_point(random);
            jacobian(tape, point, tolerances, workspace, adjoints, values, gradients);
            EXPECT_TRUE(cleared(adjoints));
            const Rows rows = dense(tape, gradients, point.size());
            expect_rule(graph, roots, point, rows);
            if (inside)
            {
                checked += check_converged(tape, point, rows);
                entries += roots.size() * point.size();
            }
        }
    }
    EXPECT_GT(pinned, 0);
    EXPECT_GT(2 * checked, entries);
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
    for (const Offset budget : {Offset{1}, Offset{3}, kBudget})
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
                const std::vector<Real> point(to_size(tape.variables()), 0.5);
                const Rows rows = dense(tape, jacobian_of(tape, point), point.size());
                expect_rule(graph, roots, point, rows);
            }
        }
    }
}

TEST(DerivativeTest, Shared)
{
    Graph graph;
    const std::vector<Offset> roots = shared_roots(graph);
    const std::vector<Real> point{0.7, 1.3, 2.1};
    const Tape whole(graph, roots);
    ASSERT_EQ(whole.pinned(), 0);
    expect_differences(whole, point);
    const std::vector<Real> wanted = jacobian_of(whole, point);

    const Tape chained(graph, roots, 1);
    ASSERT_EQ(chained.prepass(), 4);
    const Tape grouped(graph, roots, 8);
    ASSERT_EQ(grouped.prepass(), 1);
    ASSERT_EQ(grouped.blocks()[0].outputs.size(), 4U);
    for (const Offset budget : {Offset{1}, Offset{2}, Offset{5}, Offset{8}})
    {
        SCOPED_TRACE(budget);
        const Tape split(graph, roots, budget);
        EXPECT_EQ(split.pinned(), 4);
        expect_differences(split, point);
        expect_close(jacobian_of(split, point), wanted);
    }
}

TEST(DerivativeTest, Kinks)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset z = graph.add_variable(2);
    const Offset gap = graph.add_sum(0.0, std::array{x, y}, std::array{1.0, -1.0});
    const std::vector<Offset> roots{
        unary(graph, Operator::Abs, x),
        unary(graph, Operator::Abs, gap),
        listed(graph, Operator::Max, {x, y}),
        listed(graph, Operator::Max, {y, x}),
        listed(graph, Operator::Min, {z, y, x}),
        listed(graph, Operator::Max, {x, x}),
        unary(graph, Operator::Sqrt, x),
        unary(graph, Operator::Log, x),
        power(graph, x, 0.5),
        power(graph, x, 0.0),
    };
    const Tape tape(graph, roots);
    const std::vector<Real> wanted{
        1.0,
        1.0,
        -1.0,
        1.0,
        0.0,
        0.0,
        1.0,
        0.0,
        1.0,
        0.0,
        1.0,
        kInfinity,
        kInfinity,
        kInfinity,
        0.0,
    };
    for (const Real zero : {0.0, -0.0})
    {
        SCOPED_TRACE(zero);
        expect_entries(jacobian_of(tape, std::vector{zero, zero, 1.0}), wanted);
    }
    EXPECT_EQ(row(tape, {0.5, 0.5, 0.5}, 4), (std::vector<Real>{0.0, 0.0, 1.0}));
}

TEST(DerivativeTest, Routing)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset zero = constant(graph, 0.0);
    const Offset one = constant(graph, 1.0);
    const Offset flip = unary(graph, Operator::Negation, x);
    const std::vector<Offset> roots{
        unary(graph, Operator::Sqrt, listed(graph, Operator::Max, {zero, x})),
        unary(graph, Operator::Log, listed(graph, Operator::Max, {x, zero})),
        unary(graph, Operator::Sqrt, listed(graph, Operator::Min, {one, flip})),
    };
    const Tape tape(graph, roots);
    expect_entries(jacobian_of(tape, std::vector{-1.0}), {0.0, 0.0, 0.0});
    expect_entries(jacobian_of(tape, std::vector{-0.5}), {0.0, 0.0, -0.5 / std::sqrt(0.5)});
}

TEST(DerivativeTest, ZeroAdjoint)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset root = unary(graph, Operator::Sqrt, x);
    const std::vector<Offset> roots{
        listed(graph, Operator::Max, {root, constant(graph, 1.0)}),
        graph.add(Operator::Product, 1.0, std::array{y, root}),
        graph.add(Operator::Product, 0.0, std::array{y, root}),
        power(graph, root, 2.0),
        unary(graph, Operator::Cos, root),
    };
    const Tape tape(graph, roots);
    const std::vector<Real> flat{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    expect_entries(jacobian_of(tape, std::vector{0.0, 0.0}), flat);
    const std::vector<Real> steep{0.0, kInfinity, 0.0, 0.0, 0.0, 0.0, 0.0};
    expect_entries(jacobian_of(tape, std::vector{0.0, 2.0}), steep);
}

TEST(DerivativeTest, NanValues)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::vector<Offset> roots{
        graph.add_sum(0.0, std::array{x, y}, std::array{1.0, -1.0}),
        graph.add(Operator::Product, 1.0, std::array{x, y}),
        unary(graph, Operator::Negation, x),
        listed(graph, Operator::Max, {y, x}),
    };
    const Tape tape(graph, roots);
    expect_entries(jacobian_of(tape, std::vector{kInfinity, kInfinity}),
                   {kNan, kNan, kInfinity, kInfinity, -1.0, 0.0, 1.0});
    expect_entries(jacobian_of(tape, std::vector{0.0, kInfinity}),
                   {1.0, -1.0, kNan, kNan, -1.0, 0.0, 1.0});
    expect_entries(jacobian_of(tape, std::vector{kNan, 1.0}),
                   {kNan, kNan, kNan, kNan, kNan, kNan, kNan});
}

TEST(DerivativeTest, Weighted)
{
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kRoots);
        const Tape tape(graph, roots, kBudgets[random.below(kBudgets.size())]);
        const Tolerances tolerances = default_tolerances();
        Workspace workspace(tape);
        Adjoints adjoints(tape);
        std::vector<Real> values(roots.size());
        std::vector<Real> gradient(kVariables);
        for (int k = 0; k < 2; ++k)
        {
            const std::vector<Real> point = random_point(random);
            const std::vector<Real> weights = random_weights(random, roots.size());
            std::ranges::fill(gradient, kNan);
            weighted_gradient(tape,
                              point,
                              weights,
                              tolerances,
                              workspace,
                              adjoints,
                              values,
                              gradient);
            EXPECT_TRUE(cleared(adjoints));
            expect_weighted(tape, point, weights, gradient);
        }
    }
    Graph graph;
    const std::vector<Offset> roots = shared_roots(graph);
    const std::vector<Real> point{0.7, 1.3, 2.1, 0.4};
    Random random(7);
    for (const Offset budget : {Offset{1}, Offset{2}, Offset{5}, Offset{8}, kBudget})
    {
        SCOPED_TRACE(budget);
        const Tape tape(graph, roots, budget);
        const std::vector<Real> weights = random_weights(random, roots.size());
        const std::vector<Real> gradient = gradient_of(tape, point, weights);
        expect_weighted(tape, point, weights, gradient);
        EXPECT_DOUBLE_EQ(gradient[3], 0.0);
    }
}

TEST(DerivativeTest, ZeroWeight)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::array roots{unary(graph, Operator::Sqrt, x), unary(graph, Operator::Sin, y)};
    const Tape tape(graph, roots);
    const std::array point{0.0, 1.0};
    const std::array weights{0.0, 1.0};
    expect_entries(gradient_of(tape, point, weights), {0.0, std::cos(1.0)});
}

TEST(DerivativeTest, Sizes)
{
    Graph graph;
    const Offset shared = unary(graph, Operator::Exp, graph.add_variable(1));
    const std::array roots{
        unary(graph, Operator::Sin, shared),
        unary(graph, Operator::Cos, shared),
    };
    const Tape tape(graph, roots, 3);
    ASSERT_EQ(tape.pinned(), 1);
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    std::vector<Real> values(2);
    std::vector<Real> gradients(2);
    const std::array point{0.5, 0.25};
    const Call call{
        .tape = &tape,
        .point = point,
        .weights = {},
        .workspace = &workspace,
        .adjoints = &adjoints,
        .values = values,
        .out = gradients,
    };
    Call bad = call;
    bad.point = std::span(point).first(1);
    EXPECT_EQ(refused(call_jacobian, bad),
              "invalid argument: point: size 1 does not cover variable 1");
    std::vector<Real> more(3);
    bad = call;
    bad.out = more;
    EXPECT_EQ(refused(call_jacobian, bad),
              "invalid argument: gradients: size 3, not the pattern size 2");
    Workspace unshared(Tape(graph, std::span(&shared, 1)));
    Workspace short_partials(tape);
    short_partials.partials.pop_back();
    for (Workspace *other : {&unshared, &short_partials})
    {
        bad = call;
        bad.workspace = other;
        EXPECT_EQ(refused(call_jacobian, bad),
                  "invalid argument: workspace: sized for another tape");
    }
    for (std::size_t k = 0; k < 3; ++k)
    {
        Adjoints small(tape);
        const std::array parts{&small.slots, &small.shared, &small.gradient};
        parts[k]->pop_back();
        bad = call;
        bad.adjoints = &small;
        EXPECT_EQ(refused(call_jacobian, bad),
                  "invalid argument: adjoints: sized for another tape");
    }
}

TEST(DerivativeTest, WeightSizes)
{
    Graph graph;
    const Offset shared = unary(graph, Operator::Exp, graph.add_variable(1));
    const std::array roots{
        unary(graph, Operator::Sin, shared),
        unary(graph, Operator::Cos, shared),
    };
    const Tape tape(graph, roots, 3);
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    std::vector<Real> values(2);
    std::vector<Real> gradient(2);
    const std::array point{0.5, 0.25};
    const std::array weights{1.0, -1.0};
    const Call call{
        .tape = &tape,
        .point = point,
        .weights = weights,
        .workspace = &workspace,
        .adjoints = &adjoints,
        .values = values,
        .out = gradient,
    };
    Call bad = call;
    bad.weights = std::span(weights).first(1);
    EXPECT_EQ(refused(call_weighted, bad),
              "invalid argument: weights: size 1, not the root count 2");
    std::vector<Real> more(3);
    bad = call;
    bad.out = more;
    EXPECT_EQ(refused(call_weighted, bad),
              "invalid argument: gradient: size 3, not the point size 2");
    Adjoints small(tape);
    small.shared.pop_back();
    bad = call;
    bad.adjoints = &small;
    EXPECT_EQ(refused(call_weighted, bad), "invalid argument: adjoints: sized for another tape");
    std::vector<Real> fewer(1);
    bad = call;
    bad.values = fewer;
    EXPECT_EQ(refused(call_weighted, bad),
              "invalid argument: values: size 1, not the root count 2");
}
