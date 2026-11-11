#include "bounds.hpp"
#include "expr/interval.hpp"
#include "expr/rounding.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{

using biyos::hull;
using biyos::Interval;
using biyos::maximum;
using biyos::meet;
using biyos::minimum;
using biyos::shifted;
using biyos::signs;
using biyos::widen_down;
using biyos::widen_up;
using biyos::test::exact;
using biyos::test::holds;
using biyos::test::within;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kLargest = (std::numeric_limits<double>::max)();
constexpr double kSmallest = std::numeric_limits<double>::denorm_min();
constexpr double kTiny = 1e-200;
constexpr int kBoundUlps = biyos::kLibmUlps + 1;
constexpr Interval kAll = Interval::entire();

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

double ulp_of(double value)
{
    if (std::fpclassify(value) == FP_ZERO || std::fpclassify(value) == FP_SUBNORMAL)
    {
        return kSmallest;
    }
    return std::ldexp(1.0, std::ilogb(value) - 52);
}

double above(double value, int ulps)
{
    return std::nextafter(value + ulps * ulp_of(value), kInfinity);
}

double below(double value, int ulps)
{
    return std::nextafter(value - ulps * ulp_of(value), -kInfinity);
}

std::vector<double> exact_values()
{
    std::vector<double> values{0.0, kSmallest, 3 * kSmallest, 0x1p-1022, 0.1, 1.0 / 3};
    for (const int exponent : {-1074, -1060, -1023, -1022, -60, -1, 0, 1, 52, 600, 1023})
    {
        const double power = std::ldexp(1.0, exponent);
        values.push_back(power);
        values.push_back(std::nextafter(power, 0.0));
        values.push_back(std::nextafter(power, kInfinity));
    }
    const std::size_t count = values.size();
    for (std::size_t k = 0; k < count; ++k)
    {
        values.push_back(-values[k]);
    }
    return values;
}

Interval point(double value)
{
    return {value, value};
}

}

TEST(IntervalTest, Bounds)
{
    EXPECT_TRUE(Interval(2.0, 1.0).is_empty());
    EXPECT_TRUE(Interval(kNan, 1.0).is_empty());
    EXPECT_TRUE(Interval(1.0, kNan).is_empty());
    EXPECT_TRUE(Interval(kInfinity, kInfinity).is_empty());
    EXPECT_TRUE(Interval(-kInfinity, -kInfinity).is_empty());
    EXPECT_FALSE(Interval(1.0, 1.0).is_empty());
    EXPECT_EQ(bits(Interval::empty().lower()), bits(kInfinity));
    EXPECT_EQ(bits(Interval::empty().upper()), bits(-kInfinity));
    EXPECT_EQ(bits(Interval(2.0, 1.0).lower()), bits(kInfinity));
    EXPECT_EQ(bits(Interval(2.0, 1.0).upper()), bits(-kInfinity));
    EXPECT_TRUE(exact(kAll, -kInfinity, kInfinity));
    EXPECT_TRUE(exact(hull(Interval::empty(), {1.0, 2.0}), 1.0, 2.0));
    EXPECT_TRUE(exact(hull({-1.0, 0.0}, {1.0, 2.0}), -1.0, 2.0));
    EXPECT_TRUE(exact(meet({0.0, 2.0}, {1.0, 3.0}), 1.0, 2.0));
    EXPECT_TRUE(meet({0.0, 1.0}, {2.0, 3.0}).is_empty());
    EXPECT_TRUE(exact(signs({1.0, 2.0}, 1.0), 0.0, kInfinity));
    EXPECT_TRUE(exact(signs({-1.0, 1.0}, 1.0), -kInfinity, 0.0));
    EXPECT_TRUE(exact(signs({-1.0, 2.0}, 1.0), -kInfinity, kInfinity));
}

TEST(IntervalTest, Steps)
{
    EXPECT_EQ(bits(shifted(1.0, 1)), bits(std::nextafter(1.0, kInfinity)));
    EXPECT_EQ(bits(shifted(1.0, -3)), bits(1.0 - 3 * 0x1p-53));
    EXPECT_EQ(bits(shifted(0.0, -1)), bits(-kSmallest));
    EXPECT_EQ(bits(shifted(-0.0, 1)), bits(kSmallest));
    EXPECT_EQ(bits(shifted(kSmallest, -2)), bits(-kSmallest));
    EXPECT_EQ(bits(shifted(kLargest, 1)), bits(kInfinity));
    EXPECT_EQ(bits(shifted(kInfinity, 5)), bits(kInfinity));
    EXPECT_EQ(bits(shifted(kInfinity, -1)), bits(kLargest));
    EXPECT_EQ(bits(shifted(-kInfinity, -5)), bits(-kInfinity));
    EXPECT_TRUE(std::isnan(shifted(kNan, 1)));
    EXPECT_EQ(bits(biyos::round_down(0.0)), bits(0.0));
    EXPECT_EQ(bits(biyos::round_down(-0.0)), bits(-kSmallest));
    EXPECT_EQ(bits(biyos::round_up(-0.0)), bits(-0.0));
    EXPECT_EQ(bits(biyos::round_up(0.0)), bits(kSmallest));
    EXPECT_EQ(bits(biyos::round_down(2.0)), bits(std::nextafter(2.0, 0.0)));
    EXPECT_EQ(bits(widen_down(1.0)), bits(shifted(1.0, -biyos::kLibmSteps)));
    EXPECT_EQ(bits(widen_up(1.0)), bits(shifted(1.0, biyos::kLibmSteps)));
}

