#include "bounds.hpp"
#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/analysis/intervals/elementary.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/analysis/intervals/rounding.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace
{

using biyos::Interval;
using biyos::is_zero;
using biyos::kLibmSteps;
using biyos::Real;
using biyos::shifted;
using biyos::widen_down;
using biyos::widen_up;
using biyos::test::exact;
using biyos::test::holds;
using biyos::test::interval_text;
using biyos::test::kHalfPiUp;
using biyos::test::kPiUp;
using biyos::test::within;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kPi = std::numbers::pi;
constexpr Real kGridStep = 0.125;
constexpr Real kGridEnd = 3.0;

constexpr std::array kBounds{-kInfinity, -2.0, -1.0, -0.0, 0.0, 0.5, 1.0, 2.0, kInfinity};
constexpr std::array kExtremes{-1e300, -1e5, -1e-300, 1e-300, 1e5, 1e300};

struct Expected
{
    Interval y;
    Interval x;
    Real lower;
    Real upper;
};

const std::array kExpected{
    Expected{.y = {1.0, 2.0},
             .x = {1.0, 2.0},
             .lower = std::atan2(1.0, 2.0),
             .upper = std::atan2(2.0, 1.0)},
    Expected{.y = {-2.0, -1.0},
             .x = {-1.0, 1.0},
             .lower = std::atan2(-1.0, -1.0),
             .upper = std::atan2(-1.0, 1.0)},
    Expected{.y = {1.0, 2.0},
             .x = {-2.0, -1.0},
             .lower = std::atan2(2.0, -1.0),
             .upper = std::atan2(1.0, -2.0)},
    Expected{.y = {0.0, 1.0}, .x = {-1.0, 0.0}, .lower = 0.0, .upper = kPi},
    Expected{.y = {-1.0, 0.0}, .x = {0.0, 1.0}, .lower = -kPi / 2, .upper = 0.0},
    Expected{.y = {-1.0, 1.0}, .x = {0.0, 1.0}, .lower = -kPi / 2, .upper = kPi / 2},
    Expected{.y = {0.0, 0.0}, .x = {-1.0, 1.0}, .lower = 0.0, .upper = kPi},
    Expected{.y = {-0.0, -0.0}, .x = {-2.0, -1.0}, .lower = kPi, .upper = kPi},
    Expected{.y = {0.0, 0.0}, .x = {0.0, 1.0}, .lower = 0.0, .upper = 0.0},
    Expected{.y = {0.0, 0.0}, .x = {0.0, 0.0}, .lower = 0.0, .upper = 0.0},
    Expected{.y = {0.0, 1.0}, .x = {0.0, 0.0}, .lower = 0.0, .upper = kPi / 2},
    Expected{.y = {1.0, kInfinity}, .x = Interval::entire(), .lower = 0.0, .upper = kPi},
    Expected{.y = {-kInfinity, -1.0}, .x = {1.0, kInfinity}, .lower = -kPi / 2, .upper = 0.0},
    Expected{.y = {-kInfinity, -1.0}, .x = Interval::entire(), .lower = -kPi, .upper = 0.0},
    Expected{.y = {1.0, kInfinity}, .x = {1.0, kInfinity}, .lower = 0.0, .upper = kPi / 2},
};

constexpr std::array kCrossing{
    Interval{-1.0, 1.0},
    Interval{-1.0, 0.0},
    Interval{-kInfinity, -0.0},
    Interval{-1e-300, kInfinity},
};

Real angle(Real y, Real x)
{
    return std::atan2(y + 0.0, x + 0.0);
}

testing::AssertionResult whole_turn(Interval found)
{
    return exact(found, -kPiUp, kPiUp);
}

Real outward(Real value, int steps)
{
    return is_zero(value) ? value : shifted(value, steps);
}

std::vector<Real> grid()
{
    std::vector<Real> points(kBounds.begin(), kBounds.end());
    points.insert(points.end(), kExtremes.begin(), kExtremes.end());
    for (Real point = -kGridEnd; point <= kGridEnd; point += kGridStep)
    {
        points.push_back(point);
    }
    return points;
}

std::vector<Interval> intervals()
{
    std::vector<Interval> found;
    for (const Real lower : kBounds)
    {
        for (const Real upper : kBounds)
        {
            const Interval interval(lower, upper);
            if (!interval.is_empty())
            {
                found.push_back(interval);
            }
        }
    }
    return found;
}

std::vector<Real> inside(Interval interval, const std::vector<Real> &points)
{
    std::vector<Real> found;
    for (const Real point : points)
    {
        if (interval.lower() <= point && point <= interval.upper())
        {
            found.push_back(point);
        }
    }
    return found;
}

void expect_box(Interval y, Interval x, const std::vector<Real> &points)
{
    const Interval found = biyos::atan2(y, x);
    SCOPED_TRACE("y " + interval_text(y) + " x " + interval_text(x));
    Real least = kInfinity;
    Real most = -kInfinity;
    for (const Real at_y : inside(y, points))
    {
        for (const Real at_x : inside(x, points))
        {
            const Real value = angle(at_y, at_x);
            ASSERT_TRUE(holds(found, value, value)) << "at y " << at_y << " x " << at_x;
            least = (std::min)(least, value);
            most = (std::max)(most, value);
        }
    }
    EXPECT_TRUE(within(found, outward(least, -kLibmSteps), outward(most, kLibmSteps)));
}

}

TEST(Atan2Test, Grid)
{
    const std::vector<Real> points = grid();
    for (const Interval y : intervals())
    {
        for (const Interval x : intervals())
        {
            expect_box(y, x, points);
        }
    }
}

TEST(Atan2Test, Corners)
{
    for (const Expected &box : kExpected)
    {
        const Interval found = biyos::atan2(box.y, box.x);
        SCOPED_TRACE("y " + interval_text(box.y) + " x " + interval_text(box.x));
        EXPECT_TRUE(holds(found, box.lower, box.upper));
        const Real lower = outward(box.lower, -kLibmSteps);
        const Real upper = outward(box.upper, kLibmSteps);
        EXPECT_TRUE(within(found, (std::max)(lower, -kPiUp), (std::min)(upper, kPiUp)));
    }
}

TEST(Atan2Test, Whole)
{
    for (const Interval y : kCrossing)
    {
        EXPECT_TRUE(whole_turn(biyos::atan2(y, Interval(-1.0, 2.0)))) << interval_text(y);
    }
    EXPECT_TRUE(whole_turn(biyos::atan2(Interval::entire(), Interval::entire())));
}

TEST(Atan2Test, Point)
{
    const Real value = std::atan2(1.0, 2.0);
    const Interval found = biyos::atan2(Interval(1.0, 1.0), Interval(2.0, 2.0));
    EXPECT_TRUE(exact(found, widen_down(value), widen_up(value)));
}

TEST(Atan2Test, Turns)
{
    const Interval right = biyos::atan2(Interval(-1.0, 1.0), Interval(0.0, 1e-300));
    EXPECT_TRUE(exact(right, -kHalfPiUp, kHalfPiUp));
    const Interval upper = biyos::atan2(Interval(0.0, 1e-300), Interval(-1.0, 1.0));
    EXPECT_TRUE(exact(upper, 0.0, kPiUp));
}

TEST(Atan2Test, Empty)
{
    const Interval held(1.0, 2.0);
    EXPECT_TRUE(biyos::atan2(Interval::empty(), held).is_empty());
    EXPECT_TRUE(biyos::atan2(held, Interval::empty()).is_empty());
    EXPECT_TRUE(biyos::atan2(Interval::empty(), Interval::empty()).is_empty());
}
