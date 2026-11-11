#include "bounds.hpp"
#include "expr/interval.hpp"
#include "expr/periodic.hpp"
#include "expr/rounding.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace
{

using biyos::Interval;
using biyos::shifted;
using biyos::widen_down;
using biyos::widen_up;
using biyos::test::exact;
using biyos::test::holds;
using biyos::test::within;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kPiUp = 0x1.921fb54442d19p+1;
constexpr double kPi = std::numbers::pi;
constexpr double kHalfPi = std::numbers::pi / 2;
constexpr double kHalfPiUp = 0x1.921fb54442d19p+0;
constexpr double kHuge = 1e22;
constexpr double kSinPi = 0x1.1a62633145c07p-53;
constexpr double kSinHuge = -0x1.b453ab76bf397p-1;
constexpr double kCosHuge = 0x1.0be2cef01c8f4p-1;
constexpr double kTanHalfPi = 0x1.d02967c31cdb5p+53;
constexpr double kTanHalfPiUp = -0x1.617a15494767ap+52;

using Function = Interval (*)(Interval) noexcept;
using Value = double (*)(double);

struct Point
{
    Function interval;
    Value value;
    double at;
    double truth;
};

testing::AssertionResult near(const Point &point, Interval box)
{
    const Interval found = point.interval(box);
    const double computed = point.value(point.at);
    const testing::AssertionResult held =
        holds(found, shifted(point.truth, -1), shifted(point.truth, 1));
    return held ? within(found, widen_down(computed), widen_up(computed)) : held;
}

double sine(double x)
{
    return std::sin(x);
}

double cosine(double x)
{
    return std::cos(x);
}

double tangent(double x)
{
    return std::tan(x);
}

}

TEST(IntervalTest, Sine)
{
    EXPECT_TRUE(holds(biyos::sin({0.0, kPiUp}), std::sin(kPiUp), 1.0));
    EXPECT_TRUE(within(biyos::sin({0.0, kPiUp}), widen_down(std::sin(kPiUp)), 1.0));
    EXPECT_TRUE(holds(biyos::sin({1.5, 1.6}), std::sin(1.5), 1.0));
    EXPECT_TRUE(within(biyos::sin({1.5, 1.6}), widen_down(std::sin(1.5)), 1.0));
    EXPECT_TRUE(holds(biyos::sin({0.0, 4.0}), std::sin(4.0), 1.0));
    EXPECT_TRUE(within(biyos::sin({0.0, 4.0}), widen_down(std::sin(4.0)), 1.0));
    EXPECT_TRUE(exact(biyos::sin({1.0, 5.0}), -1.0, 1.0));
    EXPECT_TRUE(exact(biyos::sin({0.0, 100.0}), -1.0, 1.0));
    EXPECT_TRUE(exact(biyos::sin({-kInfinity, 0.0}), -1.0, 1.0));
    EXPECT_TRUE(biyos::sin(Interval::empty()).is_empty());
}

TEST(IntervalTest, Cosine)
{
    EXPECT_TRUE(holds(biyos::cos({-1.0, 1.0}), std::cos(1.0), 1.0));
    EXPECT_TRUE(within(biyos::cos({-1.0, 1.0}), widen_down(std::cos(1.0)), 1.0));
    EXPECT_TRUE(holds(biyos::cos({3.0, 3.5}), -1.0, std::cos(3.5)));
    EXPECT_TRUE(within(biyos::cos({3.0, 3.5}), -1.0, widen_up(std::cos(3.5))));
    EXPECT_TRUE(holds(biyos::cos({0.5, 1.0}), std::cos(1.0), std::cos(0.5)));
    const double low = widen_down(std::cos(1.0));
    EXPECT_TRUE(within(biyos::cos({0.5, 1.0}), low, widen_up(std::cos(0.5))));
    EXPECT_TRUE(exact(biyos::cos({0.0, kInfinity}), -1.0, 1.0));
    EXPECT_TRUE(biyos::cos(Interval::empty()).is_empty());
}

TEST(IntervalTest, Tangent)
{
    EXPECT_TRUE(exact(biyos::tan({1.0, 2.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(biyos::tan({4.0, 5.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(biyos::tan({0.0, 10.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(biyos::tan({-kInfinity, 0.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(holds(biyos::tan({-1.0, 1.0}), std::tan(-1.0), std::tan(1.0)));
    const double low = widen_down(std::tan(-1.0));
    EXPECT_TRUE(within(biyos::tan({-1.0, 1.0}), low, widen_up(std::tan(1.0))));
    EXPECT_TRUE(holds(biyos::tan({2.0, 4.0}), std::tan(2.0), std::tan(4.0)));
    const double high = widen_up(std::tan(4.0));
    EXPECT_TRUE(within(biyos::tan({2.0, 4.0}), widen_down(std::tan(2.0)), high));
    EXPECT_TRUE(biyos::tan(Interval::empty()).is_empty());
}

TEST(IntervalTest, Points)
{
    const Interval zeros(-0.0, 0.0);
    EXPECT_TRUE(holds(biyos::cos({0.0, 0.0}), 1.0, 1.0));
    EXPECT_TRUE(within(biyos::cos({0.0, 0.0}), widen_down(1.0), 1.0));
    EXPECT_TRUE(holds(biyos::cos(zeros), 1.0, 1.0));
    EXPECT_TRUE(within(biyos::cos(zeros), widen_down(1.0), 1.0));
    EXPECT_TRUE(holds(biyos::sin({0.0, 0.0}), 0.0, 0.0));
    EXPECT_TRUE(within(biyos::sin({0.0, 0.0}), widen_down(0.0), widen_up(0.0)));
    EXPECT_TRUE(holds(biyos::sin(zeros), 0.0, 0.0));
    EXPECT_TRUE(within(biyos::sin(zeros), widen_down(0.0), widen_up(0.0)));
    EXPECT_TRUE(holds(biyos::cos({kPi, kPi}), -1.0, shifted(-1.0, 1)));
    EXPECT_TRUE(within(biyos::cos({kPi, kPi}), -1.0, widen_up(std::cos(kPi))));
    const std::array points{
        Point{.interval = biyos::sin, .value = sine, .at = kPi, .truth = kSinPi},
        Point{.interval = biyos::sin, .value = sine, .at = kHuge, .truth = kSinHuge},
        Point{.interval = biyos::cos, .value = cosine, .at = kHuge, .truth = kCosHuge},
        Point{.interval = biyos::tan, .value = tangent, .at = kHalfPi, .truth = kTanHalfPi},
        Point{.interval = biyos::tan, .value = tangent, .at = kHalfPiUp, .truth = kTanHalfPiUp},
    };
    for (const Point &point : points)
    {
        EXPECT_TRUE(near(point, {point.at, point.at})) << point.at;
    }
}
