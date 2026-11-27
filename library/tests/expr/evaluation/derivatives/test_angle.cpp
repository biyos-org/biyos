#include "core/env/env.hpp"
#include "core/env/pool.hpp"
#include "core/numbers.hpp"
#include "core/timing/timer.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/hessian.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace
{

using biyos::Env;
using biyos::Graph;
using biyos::Hessian;
using biyos::Index;
using biyos::Offset;
using biyos::Operator;
using biyos::Pool;
using biyos::position;
using biyos::Real;
using biyos::Tape;
using biyos::TimingTree;
using biyos::to_size;
using biyos::test::binary;
using biyos::test::bits;
using biyos::test::default_tolerances;
using biyos::test::gradient_of;
using biyos::test::jacobian_of;
using biyos::test::values_of;

using Point = std::array<Real, 2>;

constexpr Real kPi = std::numbers::pi;
constexpr Real kStep = 1e-6;
constexpr Real kBound = 1e-7;
constexpr std::array kWeight{1.0};
constexpr std::array<Point, 6> kPoints{{
    {0.7, -1.3},
    {-2.0, 0.5},
    {3.0, 4.0},
    {-0.2, -0.9},
    {1.0, 0.0},
    {-5.0, 1e-3},
}};

struct Angle
{
    Graph graph;
    Offset root;

    Angle()
    {
        const Offset y = graph.add_variable(0);
        const Offset x = graph.add_variable(1);
        root = binary(graph, Operator::Atan2, y, x);
    }

    Tape tape() const { return Tape(graph, std::span(&root, 1)); }
};

struct Case
{
    Point point;
    Real angle;
};

Real value_at(const Point &point)
{
    const Angle made;
    return values_of(made.tape(), point).front();
}

std::vector<Real> slopes_at(const Point &point)
{
    const Angle made;
    return jacobian_of(made.tape(), point);
}

std::vector<Real> hessian_at(const Point &point)
{
    Env env;
    TimingTree tree;
    const Angle made;
    Hessian hessian(made.graph, std::span(&made.root, 1), {}, {env.stream(), tree});
    std::vector<Real> out(hessian.pattern().rows.size(), -1.0);
    Pool pool;
    hessian.values(point, kWeight, default_tolerances(), out, pool, {env.stream(), tree});
    std::vector<Real> dense;
    for (Index i = 0; i < 2; ++i)
    {
        for (Index j = 0; j < 2; ++j)
        {
            const Index row = (std::max)(i, j);
            const Index column = (std::min)(i, j);
            dense.push_back(out[to_size(position(hessian.pattern(), row, column))]);
        }
    }
    return dense;
}

std::vector<Real> differences_at(Point point)
{
    const Angle made;
    const Tape tape = made.tape();
    std::vector<Real> dense;
    for (std::size_t j = 0; j < 2; ++j)
    {
        const Real middle = point[j];
        point[j] = middle + kStep;
        const std::vector<Real> ahead = gradient_of(tape, point, kWeight);
        point[j] = middle - kStep;
        const std::vector<Real> behind = gradient_of(tape, point, kWeight);
        point[j] = middle;
        for (std::size_t i = 0; i < 2; ++i)
        {
            dense.push_back((ahead[i] - behind[i]) / (2.0 * kStep));
        }
    }
    return dense;
}

Real slope_difference(Point point, std::size_t j)
{
    const Real middle = point[j];
    point[j] = middle + kStep;
    const Real ahead = value_at(point);
    point[j] = middle - kStep;
    const Real behind = value_at(point);
    return (ahead - behind) / (2.0 * kStep);
}

}

TEST(AngleTest, Axes)
{
    const std::array cases{
        Case{{0.0, 0.0}, 0.0},
        Case{{-0.0, 0.0}, 0.0},
        Case{{0.0, -0.0}, 0.0},
        Case{{-0.0, -0.0}, 0.0},
        Case{{0.0, 1.0}, 0.0},
        Case{{-0.0, 1.0}, 0.0},
        Case{{0.0, -1.0}, kPi},
        Case{{-0.0, -1.0}, kPi},
        Case{{1.0, 0.0}, kPi / 2.0},
        Case{{1.0, -0.0}, kPi / 2.0},
        Case{{-1.0, 0.0}, -kPi / 2.0},
        Case{{-1.0, -0.0}, -kPi / 2.0},
    };
    for (const Case &probe : cases)
    {
        SCOPED_TRACE(testing::Message() << probe.point[0] << ", " << probe.point[1]);
        EXPECT_EQ(bits(value_at(probe.point)), bits(probe.angle));
    }
}

TEST(AngleTest, Origin)
{
    for (const Point &point :
         {Point{0.0, 0.0}, Point{-0.0, 0.0}, Point{0.0, -0.0}, Point{-0.0, -0.0}})
    {
        SCOPED_TRACE(testing::Message() << point[0] << ", " << point[1]);
        for (const Real slope : slopes_at(point))
        {
            EXPECT_TRUE(std::isnan(slope)) << slope;
        }
    }
}

TEST(AngleTest, Extremes)
{
    for (const Real size : {1e200, 1e-170})
    {
        SCOPED_TRACE(size);
        const std::vector<Real> slopes = slopes_at({size, size});
        EXPECT_DOUBLE_EQ(slopes[0], 0.5 / size);
        EXPECT_DOUBLE_EQ(slopes[1], -0.5 / size);
    }
}

TEST(AngleTest, Differences)
{
    for (const Point &point : kPoints)
    {
        SCOPED_TRACE(testing::Message() << point[0] << ", " << point[1]);
        const std::vector<Real> slopes = slopes_at(point);
        for (std::size_t j = 0; j < 2; ++j)
        {
            const Real wanted = slope_difference(point, j);
            EXPECT_NEAR(slopes[j], wanted, kBound * (1.0 + std::fabs(wanted)));
        }
    }
}

TEST(AngleTest, Curvature)
{
    for (const Point &point : kPoints)
    {
        SCOPED_TRACE(testing::Message() << point[0] << ", " << point[1]);
        const std::vector<Real> found = hessian_at(point);
        const std::vector<Real> wanted = differences_at(point);
        for (std::size_t k = 0; k < wanted.size(); ++k)
        {
            SCOPED_TRACE(k);
            EXPECT_NEAR(found[k], wanted[k], kBound * (1.0 + std::fabs(wanted[k])));
        }
    }
}
