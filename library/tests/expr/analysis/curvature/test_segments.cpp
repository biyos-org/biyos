#include "core/numbers.hpp"
#include "expr/analysis/curvature/curvature.hpp"
#include "expr/analysis/curvature/segments.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/enclosure.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using biyos::Curvature;
using biyos::enclose;
using biyos::falls;
using biyos::Graph;
using biyos::Interval;
using biyos::is_concave;
using biyos::is_convex;
using biyos::Monotonicity;
using biyos::Offset;
using biyos::Operator;
using biyos::PiecewiseLinear;
using biyos::Profile;
using biyos::Real;
using biyos::rises;
using biyos::segments;
using biyos::Shapes;
using biyos::test::default_tolerances;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr std::uint64_t kSeed = 20261124;
constexpr int kLines = 400;
constexpr int kBoxes = 12;
constexpr int kReported = 5;
constexpr Real kRounding = 4 * std::numeric_limits<Real>::epsilon();

constexpr Curvature kAffine = Curvature::Affine;
constexpr Curvature kConvex = Curvature::Convex;
constexpr Curvature kConcave = Curvature::Concave;
constexpr Curvature kNeither = Curvature::Unknown;
constexpr Monotonicity kRising = Monotonicity::Nondecreasing;
constexpr Monotonicity kFalling = Monotonicity::Nonincreasing;
constexpr Monotonicity kConstant = Monotonicity::Constant;
constexpr Monotonicity kEither = Monotonicity::Unknown;

struct Line
{
    std::vector<Real> x;
    std::vector<Real> y;

    PiecewiseLinear pwl() const { return {.x = x, .y = y}; }
};

const Line kValley{.x = {-1.0, 0.0, 1.0}, .y = {1.0, 0.0, 1.0}};
const Line kRidge{.x = {-1.0, 0.0, 2.0}, .y = {-1.0, 0.0, 1.0}};
const Line kWave{.x = {-2.0, -1.0, 0.0, 1.0, 2.0}, .y = {2.0, 0.0, 1.0, 0.0, 2.0}};
const Line kSteps{.x = {0.0, 1.0, 2.0, 3.0}, .y = {1.0, 1.0, 1.0, 1.0}};
const Line kNearly{.x = {0.0, 0.1, 0.3}, .y = {0.0, 0.7, 2.0999999999999996}};
const Line kStraight{.x = {0.0, 1.0, 2.0}, .y = {0.0, 1.0, 2.0}};
const Line kLevel{.x = {-1.0, 0.0, 1.0}, .y = {-2.0, -1.0, 0.0}};

Profile over(const Line &line, Real lower, Real upper)
{
    return segments(line.pwl(), {lower, upper});
}

Curvature curvature(const Line &line, Real lower, Real upper)
{
    return over(line, lower, upper).curvature;
}

Monotonicity monotonicity(const Line &line, Real lower, Real upper)
{
    return over(line, lower, upper).argument.monotonicity;
}