TEST(IntervalTest, Accuracy)
{
    for (const double value : exact_values())
    {
        const double high = above(value, kBoundUlps);
        const double low = below(value, kBoundUlps);
        EXPECT_LE(widen_down(high), low) << value;
        EXPECT_GE(widen_up(low), high) << value;
    }
}

TEST(IntervalTest, Sums)
{
    const Interval sum = Interval(1.0, 2.0) + Interval(0.1, 0.2);
    EXPECT_TRUE(holds(sum, 1.1, 2.2));
    EXPECT_TRUE(within(sum, shifted(1.1, -1), shifted(2.2, 1)));
    EXPECT_TRUE(exact(point(0.0) + Interval(1.0, 2.0), 1.0, 2.0));
    EXPECT_TRUE(exact(Interval(-kInfinity, 0.0) + Interval(1.0, 2.0), -kInfinity, 2.0));
    EXPECT_TRUE(exact(point(kLargest) + point(kLargest), kLargest, kInfinity));
    EXPECT_TRUE((Interval::empty() + kAll).is_empty());
}

TEST(IntervalTest, Products)
{
    EXPECT_TRUE(exact(point(0.0) * kAll, 0.0, 0.0));
    EXPECT_TRUE(holds(point(2.0) * Interval(-kInfinity, 1.0), -kInfinity, 2.0));
    EXPECT_TRUE(within(point(2.0) * Interval(-kInfinity, 1.0), -kInfinity, shifted(2.0, 1)));
    EXPECT_TRUE(holds(Interval(-2.0, 3.0) * Interval(-1.0, 4.0), -8.0, 12.0));
    EXPECT_TRUE(
        within(Interval(-2.0, 3.0) * Interval(-1.0, 4.0), shifted(-8.0, -1), shifted(12.0, 1)));
    const Interval negative(-kInfinity, -1.0);
    EXPECT_TRUE(holds(negative * negative, 1.0, kInfinity));
    EXPECT_TRUE(within(negative * negative, shifted(1.0, -1), kInfinity));
    EXPECT_TRUE(exact(point(kTiny) * point(kTiny), 0.0, kSmallest));
    EXPECT_TRUE(exact(point(-kTiny) * point(kTiny), -kSmallest, 0.0));
    EXPECT_TRUE((Interval::empty() * kAll).is_empty());
}

TEST(IntervalTest, Division)
{
    const Interval one = point(1.0);
    EXPECT_TRUE(holds(one / Interval(0.0, 1.0), 1.0, kInfinity));
    EXPECT_TRUE(within(one / Interval(0.0, 1.0), shifted(1.0, -1), kInfinity));
    EXPECT_TRUE(holds(one / Interval(-1.0, 0.0), -kInfinity, -1.0));
    EXPECT_TRUE(within(one / Interval(-1.0, 0.0), -kInfinity, shifted(-1.0, 1)));
    EXPECT_TRUE(exact(one / Interval(-1.0, 1.0), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(point(0.0) / Interval(-1.0, 1.0), 0.0, 0.0));
    EXPECT_TRUE(exact(Interval(0.0, 2.0) / Interval(0.0, 1.0), 0.0, kInfinity));
    EXPECT_TRUE(exact(Interval(-1.0, 2.0) / Interval(0.0, 1.0), -kInfinity, kInfinity));
    EXPECT_TRUE(holds(Interval(-2.0, -1.0) / Interval(-1.0, 0.0), 1.0, kInfinity));
    EXPECT_TRUE(holds(Interval(1.0, 2.0) / Interval(2.0, 4.0), 0.25, 1.0));
    EXPECT_TRUE(
        within(Interval(1.0, 2.0) / Interval(2.0, 4.0), shifted(0.25, -1), shifted(1.0, 1)));
    EXPECT_TRUE(holds(Interval(1.0, 2.0) / Interval(-4.0, -2.0), -1.0, -0.25));
    EXPECT_TRUE(exact(kAll / Interval(1.0, kInfinity), -kInfinity, kInfinity));
    EXPECT_TRUE(holds(Interval(1.0, 2.0) / Interval(1.0, kInfinity), 0.0, 2.0));
    EXPECT_TRUE(within(Interval(1.0, 2.0) / Interval(1.0, kInfinity), 0.0, shifted(2.0, 1)));
    EXPECT_TRUE((Interval(1.0, 2.0) / point(0.0)).is_empty());
    EXPECT_TRUE((point(0.0) / point(0.0)).is_empty());
    EXPECT_TRUE((Interval(0.0, 1.0) / Interval::empty()).is_empty());
}

TEST(IntervalTest, Exact)
{
    EXPECT_TRUE(exact(abs(Interval(-3.0, 2.0)), 0.0, 3.0));
    EXPECT_TRUE(exact(abs(Interval(-3.0, -2.0)), 2.0, 3.0));
    EXPECT_TRUE(exact(abs(Interval(2.0, 3.0)), 2.0, 3.0));
    EXPECT_TRUE(exact(-Interval(1.0, 2.0), -2.0, -1.0));
    EXPECT_TRUE(exact(maximum({-1.0, 2.0}, {0.0, 1.0}), 0.0, 2.0));
    EXPECT_TRUE(exact(minimum({-1.0, 2.0}, {0.0, 1.0}), -1.0, 1.0));
    EXPECT_TRUE(maximum(Interval::empty(), {0.0, 1.0}).is_empty());
    EXPECT_TRUE(minimum({0.0, 1.0}, Interval::empty()).is_empty());
    EXPECT_TRUE(abs(Interval::empty()).is_empty());
}
