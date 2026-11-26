#include "chords.hpp"
#include "core/numbers.hpp"
#include "expr/curvature/exponents.hpp"
#include "expr/curvature/shape.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/intervals/interval.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>

namespace
{

using biyos::Argument;
using biyos::Curvature;
using biyos::Interval;
using biyos::Monotonicity;
using biyos::Operator;
using biyos::power_bend;
using biyos::power_curvature;
using biyos::Profile;
using biyos::Real;
using biyos::sign_bend;
using biyos::test::expect_root;
using biyos::test::operator_graph;
using biyos::test::Parameters;
using biyos::test::text;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Interval kZero{0.0, 0.0};
constexpr Real kTiny = 0x1p-1072;
constexpr Real kNear = 0x1p-20;

constexpr Curvature kAffine = Curvature::Affine;
constexpr Curvature kConvex = Curvature::Convex;
constexpr Curvature kConcave = Curvature::Concave;
constexpr Curvature kNeither = Curvature::Unknown;
constexpr Monotonicity kRising = Monotonicity::Nondecreasing;
constexpr Monotonicity kFalling = Monotonicity::Nonincreasing;
constexpr Monotonicity kFlat = Monotonicity::Constant;
constexpr Monotonicity kUnsure = Monotonicity::Unknown;

struct Row
{
    Real exponent;
    Interval base;
    Curvature curvature;
    Monotonicity monotonicity;
};

constexpr std::array kPowers{
    Row{2.0, {-1.0, 2.0}, kConvex, kUnsure},
    Row{2.0, {0.0, 2.0}, kConvex, kRising},
    Row{2.0, {-2.0, 0.0}, kConvex, kFalling},
    Row{4.0, {-kInfinity, kInfinity}, kConvex, kUnsure},
    Row{3.0, {0.0, 2.0}, kConvex, kRising},
    Row{3.0, {-2.0, 0.0}, kConcave, kRising},
    Row{3.0, {-1.0, 1.0}, kNeither, kRising},
    Row{5.0, {-2.0, -1.0}, kConcave, kRising},
    Row{1.0, {-1.0, 2.0}, kAffine, kRising},
    Row{0.0, {-1.0, 2.0}, kAffine, kFlat},
    Row{0.5, {0.0, 4.0}, kConcave, kRising},
    Row{0.5, {-1.0, 4.0}, kConcave, kRising},
    Row{1.5, {0.0, 4.0}, kConvex, kRising},
    Row{1.5, {-1.0, 4.0}, kConvex, kRising},
    Row{-1.0, {1.0, 2.0}, kConvex, kFalling},
    Row{-1.0, {-2.0, -1.0}, kConcave, kFalling},
    Row{-1.0, {-1.0, 1.0}, kNeither, kUnsure},
    Row{-1.0, {0.0, 2.0}, kConvex, kFalling},
    Row{-1.0, {-2.0, 0.0}, kConcave, kFalling},
    Row{-2.0, {1.0, 2.0}, kConvex, kFalling},
    Row{-2.0, {-2.0, -1.0}, kConvex, kRising},
    Row{-2.0, {-2.0, 0.0}, kConvex, kRising},
    Row{-0.5, {1.0, 4.0}, kConvex, kFalling},
    Row{-0.5, {-1.0, 4.0}, kConvex, kFalling},
    Row{-1.5, {-2.0, -1.0}, kNeither, kUnsure},
    Row{0.95, {0.0, 4.0}, kConcave, kRising},
    Row{1.05, {0.0, 4.0}, kConvex, kRising},
    Row{3.0, {-kTiny, 0.0}, kConcave, kRising},
    Row{3.0, {0.0, kTiny}, kConvex, kRising},
    Row{3.0, {-kTiny, kTiny}, kNeither, kRising},
    Row{3.0, {-kNear, 0.0}, kConcave, kRising},
    Row{3.0, {0.0, kNear}, kConvex, kRising},
    Row{2.0, {-kTiny, 0.0}, kConvex, kFalling},
    Row{2.0, {0.0, kTiny}, kConvex, kRising},
    Row{2.0, {-kTiny, kTiny}, kConvex, kUnsure},
};

constexpr std::array kSignPowers{
    Row{1.5, {0.0, 2.0}, kConvex, kRising},       Row{1.5, {-2.0, 0.0}, kConcave, kRising},
    Row{1.5, {-1.0, 1.0}, kNeither, kRising},     Row{3.0, {-1.0, 0.0}, kConcave, kRising},
    Row{0.5, {0.0, 2.0}, kConcave, kRising},      Row{0.5, {-2.0, 0.0}, kConvex, kRising},
    Row{0.5, {-1.0, 1.0}, kNeither, kRising},     Row{1.0, {-1.0, 1.0}, kAffine, kRising},
    Row{2.0, {2.0, 2.0}, kAffine, kFlat},         Row{0.95, {0.0, 2.0}, kConcave, kRising},
    Row{0.95, {-2.0, 0.0}, kConvex, kRising},     Row{1.05, {0.0, 2.0}, kConvex, kRising},
    Row{1.05, {-2.0, 0.0}, kConcave, kRising},    Row{1.5, {-kTiny, 0.0}, kConcave, kRising},
    Row{1.5, {0.0, kTiny}, kConvex, kRising},     Row{1.5, {-kTiny, kTiny}, kNeither, kRising},
    Row{1.5, {-kNear, 0.0}, kConcave, kRising},   Row{1.5, {0.0, kNear}, kConvex, kRising},
    Row{0.5, {-kTiny, 0.0}, kConvex, kRising},    Row{0.5, {0.0, kTiny}, kConcave, kRising},
    Row{0.5, {-kNear, kNear}, kNeither, kRising},
};

struct Pair
{
    Interval base;
    Interval exponent;
    Curvature curvature;
    Monotonicity base_monotonicity;
    Monotonicity exponent_monotonicity;
};

constexpr std::array kPairs{
    Pair{{1.0, 2.0}, {2.0, 2.0}, kConvex, kRising, kFlat},
    Pair{{-1.0, 2.0}, {3.0, 3.0}, kNeither, kRising, kFlat},
    Pair{{2.0, 2.0}, {-1.0, 1.0}, kConvex, kUnsure, kRising},
    Pair{{0.5, 0.5}, {-1.0, 1.0}, kConvex, kUnsure, kFalling},
    Pair{{1.0, 1.0}, {-1.0, 1.0}, kAffine, kUnsure, kFlat},
    Pair{{1.0, 2.0}, {1.0, 2.0}, kNeither, kRising, kRising},
    Pair{{0.2, 0.5}, {-2.0, -1.0}, kNeither, kFalling, kFalling},
    Pair{{0.5, 2.0}, {0.0, 1.0}, kNeither, kRising, kUnsure},
    Pair{{0.0, 2.0}, {1.0, 2.0}, kNeither, kUnsure, kUnsure},
    Pair{{-2.0, -2.0}, {1.0, 2.0}, kNeither, kUnsure, kUnsure},
};

std::string row_text(const Row &row)
{
    return "exponent " + text(row.exponent) + " over [" + text(row.base.lower()) + ", " +
           text(row.base.upper()) + "]";
}

Parameters with(Real exponent)
{
    return {.constant = 1.0, .weights = {1.0, 1.0, 1.0}, .exponent = exponent};
}

void expect_row(Operator op, const Row &row, const Profile &profile)
{
    SCOPED_TRACE(row_text(row));
    EXPECT_EQ(profile.curvature, row.curvature);
    EXPECT_EQ(profile.argument.monotonicity, row.monotonicity);
    const std::array box{row.base, kZero, kZero};
    const std::array monotonicities{profile.argument.monotonicity};
    const auto graph = operator_graph(op, with(row.exponent));
    expect_root(graph, box, profile.curvature, monotonicities);
}

}

