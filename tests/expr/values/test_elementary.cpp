#include "bounds.hpp"
#include "expr/elementary.hpp"
#include "expr/interval.hpp"
#include "expr/rounding.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

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
constexpr double kLargest = (std::numeric_limits<double>::max)();
constexpr double kHalfPiUp = 0x1.921fb54442d19p+0;
constexpr double kPiUp = 0x1.921fb54442d19p+1;
constexpr double kInverseE = 0x1.78b56362cef38p-2;
constexpr int kWide = 2 * biyos::kLibmSteps;
constexpr Interval kAll = Interval::entire();

}

TEST(IntervalTest, Domains)
{
    EXPECT_TRUE(holds(biyos::sqrt({-1.0, 4.0}), 0.0, 2.0));
    EXPECT_TRUE(within(biyos::sqrt({-1.0, 4.0}), 0.0, shifted(2.0, 1)));
    EXPECT_TRUE(exact(biyos::log({0.0, 1.0}), -kInfinity, 0.0));
    EXPECT_TRUE(exact(biyos::log({1.0, kInfinity}), 0.0, kInfinity));
    EXPECT_TRUE(holds(biyos::asin({0.5, 3.0}), std::asin(0.5), kHalfPiUp));
    EXPECT_TRUE(within(biyos::asin({0.5, 3.0}), widen_down(std::asin(0.5)), kHalfPiUp));
    EXPECT_TRUE(exact(biyos::asin({-3.0, 3.0}), -kHalfPiUp, kHalfPiUp));
    EXPECT_TRUE(holds(biyos::acos({-3.0, -0.5}), std::acos(-0.5), kPiUp));
    EXPECT_TRUE(within(biyos::acos({-3.0, -0.5}), widen_down(std::acos(-0.5)), kPiUp));
    EXPECT_TRUE(exact(biyos::acos({1.0, 2.0}), 0.0, 0.0));
    EXPECT_TRUE(exact(biyos::acosh({0.0, 1.0}), 0.0, 0.0));
    EXPECT_TRUE(within(biyos::acosh({1.0, 2.0}), 0.0, kInfinity));
    EXPECT_TRUE(holds(biyos::atanh({0.5, 1.0}), std::atanh(0.5), kInfinity));
    EXPECT_TRUE(holds(biyos::atanh({-1.0, -0.5}), -kInfinity, std::atanh(-0.5)));
    EXPECT_TRUE(exact(biyos::atanh({-1.0, 1.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(holds(biyos::cosh({-1.0, 2.0}), 1.0, std::cosh(2.0)));
    EXPECT_TRUE(within(biyos::cosh({-1.0, 2.0}), 1.0, widen_up(std::cosh(2.0))));
    EXPECT_TRUE(holds(biyos::sinh({0.0, 1.0}), 0.0, std::sinh(1.0)));
    EXPECT_TRUE(within(biyos::sinh({0.0, 1.0}), 0.0, kInfinity));
    EXPECT_TRUE(exact(biyos::tanh({-kInfinity, 0.0}), -1.0, 0.0));
    EXPECT_TRUE(holds(biyos::exp({-kInfinity, 0.0}), 0.0, 1.0));
    EXPECT_TRUE(within(biyos::exp({-kInfinity, 0.0}), 0.0, widen_up(1.0)));
}

TEST(IntervalTest, Entropy)
{
    EXPECT_TRUE(holds(biyos::entropy({0.0, 1.0}), -kInverseE, 0.0));
    EXPECT_TRUE(within(biyos::entropy({0.0, 1.0}), shifted(-kInverseE, -kWide), 0.0));
    EXPECT_TRUE(holds(biyos::entropy({0.2, 2.0}), -kInverseE, 2 * std::log(2.0)));
    const double half = 0.5 * std::log(0.5);
    EXPECT_TRUE(holds(biyos::entropy({0.5, 2.0}), half, 2 * std::log(2.0)));
    EXPECT_TRUE(within(biyos::entropy({0.5, 2.0}), widen_down(half), kInfinity));
    const double fifth = 0.2 * std::log(0.2);
    EXPECT_TRUE(holds(biyos::entropy({0.1, 0.2}), fifth, 0.1 * std::log(0.1)));
    EXPECT_TRUE(within(biyos::entropy({0.1, 0.2}), widen_down(fifth), 0.0));
    EXPECT_TRUE(holds(biyos::entropy({1.0, 2.0}), 0.0, 2 * std::log(2.0)));
    EXPECT_TRUE(within(biyos::entropy({1.0, 2.0}), 0.0, kInfinity));
    EXPECT_TRUE(holds(biyos::entropy({-1.0, 0.0}), 0.0, 0.0));
    EXPECT_TRUE(within(biyos::entropy({-1.0, 0.0}), widen_down(0.0), 0.0));
}

TEST(IntervalTest, Unbounded)
{
    EXPECT_TRUE(holds(biyos::exp({0.0, kInfinity}), 1.0, kInfinity));
    EXPECT_TRUE(holds(biyos::sqrt({4.0, kInfinity}), 2.0, kInfinity));
    EXPECT_TRUE(holds(biyos::atan(kAll), -std::atan(kInfinity), std::atan(kInfinity)));
    EXPECT_TRUE(within(biyos::atan(kAll), -kHalfPiUp, kHalfPiUp));
    EXPECT_TRUE(exact(biyos::tanh(kAll), -1.0, 1.0));
    EXPECT_TRUE(exact(biyos::erf(kAll), -1.0, 1.0));
    EXPECT_TRUE(exact(biyos::cosh({-kInfinity, 0.0}), 1.0, kInfinity));
    EXPECT_TRUE(exact(biyos::asinh(kAll), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(biyos::entropy({1.0, kInfinity}), 0.0, kInfinity));
    EXPECT_TRUE(holds(biyos::sinh({800.0, 900.0}), kLargest, kInfinity));
    EXPECT_GT(biyos::sinh({800.0, 900.0}).lower(), kLargest / 2);
    EXPECT_TRUE(holds(biyos::exp({1000.0, 1000.0}), kLargest, kInfinity));
    EXPECT_TRUE(holds(biyos::sinh({-900.0, -800.0}), -kInfinity, -kLargest));
}

TEST(IntervalTest, Empty)
{
    EXPECT_TRUE(biyos::log({-2.0, -1.0}).is_empty());
    EXPECT_TRUE(biyos::log({-1.0, 0.0}).is_empty());
    EXPECT_TRUE(biyos::sqrt({-2.0, -1.0}).is_empty());
    EXPECT_TRUE(biyos::asin({2.0, 3.0}).is_empty());
    EXPECT_TRUE(biyos::acos({-3.0, -2.0}).is_empty());
    EXPECT_TRUE(biyos::acosh({0.0, 0.5}).is_empty());
    EXPECT_TRUE(biyos::atanh({1.0, 2.0}).is_empty());
    EXPECT_TRUE(biyos::atanh({-2.0, -1.0}).is_empty());
    EXPECT_TRUE(biyos::entropy({-2.0, -1.0}).is_empty());
    EXPECT_TRUE(biyos::exp(Interval::empty()).is_empty());
    EXPECT_TRUE(biyos::cosh(Interval::empty()).is_empty());
    EXPECT_TRUE(biyos::entropy(Interval::empty()).is_empty());
}
