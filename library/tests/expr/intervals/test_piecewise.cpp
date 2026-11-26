#include "bounds.hpp"
#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/graph/points.hpp"
#include "expr/intervals/interval.hpp"
#include "expr/intervals/piecewise.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using biyos::Interval;
using biyos::is_zero;
using biyos::piecewise;
using biyos::PiecewiseLinear;
using biyos::Real;
using biyos::value_at;
using biyos::test::exact;
using biyos::test::holds;
using biyos::test::interval_text;
using biyos::test::within;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kSlack = 1e-14;
constexpr Real kCoarse = 1e-12;
constexpr Real kRounding = 4 * std::numeric_limits<Real>::epsilon();
constexpr std::uint64_t kSeed = 20261124;
constexpr int kLines = 400;
constexpr int kBoxes = 12;
constexpr int kSamples = 200;
constexpr int kReported = 5;

struct Line
{
    std::vector<Real> x;
    std::vector<Real> y;

    PiecewiseLinear pwl() const { return {.x = x, .y = y}; }
};

const Line kZigzag{.x = {-2.0, -0.5, 1.0, 3.0}, .y = {1.0, -1.0, 0.5, 2.0}};
const Line kPlateau{.x = {0.0, 1.0, 2.0, 3.0}, .y = {2.0, 2.0, 5.0, 5.0}};
const Line kRise{.x = {0.0, 1.0}, .y = {0.0, 2.0}};

Interval enclose(const Line &line, Real lower, Real upper)
{
    return piecewise(line.pwl(), {lower, upper});
}

testing::AssertionResult near(Interval found, Real lower, Real upper, Real slack = kSlack)
{
    const testing::AssertionResult held = holds(found, lower, upper);
    return held ? within(found, lower - slack, upper + slack) : held;
}

std::size_t segment_of(const Line &line, Real t)
{
    std::size_t k = 0;
    while (k + 2 < line.x.size() && !(t < line.x[k + 1]))
    {
        ++k;
    }
    return k;
}

struct Bounds
{
    Real lower;
    Real upper;
};

Real climb_of(const Line &line, std::size_t k, Real from, Real t)
{
    const Real rise = line.y[k + 1] - line.y[k];
    const Real run = line.x[k + 1] - line.x[k];
    const Real gap = t - from;
    const Real share = std::isinf(gap) ? t / run - from / run : gap / run;
    return share * rise;
}

Bounds real_at(const Line &line, Real t)
{
    const std::size_t k = segment_of(line, t);
    const std::size_t anchor = t < line.x[k + 1] ? k : k + 1;
    const Real start = line.y[anchor];
    const Real climb = climb_of(line, k, line.x[anchor], t);
    const Real value = start + climb;
    const Real error = kRounding * (std::fabs(start) + std::fabs(climb));
    return {.lower = value - error, .upper = value + error};
}

Bounds point(Real value)
{
    return {.lower = value, .upper = value};
}

Bounds limit_at(const Line &line, Real t)
{
    if (std::isfinite(t))
    {
        return real_at(line, t);
    }
    const std::size_t k = t > 0.0 ? line.x.size() - 2 : 0;
    const Real rise = line.y[k + 1] - line.y[k];
    if (is_zero(rise))
    {
        return point(line.y[k]);
    }
    return point((rise > 0.0) == (t > 0.0) ? kInfinity : -kInfinity);
}

struct Truth
{
    Bounds lowest;
    Bounds highest;
};

void take(Truth &truth, Bounds value)
{
    truth.lowest.lower = (std::min)(truth.lowest.lower, value.lower);
    truth.lowest.upper = (std::min)(truth.lowest.upper, value.upper);
    truth.highest.lower = (std::max)(truth.highest.lower, value.lower);
    truth.highest.upper = (std::max)(truth.highest.upper, value.upper);
}

Truth truth_of(const Line &line, Interval box)
{
    Truth truth{.lowest = limit_at(line, box.lower()), .highest = limit_at(line, box.lower())};
    take(truth, limit_at(line, box.upper()));
    for (std::size_t k = 0; k < line.x.size(); ++k)
    {
        if (box.lower() < line.x[k] && line.x[k] < box.upper())
        {
            take(truth, point(line.y[k]));
        }
    }
    return truth;
}

Real reach_of(const Line &line, std::size_t k, Real t)
{
    return std::isfinite(t) ? std::fabs(climb_of(line, k, line.x[k], t)) : 0.0;
}

Real scale_of(const Line &line, Interval box)
{
    Real scale = 1.0;
    for (std::size_t k = 0; k < line.x.size(); ++k)
    {
        scale = (std::max)(scale, std::fabs(line.y[k]));
    }
    for (std::size_t k = 0; k + 1 < line.x.size(); ++k)
    {
        scale = (std::max)(scale, reach_of(line, k, box.lower()));
        scale = (std::max)(scale, reach_of(line, k, box.upper()));
    }
    return scale;
}

