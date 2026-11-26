#include "chords.hpp"
#include "core/numbers.hpp"
#include "expr/curvature/shape.hpp"
#include "expr/curvature/waves.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/intervals/interval.hpp"
#include "expr/intervals/rounding.hpp"
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
using biyos::falls;
using biyos::Interval;
using biyos::is_concave;
using biyos::is_convex;
using biyos::kCosine;
using biyos::kSine;
using biyos::kTangent;
using biyos::Monotonicity;
using biyos::Operator;
using biyos::Profile;
using biyos::Real;
using biyos::rises;
using biyos::shifted;
using biyos::wave;
using biyos::Wave;
using biyos::test::expect_root;
using biyos::test::operator_graph;
using biyos::test::Parameters;
using biyos::test::text;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kPi = std::numbers::pi;
constexpr Real kPiUp = 0x1.921fb54442d19p+1;
constexpr Real kHalfPi = kPi / 2;
constexpr Real kHalfPiUp = 0x1.921fb54442d19p+0;
constexpr Real kFar = 0x1p52;
constexpr Real kPastPole = 0x1.ab41b09886feap+4;
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
    Row{Operator::Sin, &kSine, {0.0, 3.0}, kConcave, kUnsure},
    Row{Operator::Sin, &kSine, {0.0, 1.5}, kConcave, kRising},
    Row{Operator::Sin, &kSine, {0.0, kPi}, kConcave, kUnsure},
    Row{Operator::Sin, &kSine, {0.0, kPiUp}, kNeither, kUnsure},
    Row{Operator::Sin, &kSine, {kPiUp, 6.0}, kConvex, kUnsure},
    Row{Operator::Sin, &kSine, {kPi, 4.0}, kNeither, kFalling},
    Row{Operator::Sin, &kSine, {-3.0, -0.2}, kConvex, kUnsure},
    Row{Operator::Sin, &kSine, {2.0, 4.0}, kNeither, kFalling},
    Row{Operator::Sin, &kSine, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Sin, &kSine, {-kHalfPi, kHalfPi}, kNeither, kRising},
    Row{Operator::Sin, &kSine, {0.0, 7.0}, kNeither, kUnsure},
    Row{Operator::Sin, &kSine, {-kInfinity, 0.0}, kNeither, kUnsure},
    Row{Operator::Sin, &kSine, {1.0, 1.0}, kAffine, kFlat},
    Row{Operator::Sin, &kSine, {1000.0, 1001.0}, kConcave, kUnsure},
    Row{Operator::Sin, &kSine, {kFar, kFar + 1.0}, kNeither, kUnsure},
    Row{Operator::Cos, &kCosine, {-1.5, 1.5}, kConcave, kUnsure},
    Row{Operator::Cos, &kCosine, {0.0, 1.5}, kConcave, kFalling},
    Row{Operator::Cos, &kCosine, {2.0, 4.0}, kConvex, kUnsure},
    Row{Operator::Cos, &kCosine, {1.0, 2.0}, kNeither, kFalling},
    Row{Operator::Cos, &kCosine, {-3.0, -2.0}, kConvex, kRising},
    Row{Operator::Cos, &kCosine, {4.8, 6.0}, kConcave, kRising},
    Row{Operator::Tan, &kTangent, {0.0, 1.5}, kConvex, kRising},
    Row{Operator::Tan, &kTangent, {0.0, kHalfPi}, kConvex, kRising},
    Row{Operator::Tan, &kTangent, {0.0, kHalfPiUp}, kNeither, kUnsure},
    Row{Operator::Tan, &kTangent, {kHalfPi, 2.0}, kNeither, kUnsure},
    Row{Operator::Tan, &kTangent, {25.2, kPastPole}, kNeither, kUnsure},
    Row{Operator::Tan, &kTangent, {-1.5, 0.0}, kConcave, kRising},
    Row{Operator::Tan, &kTangent, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Tan, &kTangent, {1.0, 2.0}, kNeither, kUnsure},
    Row{Operator::Tan, &kTangent, {2.0, 3.0}, kConcave, kRising},
    Row{Operator::Tan, &kTangent, {3.2, 4.0}, kConvex, kRising},
    Row{Operator::Tan, &kTangent, {2.0, 4.0}, kNeither, kRising},
    Row{Operator::Tan, &kTangent, {-4.0, -3.2}, kConcave, kRising},
    Row{Operator::Tan, &kTangent, {0.0, 7.0}, kNeither, kUnsure},
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
             &kSine,
             {kConcave, kConcave, kConvex, kConvex},
             {kRising, kFalling, kFalling, kRising}},
    Quarters{Operator::Cos,
             &kCosine,
             {kConcave, kConvex, kConvex, kConcave},
             {kFalling, kFalling, kRising, kRising}},
    Quarters{Operator::Tan,
             &kTangent,
             {kConvex, kConcave, kConvex, kConcave},
             {kRising, kRising, kRising, kRising}},
};

