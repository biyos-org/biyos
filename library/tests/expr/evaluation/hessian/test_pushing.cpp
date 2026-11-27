#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
#include "expr/evaluation/hessian/pushing.hpp"
#include "expr/evaluation/hessian/sweep.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace
{

using biyos::Elimination;
using biyos::elimination_of;
using biyos::Entry;
using biyos::Graph;
using biyos::Index;
using biyos::Lists;
using biyos::lists_of;
using biyos::Offset;
using biyos::Operator;
using biyos::Pass;
using biyos::Pusher;
using biyos::Real;
using biyos::Tape;
using biyos::to_size;
using biyos::Tolerances;
using biyos::variables_of;
using biyos::Workspace;
using biyos::test::arrow;
using biyos::test::band;
using biyos::test::binary;
using biyos::test::bits;
using biyos::test::clique;
using biyos::test::default_tolerances;
using biyos::test::every_operator;
using biyos::test::kBudgets;
using biyos::test::listed;
using biyos::test::products;
using biyos::test::Random;
using biyos::test::unary;
using biyos::test::variables;
using biyos::test::waves_of;

using Values = std::vector<Real>;
using Rows = std::vector<Values>;

constexpr std::array kSizes{3, 17, 33, 64, 200};
constexpr std::uint64_t kSeeds = 4;
constexpr Index kSubset = 64;
constexpr Index kMany = 200;
constexpr Real kBound = 1e-6;
constexpr std::array kOperators{0.7, 1.3, 0.4, 0.3};

struct Pushed
{
    std::vector<Elimination> eliminations;
    Lists lists;
    Workspace workspace;
};

Pushed pushed_of(const Tape &tape)
{
    Pusher pusher(tape);
    std::vector<Elimination> eliminations;
    for (Offset e = 0; e < tape.roots(); ++e)
    {
        eliminations.push_back(elimination_of(e, pusher.shape(e)));
    }
    Lists lists = lists_of(tape, eliminations);
    return {.eliminations = std::move(eliminations),
            .lists = std::move(lists),
            .workspace = Workspace(tape)};
}

Values parts_of(const Tape &tape, Pushed &pushed, Offset e, std::span<const Real> point)
{
    const Elimination &elimination = pushed.eliminations[to_size(e)];
    Values parts(elimination.parts.size(), -1.0);
    push(tape, elimination, point, default_tolerances(), pushed.workspace, pushed.lists, parts);
    return parts;
}

Values column_of(const Tape &tape,
                 Offset e,
                 std::span<const Real> point,
                 const Tolerances &tolerances,
                 Index variable)
{
    Pass pass(tape, 1);
    pass.seeds[to_size(variable) * pass.lanes] = 1.0;
    sweep(tape, e, point, tolerances, pass);
    Values column;
    for (std::size_t i = 0; i < to_size(tape.variables()); ++i)
    {
        column.push_back(pass.second.gradient[i * pass.lanes]);
    }
    return column;
}

Rows reference(const Tape &tape, Offset e, std::span<const Real> point)
{
    const Tolerances tolerances = default_tolerances();
    Rows columns;
    for (const Index variable : variables_of(tape.pattern(), e))
    {
        columns.push_back(column_of(tape, e, point, tolerances, variable));
    }
    return columns;
}

bool near(Real value, Real wanted)
{
    if (std::isnan(wanted))
    {
        return std::isnan(value);
    }
    if (std::isinf(wanted))
    {
        return std::isinf(value) && std::signbit(value) == std::signbit(wanted);
    }
    return std::fabs(value - wanted) <= kBound * (1.0 + std::fabs(wanted));
}

void expect_element(const Tape &tape, Pushed &pushed, Offset e, std::span<const Real> point)
{
    const Values parts = parts_of(tape, pushed, e, point);
    const Rows columns = reference(tape, e, point);
    const std::span<const Index> variables = variables_of(tape.pattern(), e);
    std::size_t k = 0;
    Pusher pusher(tape);
    for (const Entry entry : pusher.shape(e).entries)
    {
        const auto row = to_size(variables[to_size(entry.row)]);
        const Real wanted = columns[to_size(entry.column)][row];
        EXPECT_TRUE(near(parts[k], wanted))
            << e << " " << entry.row << " " << entry.column << " " << parts[k] << " " << wanted;
        ++k;
    }
}

void expect_tape(const Tape &tape, std::span<const Real> point)
{
    Pushed pushed = pushed_of(tape);
    for (Offset e = 0; e < tape.roots(); ++e)
    {
        expect_element(tape, pushed, e, point);
    }
}

Offset subset(Graph &graph, const std::vector<Offset> &x)
{
    return clique(graph, x, waves_of(graph, x), kSubset);
}

std::vector<Offset> shapes_of(Graph &graph, Index size, Random &random)
{
    const std::vector<Offset> x = variables(graph, size);
    return {arrow(graph, x), band(graph, x), products(graph, x, random)};
}

Values point_of(Random &random, Index size)
{
    Values point;
    for (Index k = 0; k < size; ++k)
    {
        point.push_back(random.uniform(0.2, 1.2));
    }
    return point;
}

TEST(PushingTest, Shapes)
{
    for (const Index size : kSizes)
    {
        for (std::uint64_t seed = 0; seed < kSeeds; ++seed)
        {
            SCOPED_TRACE(testing::Message() << size << " " << seed);
            Random random(seed);
            Graph graph;
            const std::vector<Offset> roots = shapes_of(graph, size, random);
            expect_tape(Tape(graph, roots), point_of(random, size));
        }
    }
}

TEST(PushingTest, Subset)
{
    Random random(7);
    Graph graph;
    const std::vector<Offset> x = variables(graph, kMany);
    const std::array root{subset(graph, x)};
    expect_tape(Tape(graph, root), point_of(random, kMany));
}

TEST(PushingTest, Operators)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    expect_tape(Tape(graph, roots), kOperators);
}

