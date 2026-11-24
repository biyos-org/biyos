#include "chords.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/interval.hpp"
#include "expr/rounding.hpp"
#include "expr/shape.hpp"
#include "expr/waves.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <string>

namespace
{

using biyos::Curvature;
using biyos::Interval;
using biyos::Monotonicity;
using biyos::Operator;
using biyos::Profile;
using biyos::Wave;
using biyos::test::Parameters;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kPi = std::numbers::pi;
constexpr double kPiUp = 0x1.921fb54442d19p+1;
constexpr double kHalfPi = kPi / 2;
constexpr double kHalfPiUp = 0x1.921fb54442d19p+0;
constexpr double kFar = 0x1p52;
constexpr double kPastPole = 0x1.ab41b09886feap+4;
constexpr Interval kZero{0.0, 0.0};
constexpr int kLattice = 40;
constexpr int kSteps = 4;
constexpr std::array kReaches{0x1p-20, 1.0};
constexpr Parameters kFixed{.constant = 1.0, .weights = {1.0, 1.0, 1.0}, .exponent = 2.0};

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
    Operator op;
    const Wave *shape;
    Interval child;
    Curvature curvature;
    Monotonicity monotonicity;
};

const std::array kRows{
    Row{Operator::Sin, &biyos::kSine, {0.0, 3.0}, kConcave, kUnsure},
    Row{Operator::Sin, &biyos::kSine, {0.0, 1.5}, kConcave, kRising},
    Row{Operator::Sin, &biyos::kSine, {0.0, kPi}, kConcave, kUnsure},
    Row{Operator::Sin, &biyos::kSine, {0.0, kPiUp}, kNeither, kUnsure},
    Row{Operator::Sin, &biyos::kSine, {kPiUp, 6.0}, kConvex, kUnsure},
    Row{Operator::Sin, &biyos::kSine, {kPi, 4.0}, kNeither, kFalling},
    Row{Operator::Sin, &biyos::kSine, {-3.0, -0.2}, kConvex, kUnsure},
    Row{Operator::Sin, &biyos::kSine, {2.0, 4.0}, kNeither, kFalling},
    Row{Operator::Sin, &biyos::kSine, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Sin, &biyos::kSine, {-kHalfPi, kHalfPi}, kNeither, kRising},
    Row{Operator::Sin, &biyos::kSine, {0.0, 7.0}, kNeither, kUnsure},
    Row{Operator::Sin, &biyos::kSine, {-kInfinity, 0.0}, kNeither, kUnsure},
    Row{Operator::Sin, &biyos::kSine, {1.0, 1.0}, kAffine, kFlat},
    Row{Operator::Sin, &biyos::kSine, {1000.0, 1001.0}, kConcave, kUnsure},
    Row{Operator::Sin, &biyos::kSine, {kFar, kFar + 1.0}, kNeither, kUnsure},
    Row{Operator::Cos, &biyos::kCosine, {-1.5, 1.5}, kConcave, kUnsure},
    Row{Operator::Cos, &biyos::kCosine, {0.0, 1.5}, kConcave, kFalling},
    Row{Operator::Cos, &biyos::kCosine, {2.0, 4.0}, kConvex, kUnsure},
    Row{Operator::Cos, &biyos::kCosine, {1.0, 2.0}, kNeither, kFalling},
    Row{Operator::Cos, &biyos::kCosine, {-3.0, -2.0}, kConvex, kRising},
    Row{Operator::Cos, &biyos::kCosine, {4.8, 6.0}, kConcave, kRising},
    Row{Operator::Tan, &biyos::kTangent, {0.0, 1.5}, kConvex, kRising},
    Row{Operator::Tan, &biyos::kTangent, {0.0, kHalfPi}, kConvex, kRising},
    Row{Operator::Tan, &biyos::kTangent, {0.0, kHalfPiUp}, kNeither, kUnsure},
    Row{Operator::Tan, &biyos::kTangent, {kHalfPi, 2.0}, kNeither, kUnsure},
    Row{Operator::Tan, &biyos::kTangent, {25.2, kPastPole}, kNeither, kUnsure},
    Row{Operator::Tan, &biyos::kTangent, {-1.5, 0.0}, kConcave, kRising},
    Row{Operator::Tan, &biyos::kTangent, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Tan, &biyos::kTangent, {1.0, 2.0}, kNeither, kUnsure},
    Row{Operator::Tan, &biyos::kTangent, {2.0, 3.0}, kConcave, kRising},
    Row{Operator::Tan, &biyos::kTangent, {3.2, 4.0}, kConvex, kRising},
    Row{Operator::Tan, &biyos::kTangent, {2.0, 4.0}, kNeither, kRising},
    Row{Operator::Tan, &biyos::kTangent, {-4.0, -3.2}, kConcave, kRising},
    Row{Operator::Tan, &biyos::kTangent, {0.0, 7.0}, kNeither, kUnsure},
};

struct Quarters
{
    Operator op;
    const Wave *shape;
    std::array<Curvature, 4> curvatures;
    std::array<Monotonicity, 4> monotonicities;
};