Curvature nested(const Line &line, Operator op, Real sign, Interval x)
{
    Graph graph;
    const Offset variable = graph.add_variable(0);
    const Offset outer = graph.add(op, std::span(&variable, 1));
    const std::array weight{sign};
    const Offset child = graph.add_sum(0.0, std::span(&outer, 1), weight);
    const Offset pwl = graph.add_piecewise_linear(line.x, line.y);
    const Offset root = graph.add_piecewise(pwl, std::span(&child, 1));
    std::vector<Interval> nodes(static_cast<std::size_t>(graph.size()), Interval::empty());
    enclose(graph, std::span(&x, 1), default_tolerances(), nodes);
    Shapes shapes(graph);
    shapes.classify(graph, nodes);
    return shapes.curvature(root);
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

Real slope_of(const Line &line, std::size_t k)
{
    return (line.y[k + 1] - line.y[k]) / (line.x[k + 1] - line.x[k]);
}

std::vector<Real> slopes_of(const Line &line, Interval box)
{
    std::vector<Real> slopes;
    std::size_t k = segment_of(line, box.lower());
    if (box.lower() < box.upper())
    {
        slopes.push_back(slope_of(line, k));
    }
    while (k + 2 < line.x.size() && line.x[k + 1] < box.upper())
    {
        slopes.push_back(slope_of(line, ++k));
    }
    return slopes;
}

bool ordered(std::span<const Real> slopes, Real sign)
{
    for (std::size_t k = 0; k + 1 < slopes.size(); ++k)
    {
        const Real step = sign * (slopes[k + 1] - slopes[k]);
        if (step < -kRounding * (std::fabs(slopes[k]) + std::fabs(slopes[k + 1])))
        {
            return false;
        }
    }
    return true;
}

bool signed_as(std::span<const Real> slopes, Real sign)
{
    return std::ranges::all_of(slopes, [sign](Real s) { return sign * s >= 0.0; });
}

bool holds(const Profile &profile, std::span<const Real> slopes)
{
    const bool convex = !is_convex(profile.curvature) || ordered(slopes, 1);
    const bool concave = !is_concave(profile.curvature) || ordered(slopes, -1);
    const bool rising = !rises(profile.argument.monotonicity) || signed_as(slopes, 1);
    const bool falling = !falls(profile.argument.monotonicity) || signed_as(slopes, -1);
    return convex && concave && rising && falling;
}

std::string text_of(const Line &line)
{
    std::ostringstream out;
    out.precision(17);
    for (std::size_t k = 0; k < line.x.size(); ++k)
    {
        out << "(" << line.x[k] << ", " << line.y[k] << ")";
    }
    return out.str();
}

Line random_line(std::mt19937_64 &engine)
{
    const std::size_t n = 2 + engine() % 6;
    std::uniform_real_distribution<Real> draw(-1.0, 1.0);
    const bool convex = engine() % 3 == 0;
    Line line;
    Real x = draw(engine);
    Real y = draw(engine);
    Real slope = draw(engine);
    for (std::size_t k = 0; k < n; ++k)
    {
        line.x.push_back(x);
        line.y.push_back(y);
        const Real width = 0.05 + std::fabs(draw(engine));
        x += width;
        slope = convex ? slope + std::fabs(draw(engine)) : draw(engine);
        y = engine() % 5 == 0 ? y : y + slope * width;
    }
    return line;
}

Real end_of(const Line &line, std::mt19937_64 &engine)
{
    switch (engine() % 5)
    {
    case 0:
        return line.x[engine() % line.x.size()];
    case 1:
        return engine() % 2 == 0 ? -kInfinity : kInfinity;
    default:
    {
        std::uniform_real_distribution<Real> draw(-0.5, 1.5);
        return line.x.front() + (line.x.back() - line.x.front()) * draw(engine);
    }
    }
}

Interval random_box(const Line &line, std::mt19937_64 &engine)
{
    const Real first = end_of(line, engine);
    const Real last = end_of(line, engine);
    const Interval box((std::min)(first, last), (std::max)(first, last));
    return box.is_empty() ? Interval(line.x.front(), line.x.back()) : box;
}

}

TEST(CurvatureTest, Piecewise)
{
    EXPECT_EQ(curvature(kValley, -kInfinity, kInfinity), kConvex);
    EXPECT_EQ(curvature(kRidge, -kInfinity, kInfinity), kConcave);
    EXPECT_EQ(curvature(kWave, -kInfinity, kInfinity), kNeither);
    EXPECT_EQ(curvature(kWave, 0.0, 2.0), kConvex);
    EXPECT_EQ(curvature(kWave, -0.5, 2.0), kNeither);
    EXPECT_EQ(curvature(kWave, -1.0, 1.0), kConcave);
    EXPECT_EQ(curvature(kWave, -1.0, 0.0), kAffine);
    EXPECT_EQ(curvature(kWave, -0.75, -0.25), kAffine);
    EXPECT_EQ(curvature(kWave, -kInfinity, -1.0), kAffine);
    EXPECT_EQ(curvature(kWave, 1.0, kInfinity), kAffine);
    EXPECT_EQ(curvature(kWave, 0.5, 0.5), kAffine);
    EXPECT_EQ(curvature(kWave, 1.0, 1.0), kAffine);
    EXPECT_EQ(curvature(kSteps, -kInfinity, kInfinity), kAffine);
    EXPECT_EQ(curvature(kValley, kInfinity, -kInfinity), kNeither);
}

