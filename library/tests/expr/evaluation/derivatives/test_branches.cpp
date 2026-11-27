#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/evaluation/derivatives/derivative.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "lanes.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace
{

using biyos::Adjoints;
using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::Tape;
using biyos::Tolerances;
using biyos::Workspace;
using biyos::test::binary;
using biyos::test::bits;
using biyos::test::branch;
using biyos::test::constant;
using biyos::test::default_tolerances;
using biyos::test::interleave;
using biyos::test::kLanes;
using biyos::test::kSide;
using biyos::test::lane;
using biyos::test::Points;
using biyos::test::Random;
using biyos::test::unary;
using biyos::test::values_of;
using biyos::test::variables;

using Values = std::vector<Real>;

constexpr Real kNan = (std::numeric_limits<Real>::quiet_NaN)();
constexpr Real kStep = 1e-6;
constexpr Real kBound = 1e-6;
constexpr Real kApart = 1e-3;

struct Lane
{
    Real value;
    Values row;
    Values gradient;
};

std::vector<Lane> swept(const Tape &tape, const Points &points)
{
    const std::size_t lanes = points.size();
    Workspace workspace(tape, lanes);
    Adjoints adjoints(tape, lanes);
    const Tolerances tolerances = default_tolerances();
    const Values point = interleave(points);
    Values values(lanes);
    Values rows(tape.pattern().variables.size() * lanes);
    biyos::jacobian(tape, point, tolerances, workspace, adjoints, values, rows);
    const Values weights(lanes, 1.0);
    Values again(lanes);
    Values gradient(point.size());
    biyos::weighted_gradient(tape,
                             point,
                             weights,
                             tolerances,
                             workspace,
                             adjoints,
                             again,
                             gradient);
    std::vector<Lane> found;
    for (std::size_t l = 0; l < lanes; ++l)
    {
        EXPECT_EQ(bits(again[l]), bits(values[l]));
        found.push_back({values[l], lane(rows, l, lanes), lane(gradient, l, lanes)});
    }
    return found;
}

void expect_near(const Values &found, const Values &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        SCOPED_TRACE(k);
        EXPECT_NEAR(found[k], wanted[k], kBound * (1.0 + std::fabs(wanted[k])));
    }
}

void expect_lane(const Lane &found, Real value, const Values &gradient)
{
    EXPECT_NEAR(found.value, value, kBound * (1.0 + std::fabs(value)));
    expect_near(found.row, gradient);
    expect_near(found.gradient, gradient);
}

Real value_at(const Tape &tape, const Values &point)
{
    return values_of(tape, point)[0];
}

Real moved(const Tape &tape, Values point, std::size_t j, Real step)
{
    point[j] += step;
    return value_at(tape, point);
}

Values central(const Tape &tape, const Values &point)
{
    Values slopes;
    for (std::size_t j = 0; j < point.size(); ++j)
    {
        const Real step = kStep * (1.0 + std::fabs(point[j]));
        slopes.push_back((moved(tape, point, j, step) - moved(tape, point, j, -step)) /
                         (2.0 * step));
    }
    return slopes;
}

Offset product(Graph &graph, std::span<const Offset> factors)
{
    return graph.add(Operator::Product, 1.0, factors);
}

Points alternating(std::size_t lanes, const Values &even, const Values &odd)
{
    Points points;
    for (std::size_t l = 0; l < lanes; ++l)
    {
        points.push_back(l % 2 == 0 ? even : odd);
    }
    return points;
}

Values apart(Random &random)
{
    Values point{random.uniform(-1.0, 1.0), random.uniform(-1.0, 1.0), random.uniform(-1.0, 1.0)};
    while (std::fabs(point[0] - point[1]) < kApart)
    {
        point[1] = random.uniform(-1.0, 1.0);
    }
    return point;
}

}

TEST(BranchTest, Domain)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset zero = constant(graph, 0.0);
    const Offset positive = binary(graph, Operator::GreaterEqual, x, zero);
    const Offset root = branch(graph, positive, unary(graph, Operator::Sqrt, x), zero);
    const Tape tape(graph, std::span(&root, 1));
    for (const std::size_t lanes : kLanes)
    {
        SCOPED_TRACE(lanes);
        const std::vector<Lane> found = swept(tape, alternating(lanes, {-1.0}, {4.0}));
        for (std::size_t l = 0; l < lanes; ++l)
        {
            SCOPED_TRACE(l);
            if (l % 2 == 0)
            {
                EXPECT_EQ(bits(found[l].value), bits(0.0));
                EXPECT_EQ(bits(found[l].row), bits(Values{0.0}));
                EXPECT_EQ(bits(found[l].gradient), bits(Values{0.0}));
                continue;
            }
            expect_lane(found[l], 2.0, {0.25});
        }
    }
}