std::vector<Real> samples_of(const Line &line, Interval box, std::mt19937_64 &engine)
{
    const Real from = std::isfinite(box.lower()) ? box.lower() : line.x.front() - 10.0;
    const Real to = std::isfinite(box.upper()) ? box.upper() : line.x.back() + 10.0;
    std::vector<Real> found{from, to};
    std::uniform_real_distribution<Real> draw(0.0, 1.0);
    for (int k = 0; k < kSamples; ++k)
    {
        found.push_back(from + (to - from) * draw(engine));
    }
    for (const Real x : line.x)
    {
        found.push_back(x);
        found.push_back(std::nextafter(x, -kInfinity));
        found.push_back(std::nextafter(x, kInfinity));
    }
    std::erase_if(found, [box](Real t) { return !(box.lower() <= t && t <= box.upper()); });
    return found;
}

testing::AssertionResult covers(Interval found, Bounds value)
{
    if (!found.is_empty() && found.lower() <= value.upper && value.lower <= found.upper())
    {
        return testing::AssertionSuccess();
    }
    std::ostringstream out;
    out.precision(17);
    out << interval_text(found) << " misses [" << value.lower << ", " << value.upper << "]";
    return testing::AssertionFailure() << out.str();
}

void expect_sampled(const Line &line, Interval box, std::mt19937_64 &engine, int &reported)
{
    const Interval found = piecewise(line.pwl(), box);
    for (const Real t : samples_of(line, box, engine))
    {
        const testing::AssertionResult real = covers(found, real_at(line, t));
        const testing::AssertionResult computed = covers(found, point(value_at(line.pwl(), t)));
        if ((!real || !computed) && reported++ < kReported)
        {
            ADD_FAILURE() << "box " << interval_text(box) << " at " << t << ": "
                          << (real ? computed.message() : real.message());
        }
    }
    const Truth truth = truth_of(line, box);
    const Real slack = 64 * std::numeric_limits<Real>::epsilon() * scale_of(line, box);
    const Bounds low = truth.lowest;
    const Bounds high = truth.highest;
    const bool lower = found.lower() >= low.lower - slack && found.lower() <= low.upper;
    const bool upper = found.upper() <= high.upper + slack && found.upper() >= high.lower;
    if ((!lower || !upper) && reported++ < kReported)
    {
        ADD_FAILURE() << "box " << interval_text(box) << " gives " << interval_text(found)
                      << " for [" << low.lower << ", " << high.upper << "]";
    }
}

Real coordinate(std::mt19937_64 &engine, Real scale)
{
    std::uniform_real_distribution<Real> draw(-1.0, 1.0);
    return scale * draw(engine);
}

Line random_line(std::mt19937_64 &engine)
{
    const std::size_t n = 2 + engine() % 7;
    const Real width = std::pow(10.0, static_cast<Real>(engine() % 7) - 3.0);
    const Real height = std::pow(10.0, static_cast<Real>(engine() % 7) - 3.0);
    Line line;
    Real x = coordinate(engine, width);
    for (std::size_t k = 0; k < n; ++k)
    {
        line.x.push_back(x);
        x += width * (0.01 + std::fabs(coordinate(engine, 1.0)));
        const bool repeat = k > 0 && engine() % 4 == 0;
        line.y.push_back(repeat ? line.y.back() : coordinate(engine, height));
    }
    return line;
}

Real end_of(const Line &line, std::mt19937_64 &engine)
{
    switch (engine() % 6)
    {
    case 0:
        return line.x[engine() % line.x.size()];
    case 1:
        return engine() % 2 == 0 ? -kInfinity : kInfinity;
    default:
    {
        const Real spread = line.x.back() - line.x.front();
        return line.x.front() + spread * (1.5 * coordinate(engine, 1.0) + 0.5);
    }
    }
}

Interval random_box(const Line &line, std::mt19937_64 &engine)
{
    const Real first = end_of(line, engine);
    const Real last = engine() % 8 == 0 ? first : end_of(line, engine);
    const Interval box((std::min)(first, last), (std::max)(first, last));
    return box.is_empty() ? Interval(line.x.front(), line.x.back()) : box;
}

}

