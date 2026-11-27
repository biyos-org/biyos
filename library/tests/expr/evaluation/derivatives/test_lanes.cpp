#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/evaluation/derivatives/derivative.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "refusal.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
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
using biyos::Bits;
using biyos::broadcast;
using biyos::forward;
using biyos::Graph;
using biyos::Index;
using biyos::is_zero;
using biyos::jacobian;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::seed_directions;
using biyos::Tape;
using biyos::to_size;
using biyos::weighted_gradient;
using biyos::Workspace;
using biyos::test::every_operator;
using biyos::test::gradient_of;
using biyos::test::jacobian_of;
using biyos::test::kBudgets;
using biyos::test::kRoots;
using biyos::test::kVariables;
using biyos::test::listed;
using biyos::test::picks;
using biyos::test::Random;
using biyos::test::random_graph;
using biyos::test::random_point;
using biyos::test::refusal;
using biyos::test::unary;
using biyos::test::values_of;

using Points = std::vector<std::vector<Real>>;

constexpr std::array kLanes{std::size_t{1}, std::size_t{3}, std::size_t{16}};

Points random_points(Random &random, std::size_t n)
{
    Points points;
    for (std::size_t l = 0; l < n; ++l)
    {
        points.push_back(random_point(random));
    }
    return points;
}

std::vector<Real> interleave(const Points &points)
{
    const std::size_t lanes = points.size();
    std::vector<Real> items(points[0].size() * lanes);
    for (std::size_t l = 0; l < lanes; ++l)
    {
        for (std::size_t k = 0; k < points[l].size(); ++k)
        {
            items[k * lanes + l] = points[l][k];
        }
    }
    return items;
}

std::vector<Real> lane(std::span<const Real> items, std::size_t l, std::size_t lanes)
{
    std::vector<Real> mine;
    for (std::size_t k = l; k < items.size(); k += lanes)
    {
        mine.push_back(items[k]);
    }
    return mine;
}

void expect_bits(std::span<const Real> found, std::span<const Real> wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        SCOPED_TRACE(k);
        if (std::isnan(wanted[k]))
        {
            EXPECT_TRUE(std::isnan(found[k])) << found[k];
            continue;
        }
        EXPECT_EQ(std::bit_cast<Bits>(found[k]), std::bit_cast<Bits>(wanted[k]))
            << found[k] << " against " << wanted[k];
    }
}

void expect_values(const Tape &tape, Workspace &workspace, const Points &points)
{
    const std::size_t lanes = points.size();
    std::vector<Real> values(to_size(tape.roots()) * lanes);
    forward(tape, interleave(points), workspace, values);
    for (std::size_t l = 0; l < lanes; ++l)
    {
        SCOPED_TRACE(l);
        expect_bits(lane(values, l, lanes), values_of(tape, points[l]));
    }
}

std::string refused(const Tape &tape,
                    Workspace &workspace,
                    std::span<const Real> point,
                    std::span<Real> values)
{
    return refusal([&] { forward(tape, point, workspace, values); });
}

bool cleared(const Adjoints &adjoints)
{
    return std::ranges::all_of(adjoints.slots, is_zero) &&
           std::ranges::all_of(adjoints.shared, is_zero) &&
           std::ranges::all_of(adjoints.gradient, is_zero);
}

Real weight(Random &random)
{
    return random.below(4) == 0 ? 0.0 : random.uniform(-2.0, 2.0);
}

Points random_weights(Random &random, const Tape &tape, std::size_t lanes)
{
    Points weights(lanes);
    for (std::vector<Real> &mine : weights)
    {
        for (Offset r = 0; r < tape.roots(); ++r)
        {
            mine.push_back(weight(random));
        }
    }
    return weights;
}

void expect_jacobian(const Tape &tape,
                     Workspace &workspace,
                     Adjoints &adjoints,
                     const Points &points)
{
    const std::size_t lanes = points.size();
    std::vector<Real> values(to_size(tape.roots()) * lanes);
    std::vector<Real> gradients(tape.pattern().variables.size() * lanes);
    jacobian(tape, interleave(points), workspace, adjoints, values, gradients);
    EXPECT_TRUE(cleared(adjoints));
    for (std::size_t l = 0; l < lanes; ++l)
    {
        SCOPED_TRACE(l);
        expect_bits(lane(values, l, lanes), values_of(tape, points[l]));
        expect_bits(lane(gradients, l, lanes), jacobian_of(tape, points[l]));
    }
}