const std::array kQuarters{
    Quarters{Operator::Sin,
             &biyos::kSine,
             {kConcave, kConcave, kConvex, kConvex},
             {kRising, kFalling, kFalling, kRising}},
    Quarters{Operator::Cos,
             &biyos::kCosine,
             {kConcave, kConvex, kConvex, kConcave},
             {kFalling, kFalling, kRising, kRising}},
    Quarters{Operator::Tan,
             &biyos::kTangent,
             {kConvex, kConcave, kConvex, kConcave},
             {kRising, kRising, kRising, kRising}},
};

std::size_t phase(int quarter)
{
    return static_cast<std::size_t>(((quarter % 4) + 4) % 4);
}

bool within(Curvature claim, Curvature truth)
{
    const bool convex = !biyos::is_convex(claim) || biyos::is_convex(truth);
    return convex && (!biyos::is_concave(claim) || biyos::is_concave(truth));
}

bool within(Monotonicity claim, Monotonicity truth)
{
    const bool rising = !biyos::rises(claim) || biyos::rises(truth);
    return rising && (!biyos::falls(claim) || biyos::falls(truth));
}

void expect_quarter(const Quarters &quarters, Interval child, int quarter, bool exact)
{
    SCOPED_TRACE("operator " + std::to_string(static_cast<int>(quarters.op)) + " over [" +
                 biyos::test::text(child.lower()) + ", " + biyos::test::text(child.upper()) +
                 "] in quarter " + std::to_string(quarter));
    const Profile profile = biyos::wave(*quarters.shape, child);
    const Curvature curvature = quarters.curvatures[phase(quarter)];
    const Monotonicity monotonicity = quarters.monotonicities[phase(quarter)];
    EXPECT_TRUE(within(profile.curvature, curvature));
    EXPECT_TRUE(within(profile.argument.monotonicity, monotonicity));
    if (exact)
    {
        EXPECT_EQ(profile.curvature, curvature);
        EXPECT_EQ(profile.argument.monotonicity, monotonicity);
    }
    const std::array box{child, kZero, kZero};
    const std::array monotonicities{profile.argument.monotonicity};
    biyos::test::expect_root(biyos::test::operator_graph(quarters.op, kFixed),
                             box,
                             profile.curvature,
                             monotonicities);
}

void expect_sides(const Quarters &quarters, int quarter, int gap)
{
    const double near = quarter * kHalfPi;
    const double below = biyos::shifted(near, -gap);
    const double above = biyos::shifted(near, gap);
    const bool exact = gap > 1;
    expect_quarter(quarters, {biyos::shifted(below, -kSteps), below}, quarter - 1, exact);
    expect_quarter(quarters, {above, biyos::shifted(above, kSteps)}, quarter, exact);
    for (const double reach : kReaches)
    {
        expect_quarter(quarters, {below - reach, below}, quarter - 1, exact);
        expect_quarter(quarters, {above, above + reach}, quarter, exact);
    }
}

std::string row_text(const Row &row)
{
    return "operator " + std::to_string(static_cast<int>(row.op)) + " over [" +
           biyos::test::text(row.child.lower()) + ", " + biyos::test::text(row.child.upper()) + "]";
}

}

TEST(CurvatureTest, Waves)
{
    for (const Row &row : kRows)
    {
        SCOPED_TRACE(row_text(row));
        const Profile profile = biyos::wave(*row.shape, row.child);
        EXPECT_EQ(profile.curvature, row.curvature);
        EXPECT_EQ(profile.argument.monotonicity, row.monotonicity);
        EXPECT_FALSE(profile.argument.cut_below || profile.argument.cut_above);
        const std::array box{row.child, kZero, kZero};
        const std::array monotonicities{profile.argument.monotonicity};
        biyos::test::expect_root(biyos::test::operator_graph(row.op, kFixed),
                                 box,
                                 profile.curvature,
                                 monotonicities);
    }
}

TEST(CurvatureTest, Straddles)
{
    for (int quarter = -40; quarter <= 40; ++quarter)
    {
        const double near = quarter * kHalfPi;
        const double below = std::nextafter(std::nextafter(near, -kInfinity), -kInfinity);
        const double above = std::nextafter(std::nextafter(near, kInfinity), kInfinity);
        const Interval across(below, above);
        const bool odd = quarter % 2 != 0;
        SCOPED_TRACE("quarter " + std::to_string(quarter));
        const Profile sine = biyos::wave(biyos::kSine, across);
        const Profile cosine = biyos::wave(biyos::kCosine, across);
        const Profile tangent = biyos::wave(biyos::kTangent, across);
        EXPECT_EQ(odd ? sine.argument.monotonicity : cosine.argument.monotonicity, kUnsure);
        EXPECT_EQ(odd ? cosine.curvature : sine.curvature, kNeither);
        EXPECT_EQ(tangent.curvature, kNeither);
        EXPECT_EQ(tangent.argument.monotonicity, odd ? kUnsure : kRising);
    }
}

TEST(CurvatureTest, Lattice)
{
    for (int quarter = -kLattice; quarter <= kLattice; ++quarter)
    {
        for (const Quarters &quarters : kQuarters)
        {
            expect_sides(quarters, quarter, 1);
            expect_sides(quarters, quarter, 2);
        }
    }
}