TEST(EnclosureTest, Piecewise)
{
    EXPECT_TRUE(near(enclose(kZigzag, 0.0, 0.5), -0.5, 0.0));
    EXPECT_TRUE(near(enclose(kZigzag, -1.0, 2.0), -1.0, 1.25));
    EXPECT_TRUE(near(enclose(kZigzag, -4.0, 5.0), -1.0, 11.0 / 3));
    EXPECT_TRUE(near(enclose(kZigzag, -2.5, -0.5), -1.0, 5.0 / 3));
    EXPECT_TRUE(near(enclose(kZigzag, 3.0, 4.0), 2.0, 2.75));
    EXPECT_TRUE(near(enclose(kZigzag, -kInfinity, 0.0), -1.0, kInfinity));
    EXPECT_TRUE(near(enclose(kZigzag, 0.0, kInfinity), -0.5, kInfinity));
    EXPECT_TRUE(near(enclose(kZigzag, -kInfinity, kInfinity), -1.0, kInfinity));
    EXPECT_TRUE(near(enclose(kRise, -kInfinity, 0.5), -kInfinity, 1.0));
    EXPECT_TRUE(exact(enclose(kRise, -kInfinity, kInfinity), -kInfinity, kInfinity));
    EXPECT_TRUE(near(enclose(kRise, 0.25, kInfinity), 0.5, kInfinity));
}

TEST(EnclosureTest, Plateau)
{
    EXPECT_TRUE(near(enclose(kPlateau, -kInfinity, kInfinity), 2.0, 5.0));
    EXPECT_TRUE(exact(enclose(kPlateau, -kInfinity, 1.0), 2.0, 2.0));
    EXPECT_TRUE(exact(enclose(kPlateau, 2.0, kInfinity), 5.0, 5.0));
    EXPECT_TRUE(exact(enclose(kPlateau, -kInfinity, 0.5), 2.0, 2.0));
    EXPECT_TRUE(near(enclose(kPlateau, 0.5, 1.5), 2.0, 3.5));
}

TEST(EnclosureTest, Breakpoint)
{
    for (std::size_t k = 0; k < kZigzag.x.size(); ++k)
    {
        const Real x = kZigzag.x[k];
        EXPECT_TRUE(exact(enclose(kZigzag, x, x), kZigzag.y[k], kZigzag.y[k])) << k;
    }
    EXPECT_TRUE(near(enclose(kZigzag, -5.0, -5.0), 5.0, 5.0));
    EXPECT_TRUE(near(enclose(kZigzag, 7.0, 7.0), 5.0, 5.0));
    EXPECT_TRUE(enclose(kZigzag, kInfinity, -kInfinity).is_empty());
    EXPECT_TRUE(piecewise(kZigzag.pwl(), Interval::empty()).is_empty());
}

TEST(EnclosureTest, Overflow)
{
    const Line far{.x = {-1.5e308, -1e308}, .y = {0.0, 10.0}};
    EXPECT_TRUE(near(enclose(far, 1e308, 1.5e308), 50.0, 60.0, kCoarse));
    EXPECT_TRUE(near(enclose(far, -1e308, 1e308), 10.0, 50.0, kCoarse));
    EXPECT_TRUE(near(enclose(far, -1e308, 0.0), 10.0, 30.0, kCoarse));
    const Line back{.x = {1e308, 1.5e308}, .y = {0.0, 10.0}};
    EXPECT_TRUE(near(enclose(back, -1.5e308, -1e308), -50.0, -40.0, kCoarse));
    EXPECT_TRUE(near(enclose(back, -1e308, 0.0), -40.0, -20.0, kCoarse));
}

TEST(EnclosureTest, Distant)
{
    std::mt19937_64 engine(kSeed);
    int reported = 0;
    const Line far{.x = {-1.5e308, -1e308, -5e307}, .y = {0.0, 10.0, -5.0}};
    for (const Real end : {1e308, 1.5e308, (std::numeric_limits<Real>::max)()})
    {
        expect_sampled(far, {-1.2e308, end}, engine, reported);
        expect_sampled(far, {1e307, end}, engine, reported);
    }
}

TEST(EnclosureTest, Steep)
{
    const Line steep{.x = {1.0, std::nextafter(1.0, kInfinity)}, .y = {0.0, 1e300}};
    EXPECT_TRUE(exact(enclose(steep, steep.x[0], steep.x[1]), 0.0, 1e300));
    const Line left{.x = {std::nextafter(-1.0, -kInfinity), -1.0}, .y = {0.0, 1e300}};
    EXPECT_TRUE(exact(enclose(left, left.x[0], left.x[1]), 0.0, 1e300));
    const Line cliff{.x = {0.0, 1.0, std::nextafter(1.0, kInfinity), 2.0},
                     .y = {0.0, 0.0, 1e300, 1e300}};
    EXPECT_TRUE(exact(enclose(cliff, 0.5, cliff.x[2]), 0.0, 1e300));
    EXPECT_TRUE(exact(enclose(cliff, cliff.x[1], cliff.x[1]), 0.0, 0.0));
}

TEST(EnclosureTest, Sampled)
{
    std::mt19937_64 engine(kSeed);
    int reported = 0;
    for (int round = 0; round < kLines; ++round)
    {
        const Line line = random_line(engine);
        for (int k = 0; k < kBoxes; ++k)
        {
            expect_sampled(line, random_box(line, engine), engine, reported);
        }
    }
}
