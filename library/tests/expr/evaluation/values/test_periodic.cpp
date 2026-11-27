#include "bounds.hpp"
#include "core/numbers.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/analysis/intervals/periodic.hpp"
#include "expr/analysis/intervals/rounding.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace
{

using biyos::cos;
using biyos::Interval;
using biyos::Real;
using biyos::shifted;
using biyos::sin;
using biyos::tan;
using biyos::widen_down;
using biyos::widen_up;
using biyos::test::exact;
using biyos::test::holds;
using biyos::test::kHalfPiUp;
using biyos::test::kPiUp;
using biyos::test::within;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kPi = std::numbers::pi;
constexpr Real kHalfPi = std::numbers::pi / 2;
constexpr Real kHuge = 1e22;
constexpr Real kSinPi = 0x1.1a62633145c07p-53;
constexpr Real kSinHuge = -0x1.b453ab76bf397p-1;
constexpr Real kCosHuge = 0x1.0be2cef01c8f4p-1;
constexpr Real kTanHalfPi = 0x1.d02967c31cdb5p+53;
constexpr Real kTanHalfPiUp = -0x1.617a15494767ap+52;

using Function = Interval (*)(Interval) noexcept;
using Value = Real (*)(Real);

struct Point
{
    Function interval;
    Value value;
    Real at;
    Real truth;
};

testing::AssertionResult near(const Point &point, Interval box)
{
    const Interval found = point.interval(box);
    const Real computed = point.value(point.at);
    const testing::AssertionResult held =
        holds(found, shifted(point.truth, -1), shifted(point.truth, 1));
    return held ? within(found, widen_down(computed), widen_up(computed)) : held;
}

Real sine(Real x)
{
    return std::sin(x);
}

Real cosine(Real x)
{
    return std::cos(x);
}

Real tangent(Real x)
{
    return std::tan(x);
}

}

TEST(IntervalTest, Sine)
{
    EXPECT_TRUE(holds(sin({0.0, kPiUp}), std::sin(kPiUp), 1.0));
    EXPECT_TRUE(within(sin({0.0, kPiUp}), widen_down(std::sin(kPiUp)), 1.0));
    EXPECT_TRUE(holds(sin({1.5, 1.6}), std::sin(1.5), 1.0));
    EXPECT_TRUE(within(sin({1.5, 1.6}), widen_down(std::sin(1.5)), 1.0));
    EXPECT_TRUE(holds(sin({0.0, 4.0}), std::sin(4.0), 1.0));
    EXPECT_TRUE(within(sin({0.0, 4.0}), widen_down(std::sin(4.0)), 1.0));
    EXPECT_TRUE(exact(sin({1.0, 5.0}), -1.0, 1.0));
    EXPECT_TRUE(exact(sin({0.0, 100.0}), -1.0, 1.0));
    EXPECT_TRUE(exact(sin({-kInfinity, 0.0}), -1.0, 1.0));
    EXPECT_TRUE(sin(Interval::empty()).is_empty());
}

TEST(IntervalTest, Cosine)
{
    EXPECT_TRUE(holds(cos({-1.0, 1.0}), std::cos(1.0), 1.0));
    EXPECT_TRUE(within(cos({-1.0, 1.0}), widen_down(std::cos(1.0)), 1.0));
    EXPECT_TRUE(holds(cos({3.0, 3.5}), -1.0, std::cos(3.5)));
    EXPECT_TRUE(within(cos({3.0, 3.5}), -1.0, widen_up(std::cos(3.5))));
    EXPECT_TRUE(holds(cos({0.5, 1.0}), std::cos(1.0), std::cos(0.5)));
    const Real low = widen_down(std::cos(1.0));
    EXPECT_TRUE(within(cos({0.5, 1.0}), low, widen_up(std::cos(0.5))));
    EXPECT_TRUE(exact(cos({0.0, kInfinity}), -1.0, 1.0));
    EXPECT_TRUE(cos(Interval::empty()).is_empty());
}

TEST(IntervalTest, Tangent)
{
    EXPECT_TRUE(exact(tan({1.0, 2.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(tan({4.0, 5.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(tan({0.0, 10.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(tan({-kInfinity, 0.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(holds(tan({-1.0, 1.0}), std::tan(-1.0), std::tan(1.0)));
    const Real low = widen_down(std::tan(-1.0));
    EXPECT_TRUE(within(tan({-1.0, 1.0}), low, widen_up(std::tan(1.0))));
    EXPECT_TRUE(holds(tan({2.0, 4.0}), std::tan(2.0), std::tan(4.0)));
    const Real high = widen_up(std::tan(4.0));
    EXPECT_TRUE(within(tan({2.0, 4.0}), widen_down(std::tan(2.0)), high));
    EXPECT_TRUE(tan(Interval::empty()).is_empty());
}

TEST(IntervalTest, Points)
{
    const Interval zeros(-0.0, 0.0);
    EXPECT_TRUE(holds(cos({0.0, 0.0}), 1.0, 1.0));
    EXPECT_TRUE(within(cos({0.0, 0.0}), widen_down(1.0), 1.0));
    EXPECT_TRUE(holds(cos(zeros), 1.0, 1.0));
    EXPECT_TRUE(within(cos(zeros), widen_down(1.0), 1.0));
    EXPECT_TRUE(holds(sin({0.0, 0.0}), 0.0, 0.0));
    EXPECT_TRUE(within(sin({0.0, 0.0}), widen_down(0.0), widen_up(0.0)));
    EXPECT_TRUE(holds(sin(zeros), 0.0, 0.0));
    EXPECT_TRUE(within(sin(zeros), widen_down(0.0), widen_up(0.0)));
    EXPECT_TRUE(holds(cos({kPi, kPi}), -1.0, shifted(-1.0, 1)));
    EXPECT_TRUE(within(cos({kPi, kPi}), -1.0, widen_up(std::cos(kPi))));
    const std::array points{
        Point{.interval = sin, .value = sine, .at = kPi, .truth = kSinPi},
        Point{.interval = sin, .value = sine, .at = kHuge, .truth = kSinHuge},
        Point{.interval = cos, .value = cosine, .at = kHuge, .truth = kCosHuge},
        Point{.interval = tan, .value = tangent, .at = kHalfPi, .truth = kTanHalfPi},
        Point{.interval = tan, .value = tangent, .at = kHalfPiUp, .truth = kTanHalfPiUp},
    };
    for (const Point &point : points)
    {
        EXPECT_TRUE(near(point, {point.at, point.at})) << point.at;
    }
}
