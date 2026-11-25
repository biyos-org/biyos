#include "bounds.hpp"
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
using biyos::Pwl;
using biyos::test::exact;
using biyos::test::holds;
using biyos::test::interval_text;
using biyos::test::within;

using Wide = long double;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kSlack = 1e-14;
constexpr double kCoarse = 1e-12;
constexpr std::uint64_t kSeed = 20261124;
constexpr int kLines = 400;
constexpr int kBoxes = 12;
constexpr int kSamples = 200;
constexpr int kReported = 5;

struct Line
{
    std::vector<double> x;
    std::vector<double> y;

    Pwl pwl() const { return {.x = x, .y = y}; }
};

const Line kZigzag{.x = {-2.0, -0.5, 1.0, 3.0}, .y = {1.0, -1.0, 0.5, 2.0}};
const Line kPlateau{.x = {0.0, 1.0, 2.0, 3.0}, .y = {2.0, 2.0, 5.0, 5.0}};
const Line kRise{.x = {0.0, 1.0}, .y = {0.0, 2.0}};

Interval enclose(const Line &line, double lower, double upper)
{
    return biyos::piecewise(line.pwl(), {lower, upper});
}

testing::AssertionResult near(Interval found, double lower, double upper, double slack = kSlack)
{
    const testing::AssertionResult held = holds(found, lower, upper);
    return held ? within(found, lower - slack, upper + slack) : held;
}

std::size_t segment_of(const Line &line, double t)
{
    std::size_t k = 0;
    while (k + 2 < line.x.size() && !(t < line.x[k + 1]))
    {
        ++k;
    }
    return k;
}

Wide real_at(const Line &line, double t)
{
    const std::size_t k = segment_of(line, t);
    const std::size_t anchor = t < line.x[k + 1] ? k : k + 1;
    const Wide rise = static_cast<Wide>(line.y[k + 1]) - line.y[k];
    const Wide run = static_cast<Wide>(line.x[k + 1]) - line.x[k];
    return line.y[anchor] + (static_cast<Wide>(t) - line.x[anchor]) * rise / run;
}

Wide limit_at(const Line &line, double t)
{
    if (std::isfinite(t))
    {
        return real_at(line, t);
    }
    const std::size_t k = t > 0.0 ? line.x.size() - 2 : 0;
    const double rise = line.y[k + 1] - line.y[k];
    if (biyos::is_zero(rise))
    {
        return line.y[k];
    }
    return (rise > 0.0) == (t > 0.0) ? kInfinity : -kInfinity;
}

struct Truth
{
    Wide lower;
    Wide upper;
};

void take(Truth &truth, Wide value)
{
    truth.lower = (std::min)(truth.lower, value);
    truth.upper = (std::max)(truth.upper, value);
}

Truth truth_of(const Line &line, Interval box)
{
    Truth truth{.lower = limit_at(line, box.lower()), .upper = limit_at(line, box.lower())};
    take(truth, limit_at(line, box.upper()));
    for (std::size_t k = 0; k < line.x.size(); ++k)
    {
        if (box.lower() < line.x[k] && line.x[k] < box.upper())
        {
            take(truth, line.y[k]);
        }
    }
    return truth;
}

Wide reach_of(double t, double x)
{
    return std::isfinite(t) ? std::fabs(static_cast<Wide>(t) - x) : 0.0L;
}

Wide scale_of(const Line &line, Interval box)
{
    Wide scale = 1.0;
    for (std::size_t k = 0; k < line.x.size(); ++k)
    {
        scale = (std::max)(scale, std::fabs(static_cast<Wide>(line.y[k])));
    }
    for (std::size_t k = 0; k + 1 < line.x.size(); ++k)
    {
        const Wide rise = static_cast<Wide>(line.y[k + 1]) - line.y[k];
        const Wide slope = std::fabs(rise / (static_cast<Wide>(line.x[k + 1]) - line.x[k]));
        scale = (std::max)(scale, slope * reach_of(box.lower(), line.x[k]));
        scale = (std::max)(scale, slope * reach_of(box.upper(), line.x[k]));
    }
    return scale;
}