TEST(CurvatureTest, Powers)
{
    for (const Row &row : kPowers)
    {
        expect_row(Operator::ConstantPower, row, power_bend(row.base, row.exponent));
    }
}

TEST(CurvatureTest, SignPowers)
{
    for (const Row &row : kSignPowers)
    {
        expect_row(Operator::SignPower, row, sign_bend(row.base, row.exponent));
    }
}

TEST(CurvatureTest, PowerPairs)
{
    for (const Pair &pair : kPairs)
    {
        SCOPED_TRACE("base [" + text(pair.base.lower()) + ", " + text(pair.base.upper()) +
                     "], exponent [" + text(pair.exponent.lower()) + ", " +
                     text(pair.exponent.upper()) + "]");
        std::array<Argument, 2> arguments{};
        const Curvature found = power_curvature(pair.base, pair.exponent, arguments);
        EXPECT_EQ(found, pair.curvature);
        EXPECT_EQ(arguments[0].monotonicity, pair.base_monotonicity);
        EXPECT_EQ(arguments[1].monotonicity, pair.exponent_monotonicity);
        const std::array box{pair.base, pair.exponent, kZero};
        const std::array monotonicities{arguments[0].monotonicity, arguments[1].monotonicity};
        const auto graph = operator_graph(Operator::Power, with(1.0));
        expect_root(graph, box, found, monotonicities);
    }
}

TEST(CurvatureTest, PowerFlags)
{
    EXPECT_TRUE(power_bend({-1.0, 4.0}, 0.5).argument.cut_below);
    EXPECT_FALSE(power_bend({0.0, 4.0}, 0.5).argument.cut_below);
    EXPECT_TRUE(power_bend({0.0, 4.0}, -0.5).argument.cut_below);
    EXPECT_TRUE(power_bend({0.0, 4.0}, -1.0).argument.cut_below);
    EXPECT_TRUE(power_bend({-4.0, 0.0}, -1.0).argument.cut_above);
    EXPECT_FALSE(power_bend({-4.0, -1.0}, -1.0).argument.cut_above);
    EXPECT_FALSE(power_bend({-4.0, 4.0}, 2.0).argument.cut_below);
    EXPECT_FALSE(sign_bend({-4.0, 4.0}, 0.5).argument.cut_below);
    EXPECT_EQ(power_bend({0.0, 0.0}, -1.0).curvature, kNeither);
    EXPECT_EQ(power_bend({-2.0, -1.0}, 0.5).curvature, kNeither);
}
