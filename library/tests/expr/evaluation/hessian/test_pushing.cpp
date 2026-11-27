#include "core/env/env.hpp"
#include "core/env/pool.hpp"
#include "core/events/event.hpp"
#include "core/events/events.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/timing/timer.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/hessian.hpp"
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
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::Elimination;
using biyos::elimination_of;
using biyos::Entry;
using biyos::Env;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::Field;
using biyos::Graph;
using biyos::Hessian;
using biyos::Index;
using biyos::Lists;
using biyos::lists_of;
using biyos::Lower;
using biyos::Offset;
using biyos::Operator;
using biyos::Pass;
using biyos::Pool;
using biyos::Pusher;
using biyos::Real;
using biyos::Tape;
using biyos::TimingTree;
using biyos::to_index;
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
constexpr Index kTie = 17;
constexpr Index kHead = 6;
constexpr Real kBound = 1e-6;
constexpr std::array kOperators{0.7, 1.3, 0.4, 0.3};
constexpr std::array kWeights{1.0, -2.5, 0.0};

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

Hessian hessian_of(const Graph &graph, std::span<const Offset> roots)
{
    Env env;
    TimingTree tree;
    return Hessian(graph, roots, {}, {env.stream(), tree});
}

Values values_of(Hessian &hessian, std::span<const Real> point, Real weight)
{
    Values out(hessian.pattern().rows.size(), -1.0);
    Pool pool;
    hessian.values(point, std::array{weight}, default_tolerances(), out, pool);
    return out;
}

void expect_hessian(const Graph &graph, Offset root, std::span<const Real> point, Real weight)
{
    const std::array roots{root};
    Hessian hessian = hessian_of(graph, roots);
    ASSERT_TRUE(hessian.pushed(0));
    const Values out = values_of(hessian, point, weight);
    const Tape tape(graph, roots);
    const Lower &lower = hessian.pattern();
    for (std::size_t j = 0; j + 1 < lower.starts.size(); ++j)
    {
        const Values column = column_of(tape, 0, point, default_tolerances(), to_index(j));
        for (Offset k = lower.starts[j]; k < lower.starts[j + 1]; ++k)
        {
            const Real wanted = weight * column[to_size(lower.rows[to_size(k)])];
            EXPECT_TRUE(near(out[to_size(k)], wanted)) << j << " " << out[to_size(k)];
        }
    }
}

std::string line_of(const Event &event)
{
    std::string line;
    for (const Field &field : event.fields)
    {
        line += field.name + "=" + field.value + " ";
    }
    return line;
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

TEST(PushingTest, Choice)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, kMany);
    const std::array roots{arrow(graph, x), subset(graph, x)};
    const Hessian hessian = hessian_of(graph, roots);
    EXPECT_TRUE(hessian.colored(0));
    EXPECT_FALSE(hessian.pushed(0));
    EXPECT_FALSE(hessian.colored(1));
    EXPECT_TRUE(hessian.pushed(1));
}

TEST(PushingTest, Event)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, kMany);
    const std::vector<Offset> few(x.begin(), x.begin() + kTie);
    const Offset total = graph.add_sum(0.0, x, Values(x.size(), 1.0));
    const std::array roots{arrow(graph, x),
                           subset(graph, x),
                           clique(graph, few, waves_of(graph, few), kHead),
                           unary(graph, Operator::Exp, total)};
    Env env;
    std::vector<std::string> lines;
    const auto keep = [&lines](const Event &event) { lines.push_back(line_of(event)); };
    env.events().add_hook(EventKind::Message, EventLevel::Debug, keep);
    TimingTree tree;
    const Hessian hessian(graph, roots, {}, {env.stream(), tree});
    const std::vector<std::string> wanted{
        "element=0 method=coloring pushes=802 sweeps=1 length=403 ",
        "element=1 method=pushing pushes=2281 sweeps=13 length=405 ",
        "element=2 method=coloring pushes=39 sweeps=1 length=39 ",
        "element=3 method=dense pushes=20101 sweeps=13 length=202 ",
    };
    EXPECT_EQ(lines, wanted);
}

TEST(PushingTest, Values)
{
    Random random(11);
    Graph graph;
    const std::vector<Offset> x = variables(graph, kMany);
    const Offset root = subset(graph, x);
    const Values point = point_of(random, kMany);
    for (const Real weight : kWeights)
    {
        SCOPED_TRACE(weight);
        expect_hessian(graph, root, point, weight);
    }
}

TEST(PushingTest, Zero)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, kMany);
    const std::array factors{waves_of(graph, x), unary(graph, Operator::Log, x[0])};
    const Offset tail = graph.add(Operator::Product, 1.0, factors);
    const std::array roots{clique(graph, x, tail, kSubset)};
    Hessian hessian = hessian_of(graph, roots);
    ASSERT_TRUE(hessian.pushed(0));
    Values point(to_size(kMany), 0.5);
    values_of(hessian, point, 1.0);
    point.front() = -1.0;
    const Values out = values_of(hessian, point, 0.0);
    EXPECT_EQ(bits(out), bits(Values(out.size(), 0.0)));
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
