#include "core/numbers.hpp"
#include "expr/graph/points.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Bits;
using biyos::Offset;
using biyos::PiecewiseLinear;
using biyos::Points;
using biyos::Real;
using biyos::segment_of;
using biyos::slope_of;
using biyos::test::refusal;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kNan = std::numeric_limits<Real>::quiet_NaN();
constexpr Real kSmallest = std::numeric_limits<Real>::denorm_min();

std::vector<Bits> bits(std::span<const Real> values)
{
    std::vector<Bits> result;
    for (const Real value : values)
    {
        result.push_back(std::bit_cast<Bits>(value));
    }
    return result;
}

void expect_stored(const Points &points,
                   Offset pwl,
                   std::span<const Real> x,
                   std::span<const Real> y)
{
    const PiecewiseLinear stored = points.piecewise_linear(pwl);
    EXPECT_EQ(bits(stored.x), bits(x));
    EXPECT_EQ(bits(stored.y), bits(y));
}

std::string refused(std::span<const Real> x, std::span<const Real> y)
{
    Points points;
    const std::string reason = refusal([&] { points.add(x, y); });
    EXPECT_EQ(points.size(), 0);
    return reason;
}

TEST(PointsTest, Stored)
{
    Points points;
    EXPECT_EQ(points.size(), 0);
    const std::array x{-0.0, kSmallest, 3.0};
    const std::array y{0.0, -0.0, 1e300};
    const std::array u{-1.0, 1.0};
    const std::array v{2.0, 2.0};
    EXPECT_EQ(points.add(x, y), 0);
    EXPECT_EQ(points.add(u, v), 1);
    EXPECT_EQ(points.add(x, y), 2);
    EXPECT_EQ(points.size(), 3);
    expect_stored(points, 0, x, y);
    expect_stored(points, 1, u, v);
    expect_stored(points, 2, x, y);
}

TEST(PointsTest, Counts)
{
    const std::array one{1.0};
    const std::array two{1.0, 2.0};
    EXPECT_EQ(refused(two, one), "invalid argument: expression: 2 x values and 1 y value");
    EXPECT_EQ(refused(one, two), "invalid argument: expression: 1 x value and 2 y values");
    EXPECT_EQ(refused(one, one),
              "invalid argument: expression: a pwl takes 2 points or more, not 1");
    EXPECT_EQ(refused({}, {}), "invalid argument: expression: a pwl takes 2 points or more, not 0");
}

TEST(PointsTest, Nonfinite)
{
    const std::array x{0.0, 1.0, 2.0};
    const std::array y{0.0, 1.0, 2.0};
    const std::array nan_x{0.0, kNan, 2.0};
    const std::array high_x{0.0, 1.0, kInfinity};
    const std::array low_x{-kInfinity, 1.0, 2.0};
    const std::array nan_y{kNan, 1.0, 2.0};
    const std::array high_y{0.0, 1.0, kInfinity};
    const std::array low_y{0.0, -kInfinity, 2.0};
    EXPECT_EQ(refused(nan_x, y), "invalid argument: expression: x 1 is NaN");
    EXPECT_EQ(refused(high_x, y), "invalid argument: expression: x 2 is infinity");
    EXPECT_EQ(refused(low_x, y), "invalid argument: expression: x 0 is minus infinity");
    EXPECT_EQ(refused(x, nan_y), "invalid argument: expression: y 0 is NaN");
    EXPECT_EQ(refused(x, high_y), "invalid argument: expression: y 2 is infinity");
    EXPECT_EQ(refused(x, low_y), "invalid argument: expression: y 1 is minus infinity");
}

TEST(PointsTest, Increasing)
{
    const std::array y{0.0, 1.0, 2.0};
    const std::array equal{0.0, 1.0, 1.0};
    const std::array falling{0.0, 2.0, 1.0};
    const std::array zeros{-0.0, 0.0, 1.0};
    const std::array start{1.0, 0.0, 2.0};
    EXPECT_EQ(refused(equal, y), "invalid argument: expression: x 2 is not above x 1");
    EXPECT_EQ(refused(falling, y), "invalid argument: expression: x 2 is not above x 1");
    EXPECT_EQ(refused(zeros, y), "invalid argument: expression: x 1 is not above x 0");
    EXPECT_EQ(refused(start, y), "invalid argument: expression: x 1 is not above x 0");
}