TEST(CurvatureTest, Collinear)
{
    EXPECT_EQ(curvature(kNearly, 0.0, 0.3), kNeither);
    EXPECT_EQ(curvature(kNearly, 0.0, 0.1), kAffine);
    EXPECT_EQ(curvature(kStraight, 0.0, 2.0), kNeither);
    EXPECT_EQ(monotonicity(kStraight, 0.0, 2.0), kRising);
    EXPECT_EQ(curvature(kLevel, -1.0, 1.0), kNeither);
    EXPECT_EQ(monotonicity(kLevel, -1.0, 1.0), kRising);
}

TEST(CurvatureTest, Monotone)
{
    EXPECT_EQ(monotonicity(kValley, -kInfinity, kInfinity), kEither);
    EXPECT_EQ(monotonicity(kValley, 0.0, kInfinity), kRising);
    EXPECT_EQ(monotonicity(kValley, -kInfinity, 0.0), kFalling);
    EXPECT_EQ(monotonicity(kWave, -1.0, 0.0), kRising);
    EXPECT_EQ(monotonicity(kWave, -2.0, -1.0), kFalling);
    EXPECT_EQ(monotonicity(kWave, -1.5, -0.5), kEither);
    EXPECT_EQ(monotonicity(kSteps, -kInfinity, kInfinity), kConstant);
    EXPECT_EQ(monotonicity(kWave, 0.5, 0.5), kConstant);
    EXPECT_EQ(monotonicity(kValley, kInfinity, -kInfinity), kEither);
}

TEST(CurvatureTest, Nested)
{
    const Line bowl{.x = {0.0, 1.0, 2.0}, .y = {0.0, 1.0, 3.0}};
    const Line falling{.x = {0.0, 1.0, 2.0}, .y = {3.0, 1.0, 0.0}};
    const Line cap{.x = {-2.0, -1.0, 0.0}, .y = {-3.0, -1.0, 0.0}};
    const Line cup{.x = {-2.0, -1.0, 0.0}, .y = {0.0, 1.0, 3.0}};
    EXPECT_EQ(nested(bowl, Operator::Exp, 1.0, {-3.0, 0.5}), kConvex);
    EXPECT_EQ(nested(falling, Operator::Exp, 1.0, {-3.0, 0.5}), kNeither);
    EXPECT_EQ(nested(falling, Operator::Exp, 1.0, {-3.0, -0.1}), kConcave);
    EXPECT_EQ(nested(cap, Operator::Exp, -1.0, {-3.0, 0.5}), kConcave);
    EXPECT_EQ(nested(cup, Operator::Exp, -1.0, {-3.0, 0.5}), kNeither);
    EXPECT_EQ(nested(falling, Operator::Log, 1.0, {0.5, 2.0}), kConvex);
}

TEST(CurvatureTest, Slopes)
{
    std::mt19937_64 engine(kSeed);
    int reported = 0;
    int claimed = 0;
    for (int round = 0; round < kLines; ++round)
    {
        const Line line = random_line(engine);
        for (int k = 0; k < kBoxes; ++k)
        {
            const Interval box = random_box(line, engine);
            const Profile profile = segments(line.pwl(), box);
            claimed += profile.curvature == kNeither ? 0 : 1;
            if (!holds(profile, slopes_of(line, box)) && reported++ < kReported)
            {
                ADD_FAILURE() << text_of(line) << " over [" << box.lower() << ", " << box.upper()
                              << "] claims " << static_cast<int>(profile.curvature) << " "
                              << static_cast<int>(profile.argument.monotonicity);
            }
        }
    }
    EXPECT_GT(claimed, kLines);
}