void expect_weighted(const Tape &tape,
                     Workspace &workspace,
                     Adjoints &adjoints,
                     const Points &points,
                     const Points &weights)
{
    const std::size_t lanes = points.size();
    const std::vector<Real> point = interleave(points);
    std::vector<Real> values(to_size(tape.roots()) * lanes);
    std::vector<Real> gradient(point.size(), (std::numeric_limits<Real>::quiet_NaN)());
    weighted_gradient(tape, point, interleave(weights), workspace, adjoints, values, gradient);
    EXPECT_TRUE(cleared(adjoints));
    for (std::size_t l = 0; l < lanes; ++l)
    {
        SCOPED_TRACE(l);
        expect_bits(lane(values, l, lanes), values_of(tape, points[l]));
        expect_bits(lane(gradient, l, lanes), gradient_of(tape, points[l], weights[l]));
    }
}

void expect_sweeps(const Tape &tape, Random &random)
{
    for (const std::size_t lanes : kLanes)
    {
        SCOPED_TRACE(lanes);
        Workspace workspace(tape, lanes);
        Adjoints adjoints(tape, lanes);
        for (int k = 0; k < 2; ++k)
        {
            expect_jacobian(tape, workspace, adjoints, random_points(random, lanes));
            const Points points = random_points(random, lanes);
            expect_weighted(tape, workspace, adjoints, points, random_weights(random, tape, lanes));
        }
    }
}

Points special_points(std::size_t lanes)
{
    constexpr Real kInfinity = (std::numeric_limits<Real>::infinity)();
    constexpr Real kNan = (std::numeric_limits<Real>::quiet_NaN)();
    constexpr std::array kSpecial{0.0, -0.0, 1.0, -1.0, kInfinity, -kInfinity, kNan, 0.5};
    Points points(lanes);
    for (std::size_t l = 0; l < lanes; ++l)
    {
        for (std::size_t k = 0; k < static_cast<std::size_t>(kVariables); ++k)
        {
            points[l].push_back(kSpecial[(l + 3 * k * (l % 2)) % kSpecial.size()]);
        }
    }
    return points;
}

struct Sweep
{
    const Tape *tape;
    std::span<const Real> point;
    std::span<const Real> weights;
    Workspace *workspace;
    Adjoints *adjoints;
    std::span<Real> values;
    std::span<Real> out;
};

void run_jacobian(const Sweep &call)
{
    jacobian(*call.tape, call.point, *call.workspace, *call.adjoints, call.values, call.out);
}

void run_weighted(const Sweep &call)
{
    const Tape &tape = *call.tape;
    Workspace &workspace = *call.workspace;
    Adjoints &adjoints = *call.adjoints;
    weighted_gradient(tape, call.point, call.weights, workspace, adjoints, call.values, call.out);
}

std::string refused(void (*sweep)(const Sweep &), const Sweep &call)
{
    return refusal([&] { sweep(call); });
}

constexpr Real kStep = 0x1p-10;

std::vector<Real> stepped(const std::vector<Real> &point,
                          std::span<const Real> directions,
                          std::size_t lanes)
{
    std::vector<Real> points(point.size() * lanes);
    broadcast(point, lanes, points);
    for (std::size_t k = 0; k < points.size(); ++k)
    {
        points[k] += kStep * directions[k];
    }
    return points;
}

std::vector<Real> zeros()
{
    return std::vector<Real>(static_cast<std::size_t>(kVariables), 0.0);
}

std::vector<Real> unit(Index variable)
{
    std::vector<Real> direction = zeros();
    direction[static_cast<std::size_t>(variable)] = 1.0;
    return direction;
}

void expect_seeded(const Tape &tape, Random &random, std::size_t lanes)
{
    constexpr std::array<Index, 4> kOrder{2, 0, 3, 1};
    const std::size_t seeded = (std::min)(lanes, kOrder.size());
    const std::span<const Index> variables = std::span(kOrder).first(seeded);
    std::vector<Real> directions(static_cast<std::size_t>(kVariables) * lanes, 0.5);
    seed_directions(variables, lanes, directions);
    const std::vector<Real> point = random_point(random);
    Workspace workspace(tape, lanes);
    std::vector<Real> values(to_size(tape.roots()) * lanes);
    forward(tape, stepped(point, directions, lanes), workspace, values);
    for (std::size_t l = 0; l < lanes; ++l)
    {
        SCOPED_TRACE(l);
        const std::vector<Real> mine = lane(directions, l, lanes);
        expect_bits(mine, l < seeded ? unit(variables[l]) : zeros());
        expect_bits(lane(values, l, lanes), values_of(tape, stepped(point, mine, 1)));
    }
}