TEST(PointsTest, Overflow)
{
    const std::array wide{-1e308, 1e308};
    const std::array unit{0.0, 1.0};
    const std::array tall{-1e308, 1e308};
    const std::array narrow{0.0, 1e-310};
    const std::array broad{-8e307, 8e307};
    const std::array falling{1e308, -1e308};
    EXPECT_EQ(refused(wide, unit),
              "invalid argument: expression: the pwl's width from point 0 to point 1 is "
              "infinity");
    EXPECT_EQ(refused(wide, tall),
              "invalid argument: expression: the pwl's width from point 0 to point 1 is "
              "infinity");
    EXPECT_EQ(refused(unit, tall),
              "invalid argument: expression: the pwl's height difference from point 0 to "
              "point 1 is infinity");
    EXPECT_EQ(refused(broad, tall),
              "invalid argument: expression: the pwl's height difference from point 0 to "
              "point 1 is infinity");
    EXPECT_EQ(refused(broad, falling),
              "invalid argument: expression: the pwl's height difference from point 0 to "
              "point 1 is minus infinity");
    EXPECT_EQ(refused(narrow, unit),
              "invalid argument: expression: the pwl's slope from point 0 to point 1 is "
              "infinity");
    EXPECT_EQ(refused(narrow, std::array{0.0, -1.0}),
              "invalid argument: expression: the pwl's slope from point 0 to point 1 is "
              "minus infinity");
}

TEST(PointsTest, Unchanged)
{
    Points points;
    const std::array x{0.0, 1.0};
    const std::array y{2.0, 3.0};
    const std::array bad{1.0, 0.0};
    points.add(x, y);
    refusal([&] { points.add(bad, y); });
    EXPECT_EQ(points.size(), 1);
    expect_stored(points, 0, x, y);
    EXPECT_EQ(points.add(y, x), 1);
    expect_stored(points, 1, y, x);
}

TEST(PointsTest, Unknown)
{
    EXPECT_NO_THROW(biyos::check_piecewise_linear(0, 1));
    EXPECT_NO_THROW(biyos::check_piecewise_linear(2, 3));
    EXPECT_EQ(refusal([] { biyos::check_piecewise_linear(1, 1); }),
              "invalid argument: expression: pwl 1 is unknown");
    EXPECT_EQ(refusal([] { biyos::check_piecewise_linear(-1, 4); }),
              "invalid argument: expression: pwl -1 is unknown");
    EXPECT_EQ(refusal([] { biyos::check_piecewise_linear(0, 0); }),
              "invalid argument: expression: pwl 0 is unknown");
}

TEST(PointsTest, Segments)
{
    const std::array x{-1.0, 0.0, 2.0, 5.0};
    EXPECT_EQ(segment_of(x, -kInfinity), 0U);
    EXPECT_EQ(segment_of(x, -7.0), 0U);
    EXPECT_EQ(segment_of(x, -1.0), 0U);
    EXPECT_EQ(segment_of(x, -0.5), 0U);
    EXPECT_EQ(segment_of(x, std::nextafter(0.0, -1.0)), 0U);
    EXPECT_EQ(segment_of(x, -0.0), 1U);
    EXPECT_EQ(segment_of(x, 0.0), 1U);
    EXPECT_EQ(segment_of(x, 1.0), 1U);
    EXPECT_EQ(segment_of(x, std::nextafter(2.0, 0.0)), 1U);
    EXPECT_EQ(segment_of(x, 2.0), 2U);
    EXPECT_EQ(segment_of(x, 4.0), 2U);
    EXPECT_EQ(segment_of(x, 5.0), 2U);
    EXPECT_EQ(segment_of(x, 9.0), 2U);
    EXPECT_EQ(segment_of(x, kInfinity), 2U);
    EXPECT_EQ(segment_of(x, kNan), 2U);
}

TEST(PointsTest, TwoPoints)
{
    const std::array x{1.0, 3.0};
    for (const Real t : {-kInfinity, 0.0, 1.0, 2.0, 3.0, 4.0, kInfinity, kNan})
    {
        EXPECT_EQ(segment_of(x, t), 0U) << t;
    }
}

TEST(PointsTest, Slopes)
{
    const std::array x{-1.0, 0.0, 2.0, 6.0};
    const std::array y{3.0, 1.0, 1.0, 7.0};
    const PiecewiseLinear pwl{.x = x, .y = y};
    EXPECT_DOUBLE_EQ(slope_of(pwl, 0), -2.0);
    EXPECT_DOUBLE_EQ(slope_of(pwl, 1), 0.0);
    EXPECT_DOUBLE_EQ(slope_of(pwl, 2), 1.5);
}

TEST(PointsTest, Truncated)
{
    Points points;
    const std::array x{0.0, 1.0};
    const std::array y{4.0, 5.0};
    const std::array u{-3.0, 0.0, 3.0};
    const std::array v{1.0, 0.0, 1.0};
    const std::array w{2.0, 7.0, -1.0};
    points.add(x, y);
    points.truncate(1);
    EXPECT_EQ(points.size(), 1);
    points.add(u, v);
    points.add(x, y);
    points.truncate(1);
    EXPECT_EQ(points.size(), 1);
    expect_stored(points, 0, x, y);
    EXPECT_EQ(points.add(u, w), 1);
    expect_stored(points, 1, u, w);
    points.truncate(0);
    EXPECT_EQ(points.size(), 0);
    EXPECT_EQ(points.add(x, y), 0);
    expect_stored(points, 0, x, y);
}

}