std::vector<double> samples_of(const Line &line, Interval box, std::mt19937_64 &engine)
{
    const double from = std::isfinite(box.lower()) ? box.lower() : line.x.front() - 10.0;
    const double to = std::isfinite(box.upper()) ? box.upper() : line.x.back() + 10.0;
    std::vector<double> found{from, to};
    std::uniform_real_distribution<double> draw(0.0, 1.0);
    for (int k = 0; k < kSamples; ++k)
    {
        found.push_back(from + (to - from) * draw(engine));
    }
    for (const double x : line.x)
    {
        found.push_back(x);
        found.push_back(std::nextafter(x, -kInfinity));
        found.push_back(std::nextafter(x, kInfinity));
    }
    std::erase_if(found, [box](double t) { return !(box.lower() <= t && t <= box.upper()); });
    return found;
}

testing::AssertionResult covers(Interval found, Wide value)
{
    if (!found.is_empty() && found.lower() <= value && value <= found.upper())
    {
        return testing::AssertionSuccess();
    }
    std::ostringstream out;
    out.precision(21);
    out << interval_text(found) << " misses " << value;
    return testing::AssertionFailure() << out.str();
}

void expect_sampled(const Line &line, Interval box, std::mt19937_64 &engine, int &reported)
{
    const Interval found = biyos::piecewise(line.pwl(), box);
    for (const double t : samples_of(line, box, engine))
    {
        const testing::AssertionResult real = covers(found, real_at(line, t));
        const testing::AssertionResult computed = covers(found, biyos::value_at(line.pwl(), t));
        if ((!real || !computed) && reported++ < kReported)
        {
            ADD_FAILURE() << "box " << interval_text(box) << " at " << t << ": "
                          << (real ? computed.message() : real.message());
        }
    }
    const Truth truth = truth_of(line, box);
    const Wide slack = 64 * std::numeric_limits<double>::epsilon() * scale_of(line, box);
    const bool lower = found.lower() >= truth.lower - slack && found.lower() <= truth.lower;
    const bool upper = found.upper() <= truth.upper + slack && found.upper() >= truth.upper;
    if ((!lower || !upper) && reported++ < kReported)
    {
        ADD_FAILURE() << "box " << interval_text(box) << " gives " << interval_text(found)
                      << " for [" << static_cast<double>(truth.lower) << ", "
                      << static_cast<double>(truth.upper) << "]";
    }
}

double coordinate(std::mt19937_64 &engine, double scale)
{
    std::uniform_real_distribution<double> draw(-1.0, 1.0);
    return scale * draw(engine);
}

Line random_line(std::mt19937_64 &engine)
{
    const std::size_t n = 2 + engine() % 7;
    const double width = std::pow(10.0, static_cast<double>(engine() % 7) - 3.0);
    const double height = std::pow(10.0, static_cast<double>(engine() % 7) - 3.0);
    Line line;
    double x = coordinate(engine, width);
    for (std::size_t k = 0; k < n; ++k)
    {
        line.x.push_back(x);
        x += width * (0.01 + std::fabs(coordinate(engine, 1.0)));
        const bool repeat = k > 0 && engine() % 4 == 0;
        line.y.push_back(repeat ? line.y.back() : coordinate(engine, height));
    }
    return line;
}

double end_of(const Line &line, std::mt19937_64 &engine)
{
    switch (engine() % 6)
    {
    case 0:
        return line.x[engine() % line.x.size()];
    case 1:
        return engine() % 2 == 0 ? -kInfinity : kInfinity;
    default:
    {
        const double spread = line.x.back() - line.x.front();
        return line.x.front() + spread * (1.5 * coordinate(engine, 1.0) + 0.5);
    }
    }
}

Interval random_box(const Line &line, std::mt19937_64 &engine)
{
    const double first = end_of(line, engine);
    const double last = engine() % 8 == 0 ? first : end_of(line, engine);
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
        const double x = kZigzag.x[k];
        EXPECT_TRUE(exact(enclose(kZigzag, x, x), kZigzag.y[k], kZigzag.y[k])) << k;
    }
    EXPECT_TRUE(near(enclose(kZigzag, -5.0, -5.0), 5.0, 5.0));
    EXPECT_TRUE(near(enclose(kZigzag, 7.0, 7.0), 5.0, 5.0));
    EXPECT_TRUE(enclose(kZigzag, kInfinity, -kInfinity).is_empty());
    EXPECT_TRUE(biyos::piecewise(kZigzag.pwl(), Interval::empty()).is_empty());
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
    for (const double end : {1e308, 1.5e308, (std::numeric_limits<double>::max)()})
    {
        expect_sampled(far, {-1.2e308, end}, engine, reported);
        expect_sampled(far, {1e307, end}, engine, reported);
    }
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