std::string seed_refusal(std::span<const Index> variables,
                         std::size_t lanes,
                         std::span<Real> directions)
{
    return refusal([&] { seed_directions(variables, lanes, directions); });
}

Tape shared_tape(Graph &graph)
{
    const Offset shared = unary(graph, Operator::Exp, graph.add_variable(1));
    const std::array roots{
        unary(graph, Operator::Sin, shared),
        unary(graph, Operator::Cos, shared),
    };
    return {graph, roots, 3};
}

Tape wide_tape(Graph &graph)
{
    const std::array pair{graph.add_variable(0), graph.add_variable(2)};
    const Offset product = graph.add(Operator::Product, 2.0, pair);
    const Offset shared = unary(graph, Operator::Exp, product);
    const Offset other = unary(graph, Operator::Sin, product);
    const std::array both{shared, other};
    const std::array roots{
        graph.add(Operator::Product, 1.0, both),
        unary(graph, Operator::Cos, shared),
        unary(graph, Operator::Tan, other),
    };
    return {graph, roots, 3};
}

std::string overflow(std::size_t size)
{
    const std::size_t lanes = (std::numeric_limits<std::size_t>::max)() / size + 1;
    return "invalid argument: lanes: " + std::to_string(lanes) + " times size ";
}

std::string prefix(const std::string &message, const std::string &wanted)
{
    return message.substr(0, wanted.size());
}
}

TEST(LaneTest, Sizes)
{
    Graph graph;
    const Tape tape = shared_tape(graph);
    ASSERT_EQ(tape.pinned(), 1);
    const Workspace workspace(tape, 3);
    EXPECT_EQ(workspace.lanes, 3U);
    EXPECT_EQ(workspace.shared.size(), 3 * to_size(tape.pinned()));
    EXPECT_EQ(workspace.slots.size(), 3 * tape.width());
    EXPECT_EQ(workspace.partials.size(), 3 * to_size(tape.partials()));
    EXPECT_EQ(workspace.result.size(), 3U);
    const Adjoints adjoints(tape, 3);
    EXPECT_EQ(adjoints.lanes, 3U);
    EXPECT_EQ(adjoints.slots.size(), 3 * tape.width());
    EXPECT_EQ(adjoints.shared.size(), 3 * to_size(tape.pinned()));
    EXPECT_EQ(adjoints.gradient.size(), 3 * to_size(tape.variables()));
    EXPECT_EQ(Workspace(tape).lanes, 1U);
    EXPECT_EQ(Adjoints(tape).lanes, 1U);
    EXPECT_EQ(refusal([&] { Workspace{tape, 0}; }), "invalid argument: lanes: zero");
    EXPECT_EQ(refusal([&] { Adjoints{tape, 0}; }), "invalid argument: lanes: zero");
}

TEST(LaneTest, Values)
{
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kRoots);
        const Tape tape(graph, roots, kBudgets[random.below(kBudgets.size())]);
        for (const std::size_t lanes : kLanes)
        {
            SCOPED_TRACE(lanes);
            Workspace workspace(tape, lanes);
            expect_values(tape, workspace, random_points(random, lanes));
            expect_values(tape, workspace, random_points(random, lanes));
        }
    }
}

TEST(LaneTest, Operators)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    const Tape tape(graph, roots);
    Random random(5);
    for (const std::size_t lanes : kLanes)
    {
        SCOPED_TRACE(lanes);
        Workspace workspace(tape, lanes);
        for (int k = 0; k < 4; ++k)
        {
            expect_values(tape, workspace, random_points(random, lanes));
        }
    }
}

TEST(LaneTest, Refused)
{
    Graph graph;
    const Tape tape = shared_tape(graph);
    Workspace workspace(tape, 3);
    std::vector<Real> values(6);
    const std::vector<Real> point(6, 0.5);
    EXPECT_EQ(refused(tape, workspace, std::span(point).first(5), values),
              "invalid argument: point: size 5 is not a multiple of 3 lanes");
    EXPECT_EQ(refused(tape, workspace, std::span(point).first(3), values),
              "invalid argument: point: size 3 does not cover variable 1 in 3 lanes");
    EXPECT_EQ(refused(tape, workspace, point, std::span(values).first(2)),
              "invalid argument: values: size 2, not the root count 2 in 3 lanes");
    std::vector<Real> more(7);
    EXPECT_EQ(refused(tape, workspace, point, more),
              "invalid argument: values: size 7, not the root count 2 in 3 lanes");
    const std::string other = "invalid argument: workspace: sized for another tape";
    workspace.result.pop_back();
    EXPECT_EQ(refused(tape, workspace, point, values), other);
    workspace = Workspace(tape);
    workspace.lanes = 3;
    EXPECT_EQ(refused(tape, workspace, point, values), other);
    workspace = Workspace(tape, 3);
    workspace.slots.pop_back();
    EXPECT_EQ(refused(tape, workspace, point, values), other);
    workspace = Workspace(tape, 3);
    workspace.shared.pop_back();
    EXPECT_EQ(refused(tape, workspace, point, values), other);
    workspace.lanes = 0;
    EXPECT_EQ(refused(tape, workspace, point, values), "invalid argument: lanes: zero");
}

