#include "bounds.hpp"
#include "core/numbers.hpp"
#include "expr/intervals/elementary.hpp"
#include "expr/intervals/interval.hpp"
#include "expr/intervals/rounding.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace
{

using biyos::acos;
using biyos::acosh;
using biyos::asin;
using biyos::asinh;
using biyos::atan;
using biyos::atanh;
using biyos::cosh;
using biyos::entropy;
using biyos::erf;
using biyos::exp;
using biyos::Interval;
using biyos::kLibmSteps;
using biyos::log;
using biyos::Real;
using biyos::shifted;
using biyos::sinh;
using biyos::sqrt;
using biyos::tanh;
using biyos::widen_down;
using biyos::widen_up;
using biyos::test::exact;
using biyos::test::holds;
using biyos::test::within;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kLargest = (std::numeric_limits<Real>::max)();
constexpr Real kHalfPiUp = 0x1.921fb54442d19p+0;
constexpr Real kPiUp = 0x1.921fb54442d19p+1;
constexpr Real kInverseE = 0x1.78b56362cef38p-2;
constexpr int kWide = 2 * kLibmSteps;
constexpr Interval kAll = Interval::entire();

}

TEST(IntervalTest, Domains)
{
    EXPECT_TRUE(holds(sqrt({-1.0, 4.0}), 0.0, 2.0));
    EXPECT_TRUE(within(sqrt({-1.0, 4.0}), 0.0, shifted(2.0, 1)));
    EXPECT_TRUE(exact(log({0.0, 1.0}), -kInfinity, 0.0));
    EXPECT_TRUE(exact(log({1.0, kInfinity}), 0.0, kInfinity));
    EXPECT_TRUE(holds(asin({0.5, 3.0}), std::asin(0.5), kHalfPiUp));
    EXPECT_TRUE(within(asin({0.5, 3.0}), widen_down(std::asin(0.5)), kHalfPiUp));
    EXPECT_TRUE(exact(asin({-3.0, 3.0}), -kHalfPiUp, kHalfPiUp));
    EXPECT_TRUE(holds(acos({-3.0, -0.5}), std::acos(-0.5), kPiUp));
    EXPECT_TRUE(within(acos({-3.0, -0.5}), widen_down(std::acos(-0.5)), kPiUp));
    EXPECT_TRUE(exact(acos({1.0, 2.0}), 0.0, 0.0));
    EXPECT_TRUE(exact(acosh({0.0, 1.0}), 0.0, 0.0));
    EXPECT_TRUE(within(acosh({1.0, 2.0}), 0.0, kInfinity));
    EXPECT_TRUE(holds(atanh({0.5, 1.0}), std::atanh(0.5), kInfinity));
    EXPECT_TRUE(holds(atanh({-1.0, -0.5}), -kInfinity, std::atanh(-0.5)));
    EXPECT_TRUE(exact(atanh({-1.0, 1.0}), -kInfinity, kInfinity));
    EXPECT_TRUE(holds(cosh({-1.0, 2.0}), 1.0, std::cosh(2.0)));
    EXPECT_TRUE(within(cosh({-1.0, 2.0}), 1.0, widen_up(std::cosh(2.0))));
    EXPECT_TRUE(holds(sinh({0.0, 1.0}), 0.0, std::sinh(1.0)));
    EXPECT_TRUE(within(sinh({0.0, 1.0}), 0.0, kInfinity));
    EXPECT_TRUE(exact(tanh({-kInfinity, 0.0}), -1.0, 0.0));
    EXPECT_TRUE(holds(exp({-kInfinity, 0.0}), 0.0, 1.0));
    EXPECT_TRUE(within(exp({-kInfinity, 0.0}), 0.0, widen_up(1.0)));
}

TEST(IntervalTest, Entropy)
{
    EXPECT_TRUE(holds(entropy({0.0, 1.0}), -kInverseE, 0.0));
    EXPECT_TRUE(within(entropy({0.0, 1.0}), shifted(-kInverseE, -kWide), 0.0));
    EXPECT_TRUE(holds(entropy({0.2, 2.0}), -kInverseE, 2 * std::log(2.0)));
    const Real half = 0.5 * std::log(0.5);
    EXPECT_TRUE(holds(entropy({0.5, 2.0}), half, 2 * std::log(2.0)));
    EXPECT_TRUE(within(entropy({0.5, 2.0}), widen_down(half), kInfinity));
    const Real fifth = 0.2 * std::log(0.2);
    EXPECT_TRUE(holds(entropy({0.1, 0.2}), fifth, 0.1 * std::log(0.1)));
    EXPECT_TRUE(within(entropy({0.1, 0.2}), widen_down(fifth), 0.0));
    EXPECT_TRUE(holds(entropy({1.0, 2.0}), 0.0, 2 * std::log(2.0)));
    EXPECT_TRUE(within(entropy({1.0, 2.0}), 0.0, kInfinity));
    EXPECT_TRUE(holds(entropy({-1.0, 0.0}), 0.0, 0.0));
    EXPECT_TRUE(within(entropy({-1.0, 0.0}), widen_down(0.0), 0.0));
}

TEST(IntervalTest, Unbounded)
{
    EXPECT_TRUE(holds(exp({0.0, kInfinity}), 1.0, kInfinity));
    EXPECT_TRUE(holds(sqrt({4.0, kInfinity}), 2.0, kInfinity));
    EXPECT_TRUE(holds(atan(kAll), -std::atan(kInfinity), std::atan(kInfinity)));
    EXPECT_TRUE(within(atan(kAll), -kHalfPiUp, kHalfPiUp));
    EXPECT_TRUE(exact(tanh(kAll), -1.0, 1.0));
    EXPECT_TRUE(exact(erf(kAll), -1.0, 1.0));
    EXPECT_TRUE(exact(cosh({-kInfinity, 0.0}), 1.0, kInfinity));
    EXPECT_TRUE(exact(asinh(kAll), -kInfinity, kInfinity));
    EXPECT_TRUE(exact(entropy({1.0, kInfinity}), 0.0, kInfinity));
    EXPECT_TRUE(holds(sinh({800.0, 900.0}), kLargest, kInfinity));
    EXPECT_GT(sinh({800.0, 900.0}).lower(), kLargest / 2);
    EXPECT_TRUE(holds(exp({1000.0, 1000.0}), kLargest, kInfinity));
    EXPECT_TRUE(holds(sinh({-900.0, -800.0}), -kInfinity, -kLargest));
}

TEST(IntervalTest, Empty)
{
    EXPECT_TRUE(log({-2.0, -1.0}).is_empty());
    EXPECT_TRUE(log({-1.0, 0.0}).is_empty());
    EXPECT_TRUE(sqrt({-2.0, -1.0}).is_empty());
    EXPECT_TRUE(asin({2.0, 3.0}).is_empty());
    EXPECT_TRUE(acos({-3.0, -2.0}).is_empty());
    EXPECT_TRUE(acosh({0.0, 0.5}).is_empty());
    EXPECT_TRUE(atanh({1.0, 2.0}).is_empty());
    EXPECT_TRUE(atanh({-2.0, -1.0}).is_empty());
    EXPECT_TRUE(entropy({-2.0, -1.0}).is_empty());
    EXPECT_TRUE(exp(Interval::empty()).is_empty());
    EXPECT_TRUE(cosh(Interval::empty()).is_empty());
    EXPECT_TRUE(entropy(Interval::empty()).is_empty());
}