std::size_t phase(int quarter)
{
    return static_cast<std::size_t>(((quarter % 4) + 4) % 4);
}

bool within(Curvature claim, Curvature truth)
{
    const bool convex = !is_convex(claim) || is_convex(truth);
    return convex && (!is_concave(claim) || is_concave(truth));
}

bool within(Monotonicity claim, Monotonicity truth)
{
    const bool rising = !rises(claim) || rises(truth);
    return rising && (!falls(claim) || falls(truth));
}

void expect_quarter(const Quarters &quarters, Interval child, int quarter, bool exact)
{
    SCOPED_TRACE("operator " + std::to_string(static_cast<int>(quarters.op)) + " over [" +
                 text(child.lower()) + ", " + text(child.upper()) + "] in quarter " +
                 std::to_string(quarter));
    const Profile profile = wave(*quarters.shape, child);
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
    expect_root(operator_graph(quarters.op, kFixed), box, profile.curvature, monotonicities);
}

void expect_sides(const Quarters &quarters, int quarter, int gap)
{
    const Real near = quarter * kHalfPi;
    const Real below = shifted(near, -gap);
    const Real above = shifted(near, gap);
    const bool exact = gap > 1;
    expect_quarter(quarters, {shifted(below, -kSteps), below}, quarter - 1, exact);
    expect_quarter(quarters, {above, shifted(above, kSteps)}, quarter, exact);
    for (const Real reach : kReaches)
    {
        expect_quarter(quarters, {below - reach, below}, quarter - 1, exact);
        expect_quarter(quarters, {above, above + reach}, quarter, exact);
    }
}

std::string row_text(const Row &row)
{
    return "operator " + std::to_string(static_cast<int>(row.op)) + " over [" +
           text(row.child.lower()) + ", " + text(row.child.upper()) + "]";
}

}

TEST(CurvatureTest, Waves)
{
    for (const Row &row : kRows)
    {
        SCOPED_TRACE(row_text(row));
        const Profile profile = wave(*row.shape, row.child);
        EXPECT_EQ(profile.curvature, row.curvature);
        EXPECT_EQ(profile.argument.monotonicity, row.monotonicity);
        EXPECT_FALSE(profile.argument.cut_below || profile.argument.cut_above);
        const std::array box{row.child, kZero, kZero};
        const std::array monotonicities{profile.argument.monotonicity};
        expect_root(operator_graph(row.op, kFixed), box, profile.curvature, monotonicities);
    }
}

TEST(CurvatureTest, Straddles)
{
    for (int quarter = -40; quarter <= 40; ++quarter)
    {
        const Real near = quarter * kHalfPi;
        const Real below = std::nextafter(std::nextafter(near, -kInfinity), -kInfinity);
        const Real above = std::nextafter(std::nextafter(near, kInfinity), kInfinity);
        const Interval across(below, above);
        const bool odd = quarter % 2 != 0;
        SCOPED_TRACE("quarter " + std::to_string(quarter));
        const Profile sine = wave(kSine, across);
        const Profile cosine = wave(kCosine, across);
        const Profile tangent = wave(kTangent, across);
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