TEST(LaneTest, Sweeps)
{
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kRoots);
        const Tape tape(graph, roots, kBudgets[random.below(kBudgets.size())]);
        expect_sweeps(tape, random);
    }
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    Random random(9);
    expect_sweeps(Tape(graph, roots), random);
}

TEST(LaneTest, Special)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    const Tape tape(graph, roots);
    Random random(11);
    for (const std::size_t lanes : kLanes)
    {
        SCOPED_TRACE(lanes);
        Workspace workspace(tape, lanes);
        Adjoints adjoints(tape, lanes);
        const Points points = special_points(lanes);
        expect_values(tape, workspace, points);
        expect_jacobian(tape, workspace, adjoints, points);
        expect_weighted(tape, workspace, adjoints, points, random_weights(random, tape, lanes));
    }
}

TEST(LaneTest, ZeroWeight)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::array roots{unary(graph, Operator::Sqrt, x), unary(graph, Operator::Sin, y)};
    const Tape tape(graph, roots);
    Workspace workspace(tape, 3);
    Adjoints adjoints(tape, 3);
    const Points points{{0.0, 1.0}, {0.0, 2.0}, {4.0, 0.5}};
    const Points weights{{0.0, 1.0}, {1.0, 1.0}, {0.0, 0.0}};
    expect_weighted(tape, workspace, adjoints, points, weights);
}

TEST(LaneTest, Masked)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::array terms{x, y};
    const std::array roots{
        graph.add_sum(0.5, terms, std::array{2.0, -3.0}),
        listed(graph, Operator::Max, {x, y}),
    };
    const Tape tape(graph, roots);
    Workspace workspace(tape, 3);
    Adjoints adjoints(tape, 3);
    const Real nan = (std::numeric_limits<Real>::quiet_NaN)();
    const Points points{{nan, 2.0}, {nan, 2.0}, {1.0, 2.0}};
    const Points weights{{0.0, 0.0}, {1.0, 1.0}, {1.0, 1.0}};
    expect_weighted(tape, workspace, adjoints, points, weights);
    expect_bits(gradient_of(tape, points[0], weights[0]), std::vector{0.0, 0.0});
}

TEST(LaneTest, Mismatch)
{
    Graph graph;
    const Tape tape = shared_tape(graph);
    Workspace workspace(tape, 3);
    Adjoints adjoints(tape, 3);
    const std::vector<Real> point(6, 0.5);
    const std::vector<Real> weights(6, 1.0);
    std::vector<Real> values(6);
    std::vector<Real> out(6);
    const Sweep sweep{
        .tape = &tape,
        .point = point,
        .weights = weights,
        .workspace = &workspace,
        .adjoints = &adjoints,
        .values = values,
        .out = out,
    };
    run_jacobian(sweep);
    run_weighted(sweep);
    Sweep bad = sweep;
    bad.out = std::span(out).first(5);
    EXPECT_EQ(refused(run_jacobian, bad),
              "invalid argument: gradients: size 5, not the pattern size 2 in 3 lanes");
    EXPECT_EQ(refused(run_weighted, bad),
              "invalid argument: gradient: size 5, not the point size 6");
    bad = sweep;
    bad.weights = std::span(weights).first(2);
    EXPECT_EQ(refused(run_weighted, bad),
              "invalid argument: weights: size 2, not the root count 2 in 3 lanes");
    const std::string other = "sized for another tape";
    adjoints = Adjoints(tape);
    EXPECT_EQ(refused(run_jacobian, sweep),
              "invalid argument: adjoints: lane count 1, not the workspace's 3");
    adjoints = Adjoints(tape, 3);
    adjoints.taken.pop_back();
    EXPECT_EQ(refused(run_weighted, sweep), "invalid argument: adjoints: " + other);
    adjoints = Adjoints(tape, 3);
    adjoints.slots.pop_back();
    EXPECT_EQ(refused(run_jacobian, sweep), "invalid argument: adjoints: " + other);
    adjoints = Adjoints(tape, 3);
    adjoints.shared.pop_back();
    EXPECT_EQ(refused(run_weighted, sweep), "invalid argument: adjoints: " + other);
    adjoints = Adjoints(tape, 3);
    adjoints.gradient.pop_back();
    EXPECT_EQ(refused(run_jacobian, sweep), "invalid argument: adjoints: " + other);
    adjoints = Adjoints(tape, 3);
    workspace.partials.pop_back();
    EXPECT_EQ(refused(run_jacobian, sweep), "invalid argument: workspace: " + other);
    workspace = Workspace(tape, 3);
    workspace.spare.pop_back();
    EXPECT_EQ(refused(run_jacobian, sweep), "invalid argument: workspace: " + other);
}