TEST(PushingTest, Routed)
{
    Random random(3);
    Graph graph;
    const std::vector<Offset> roots = shapes_of(graph, kSizes[2], random);
    const Values point = point_of(random, kSizes[2]);
    for (const Offset budget : kBudgets)
    {
        SCOPED_TRACE(budget);
        const Tape tape(graph, roots, budget);
        expect_tape(tape, point);
    }
}

TEST(PushingTest, Repeat)
{
    Random random(5);
    Graph graph;
    const std::vector<Offset> roots = shapes_of(graph, kSizes[1], random);
    const Tape tape(graph, roots);
    const Values first = point_of(random, kSizes[1]);
    const Values second = point_of(random, kSizes[1]);
    Pushed pushed = pushed_of(tape);
    for (Offset e = 0; e < tape.roots(); ++e)
    {
        Pushed fresh = pushed_of(tape);
        parts_of(tape, pushed, e, first);
        EXPECT_EQ(bits(parts_of(tape, pushed, e, second)), bits(parts_of(tape, fresh, e, second)))
            << e;
    }
}

TEST(PushingTest, Undefined)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const std::array roots{unary(graph, Operator::Log, x[0]), arrow(graph, x)};
    const Tape tape(graph, roots);
    const std::array point{-1.0, 0.5};
    Pushed pushed = pushed_of(tape);
    EXPECT_TRUE(std::isnan(parts_of(tape, pushed, 0, point).front()));
    expect_element(tape, pushed, 1, point);
}

TEST(PushingTest, Twice)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const std::array pair{x[0], x[0]};
    const Offset doubled = graph.add_sum(0.0, pair, std::array{1.0, 1.0});
    const std::array roots{unary(graph, Operator::Exp, doubled),
                           binary(graph, Operator::Division, x[1], x[1])};
    expect_tape(Tape(graph, roots), std::array{0.7, 1.3});
}

TEST(PushingTest, Shared)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset product = graph.add(Operator::Product, 1.0, std::array{x[0], x[1]});
    const Offset grown = unary(graph, Operator::Exp, product);
    const std::array waves{unary(graph, Operator::Sin, grown), unary(graph, Operator::Cos, grown)};
    const std::array roots{graph.add_sum(0.0, waves, std::array{1.0, 1.0})};
    expect_tape(Tape(graph, roots), std::array{0.4, 0.9});
}

TEST(PushingTest, Common)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset product = graph.add(Operator::Product, 1.0, std::array{x[0], x[1]});
    const Offset grown = unary(graph, Operator::Exp, product);
    const std::array roots{unary(graph, Operator::Sin, grown), unary(graph, Operator::Cos, grown)};
    const Tape tape(graph, roots, kBudgets.front());
    ASSERT_GT(tape.prepass(), 0);
    expect_tape(tape, std::array{0.4, 0.9});
}

TEST(PushingTest, Chosen)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 4);
    const std::array roots{listed(graph, Operator::Max, {x[0], unary(graph, Operator::Exp, x[1])}),
                           listed(graph, Operator::Max, {x[2], unary(graph, Operator::Log, x[3])})};
    expect_tape(Tape(graph, roots), std::array{0.0, 1.0, 0.0, -1.0});
}

TEST(PushingTest, Marked)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 1);
    const std::array roots{unary(graph, Operator::Entropy, x[0])};
    const Tape tape(graph, roots);
    Pushed pushed = pushed_of(tape);
    EXPECT_TRUE(std::isnan(parts_of(tape, pushed, 0, std::array{-1.0}).front()));
    expect_element(tape, pushed, 0, std::array{-1.0});
}

TEST(PushingTest, Boundary)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 4);
    const Offset fallen = unary(graph, Operator::Negation, unary(graph, Operator::Exp, x[3]));
    const std::array roots{
        listed(graph, Operator::Max, {x[0], unary(graph, Operator::Sqrt, x[1])}),
        unary(graph, Operator::Sqrt, listed(graph, Operator::Max, {x[2], fallen}))};
    expect_tape(Tape(graph, roots), std::array{1.0, 0.0, 0.0, 0.0});
}

}
