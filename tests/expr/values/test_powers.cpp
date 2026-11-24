#include "bounds.hpp"
#include "expr/interval.hpp"
#include "expr/powers.hpp"
#include "expr/rounding.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace
{

using biyos::Interval;
using biyos::widen_down;
using biyos::widen_up;
using biyos::test::exact;
using biyos::test::holds;
using biyos::test::within;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr Interval kAll = Interval::entire();

Interval raised(Interval base, double exponent)
{
    return biyos::power(base, {exponent, exponent});
}

}

TEST(IntervalTest, ConstantPower)
{
    EXPECT_TRUE(holds(raised({-1.0, 2.0}, 2.0), 0.0, 4.0));
    EXPECT_TRUE(within(raised({-1.0, 2.0}, 2.0), 0.0, widen_up(4.0)));
    EXPECT_TRUE(holds(raised({-1.0, 2.0}, 3.0), -1.0, 8.0));
    EXPECT_TRUE(within(raised({-1.0, 2.0}, 3.0), widen_down(-1.0), widen_up(8.0)));
    EXPECT_TRUE(holds(raised({-2.0, -1.0}, 4.0), 1.0, 16.0));
    EXPECT_TRUE(within(raised({-2.0, -1.0}, 4.0), widen_down(1.0), widen_up(16.0)));
    EXPECT_TRUE(exact(raised({-1.0, 1.0}, -1.0), -kInfinity, kInfinity));
    EXPECT_TRUE(holds(raised({0.0, 2.0}, -1.0), 0.5, kInfinity));
    EXPECT_TRUE(holds(raised({-1.0, 1.0}, -2.0), 1.0, kInfinity));
    EXPECT_TRUE(within(raised({-1.0, 1.0}, -2.0), widen_down(1.0), kInfinity));
    EXPECT_TRUE(holds(raised({-2.0, -1.0}, -3.0), -1.0, -0.125));
    EXPECT_TRUE(within(raised({-2.0, -1.0}, -3.0), widen_down(-1.0), widen_up(-0.125)));
    EXPECT_TRUE(holds(raised({-1.0, 4.0}, 0.5), 0.0, 2.0));
    EXPECT_TRUE(within(raised({-1.0, 4.0}, 0.5), 0.0, widen_up(2.0)));
    EXPECT_TRUE(holds(raised({0.0, 4.0}, -0.5), 0.5, kInfinity));
    EXPECT_TRUE(holds(raised({-1.0, 4.0}, 1.5), 0.0, 8.0));
    EXPECT_TRUE(holds(raised({-5.0, 5.0}, 0.0), 1.0, 1.0));
    EXPECT_TRUE(within(raised({-5.0, 5.0}, 0.0), widen_down(1.0), widen_up(1.0)));
    EXPECT_TRUE(exact(raised({0.0, 0.0}, 0.0), 1.0, 1.0));
    EXPECT_TRUE(exact(raised(kAll, 2.0), 0.0, kInfinity));
    EXPECT_TRUE(holds(raised({-kInfinity, -1.0}, 3.0), -kInfinity, -1.0));
    EXPECT_TRUE(raised({-2.0, -1.0}, 0.5).is_empty());
    EXPECT_TRUE(raised({0.0, 0.0}, -1.0).is_empty());
    EXPECT_TRUE(raised({0.0, 0.0}, -0.5).is_empty());
    EXPECT_TRUE(raised({-1.0, 0.0}, -0.5).is_empty());
}

TEST(IntervalTest, Power)
{
    EXPECT_TRUE(holds(biyos::power({-2.0, -1.0}, {2.0, 3.0}), -8.0, 4.0));
    EXPECT_TRUE(holds(biyos::power({-8.0, -8.0}, {3.0, 3.0}), -512.0, -512.0));
    EXPECT_TRUE(holds(biyos::power({1.0, 2.0}, {-1.0, 1.0}), 0.5, 2.0));
    EXPECT_TRUE(within(biyos::power({1.0, 2.0}, {-1.0, 1.0}), widen_down(0.5), widen_up(2.0)));
    EXPECT_TRUE(exact(biyos::power({0.0, 0.0}, {-1.0, 1.0}), 0.0, 1.0));
    EXPECT_TRUE(holds(biyos::power({2.0, kInfinity}, {1.0, kInfinity}), 2.0, kInfinity));
    EXPECT_TRUE(holds(biyos::power({0.5, 2.0}, kAll), 0.0, kInfinity));
    EXPECT_TRUE(biyos::power({-2.0, -1.0}, {0.2, 0.8}).is_empty());
    EXPECT_TRUE(biyos::power({0.0, 0.0}, {-2.0, -1.0}).is_empty());
    EXPECT_TRUE(biyos::power(Interval::empty(), {1.0, 2.0}).is_empty());
    EXPECT_TRUE(biyos::power({1.0, 2.0}, Interval::empty()).is_empty());
}

TEST(IntervalTest, SignPower)
{
    const double root = std::pow(8.0, 1.0 / 3);
    EXPECT_TRUE(holds(biyos::sign_power({-8.0, 8.0}, 1.0 / 3), -root, root));
    const double low = widen_down(-root);
    EXPECT_TRUE(within(biyos::sign_power({-8.0, 8.0}, 1.0 / 3), low, widen_up(root)));
    EXPECT_TRUE(holds(biyos::sign_power({0.0, 2.0}, 2.0), 0.0, 4.0));
    EXPECT_TRUE(within(biyos::sign_power({0.0, 2.0}, 2.0), 0.0, widen_up(4.0)));
    EXPECT_TRUE(within(biyos::sign_power({-2.0, 0.0}, 0.5), -kInfinity, 0.0));
    EXPECT_TRUE(exact(biyos::sign_power(kAll, 1.5), -kInfinity, kInfinity));
    EXPECT_TRUE(biyos::sign_power(Interval::empty(), 2.0).is_empty());
}