TEST(LaneTest, Overflow)
{
    Graph graph;
    const Tape tape = wide_tape(graph);
    const std::array sizes{
        to_size(tape.roots()),
        to_size(tape.variables()),
        to_size(tape.pinned()),
        to_size(tape.partials()),
        std::size_t{tape.width()},
    };
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    const std::vector<Real> point(3, 0.5);
    std::vector<Real> values(3);
    std::vector<Real> out(3);
    for (const std::size_t size : sizes)
    {
        SCOPED_TRACE(size);
        ASSERT_GT(size, 1U);
        const std::size_t lanes = (std::numeric_limits<std::size_t>::max)() / size + 1;
        const std::string wanted = overflow(size);
        EXPECT_EQ(prefix(refusal([&] { Workspace{tape, lanes}; }), wanted), wanted);
        EXPECT_EQ(prefix(refusal([&] { Adjoints{tape, lanes}; }), wanted), wanted);
        workspace.lanes = lanes;
        EXPECT_EQ(prefix(refused(tape, workspace, point, values), wanted), wanted);
        const Sweep sweep{
            .tape = &tape,
            .point = point,
            .weights = point,
            .workspace = &workspace,
            .adjoints = &adjoints,
            .values = values,
            .out = out,
        };
        EXPECT_EQ(prefix(refused(run_jacobian, sweep), wanted), wanted);
        EXPECT_EQ(prefix(refused(run_weighted, sweep), wanted), wanted);
    }
    std::vector<Real> points(4);
    const std::size_t lanes = (std::numeric_limits<std::size_t>::max)() / 3 + 1;
    EXPECT_EQ(refusal([&] { broadcast(point, lanes, points); }),
              "invalid argument: lanes: " + std::to_string(lanes) + " times size 3 overflows");
}

TEST(LaneTest, Directions)
{
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kRoots);
        const Tape tape(graph, roots, kBudgets[random.below(kBudgets.size())]);
        for (const std::size_t lanes : kLanes)
        {
            SCOPED_TRACE(lanes);
            expect_seeded(tape, random, lanes);
        }
    }
}

TEST(LaneTest, Seeds)
{
    const std::array<Index, 2> variables{1, 0};
    std::vector<Real> directions(6);
    seed_directions(variables, 3, directions);
    expect_bits(directions, std::vector{0.0, 1.0, 0.0, 1.0, 0.0, 0.0});
    seed_directions({}, 3, directions);
    expect_bits(directions, std::vector(6, 0.0));
    EXPECT_EQ(seed_refusal(variables, 1, directions), "invalid argument: variables: 2 for 1 lanes");
    EXPECT_EQ(seed_refusal(variables, 4, directions),
              "invalid argument: directions: size 6 is not a multiple of 4 lanes");
    const std::array<Index, 2> outside{0, 2};
    EXPECT_EQ(seed_refusal(outside, 3, directions),
              "invalid argument: variables: variable 2 outside the directions");
    const std::array<Index, 1> negative{-1};
    EXPECT_EQ(seed_refusal(negative, 3, directions),
              "invalid argument: variables: variable -1 outside the directions");
    EXPECT_EQ(seed_refusal(variables, 0, directions), "invalid argument: lanes: zero");
    const std::array point{0.25, 0.5};
    std::vector<Real> points(6);
    broadcast(point, 3, points);
    expect_bits(points, std::vector{0.25, 0.25, 0.25, 0.5, 0.5, 0.5});
    EXPECT_EQ(refusal([&] { broadcast(point, 2, points); }),
              "invalid argument: points: size 6, not the point size 2 in 2 lanes");
    EXPECT_EQ(refusal([&] { broadcast(point, 0, points); }), "invalid argument: lanes: zero");
}