TEST(BranchTest, Undefined)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset negative = binary(graph, Operator::Less, x[0], constant(graph, 0.0));
    const Offset root = branch(graph, negative, x[1], product(graph, std::array{x[1], x[1]}));
    const Tape tape(graph, std::span(&root, 1));
    for (const std::size_t lanes : kLanes)
    {
        SCOPED_TRACE(lanes);
        const std::vector<Lane> found = swept(tape, alternating(lanes, {kNan, 3.0}, {1.0, 3.0}));
        for (std::size_t l = 0; l < lanes; ++l)
        {
            SCOPED_TRACE(l);
            if (l % 2 == 0)
            {
                EXPECT_TRUE(std::isnan(found[l].value));
                EXPECT_TRUE(std::isnan(found[l].row[0]) && std::isnan(found[l].row[1]));
                EXPECT_TRUE(std::isnan(found[l].gradient[0]) && std::isnan(found[l].gradient[1]));
                continue;
            }
            expect_lane(found[l], 9.0, {0.0, 6.0});
        }
    }
}

TEST(BranchTest, Differences)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset grown = unary(graph, Operator::Exp, x[2]);
    const std::array terms{product(graph, x), grown};
    const Offset then = graph.add_sum(0.0, terms, std::array{1.0, 1.0});
    const std::array factors{unary(graph, Operator::Sin, x[0]), x[2], x[2]};
    const Offset below = binary(graph, Operator::Less, x[0], x[1]);
    const Offset root = branch(graph, below, then, product(graph, factors));
    const Tape tape(graph, std::span(&root, 1));
    Random random(7);
    for (const std::size_t lanes : kLanes)
    {
        SCOPED_TRACE(lanes);
        Points points;
        for (std::size_t l = 0; l < lanes; ++l)
        {
            points.push_back(apart(random));
        }
        const std::vector<Lane> found = swept(tape, points);
        for (std::size_t l = 0; l < lanes; ++l)
        {
            SCOPED_TRACE(l);
            expect_lane(found[l], value_at(tape, points[l]), central(tape, points[l]));
        }
    }
}

TEST(BranchTest, Switch)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset one = constant(graph, 1.0);
    const Offset curved = product(graph, std::array{x[0], x[0], x[1]});
    const Offset line = graph.add_sum(0.0, x, std::array{3.0, 1.0});
    struct Case
    {
        Offset root;
        Real side;
    };
    const std::array cases{
        Case{branch(graph, binary(graph, Operator::GreaterEqual, x[0], one), curved, line), 1.0},
        Case{branch(graph, binary(graph, Operator::Less, x[0], one), line, curved), 1.0},
        Case{branch(graph, binary(graph, Operator::LessEqual, x[0], one), line, curved), -1.0},
        Case{branch(graph, binary(graph, Operator::Greater, x[0], one), curved, line), -1.0},
    };
    for (std::size_t c = 0; c < cases.size(); ++c)
    {
        SCOPED_TRACE(c);
        const Tape tape(graph, std::span(&cases[c].root, 1));
        for (const std::size_t lanes : kLanes)
        {
            SCOPED_TRACE(lanes);
            Points points;
            for (std::size_t l = 0; l < lanes; ++l)
            {
                points.push_back({1.0, 2.0 + 0.25 * static_cast<Real>(l)});
            }
            const std::vector<Lane> found = swept(tape, points);
            for (std::size_t l = 0; l < lanes; ++l)
            {
                SCOPED_TRACE(l);
                const Real here = value_at(tape, points[l]);
                const Real step = cases[c].side * kSide;
                const Real slope = (moved(tape, points[l], 0, step) - here) / step;
                expect_lane(found[l], here, {slope, central(tape, points[l])[1]});
            }
        }
    }
}

TEST(BranchTest, Shared)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset zero = constant(graph, 0.0);
    const Offset grown = unary(graph, Operator::Exp, x[0]);
    const Offset square = product(graph, std::array{x[1], x[1]});
    const Offset positive = binary(graph, Operator::GreaterEqual, x[0], zero);
    const std::array terms{branch(graph, positive, square, grown), grown};
    const Offset total = graph.add_sum(0.0, terms, std::array{1.0, 2.0});
    const Offset large = binary(graph, Operator::GreaterEqual, grown, constant(graph, 1.0));
    const Offset scaled = branch(graph, large, product(graph, std::array{grown, x[1]}), x[1]);
    const Real high = std::exp(0.5);
    const Real low = std::exp(-0.5);
    for (const std::size_t lanes : kLanes)
    {
        SCOPED_TRACE(lanes);
        const Points points = alternating(lanes, {0.5, 0.3}, {-0.5, 0.3});
        const std::vector<Lane> sums = swept(Tape(graph, std::span(&total, 1)), points);
        const std::vector<Lane> products = swept(Tape(graph, std::span(&scaled, 1)), points);
        for (std::size_t l = 0; l < lanes; ++l)
        {
            SCOPED_TRACE(l);
            if (l % 2 == 0)
            {
                expect_lane(sums[l], 0.09 + 2.0 * high, {2.0 * high, 0.6});
                expect_lane(products[l], 0.3 * high, {0.3 * high, high});
                continue;
            }
            expect_lane(sums[l], 3.0 * low, {3.0 * low, 0.0});
            expect_lane(products[l], 0.3, {0.0, 1.0});
        }
    }
}
