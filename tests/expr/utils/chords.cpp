#include "chords.hpp"

#include "core/numbers.hpp"
#include "expr/curvature/shape.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/intervals/interval.hpp"
#include "reference.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Curvature;
using biyos::Graph;
using biyos::Interval;
using biyos::Monotonicity;
using biyos::Offset;
using biyos::Operator;
using biyos::test::Claims;
using biyos::test::Engine;
using biyos::test::Monotonicities;
using biyos::test::text;

using Values = std::vector<double>;

struct Run
{
    Values values;
    Values sizes;
};

using Runs = std::span<const Run>;

constexpr double kReach = 1000.0;
constexpr double kSlack = 0x1p-30;
constexpr double kFloor = 0x1p-1000;
constexpr int kChords = 96;
constexpr int kPairs = 48;
constexpr int kReported = 5;
constexpr std::uint64_t kSeed = 20261111;

std::size_t at(Offset value)
{
    return static_cast<std::size_t>(value);
}

double unit(Engine &engine)
{
    return static_cast<double>(engine() >> 11) * 0x1p-53;
}

std::size_t pick(Engine &engine, std::size_t n)
{
    return static_cast<std::size_t>(engine() % n);
}

double finite_lower(Interval box)
{
    return std::isfinite(box.lower()) ? box.lower() : (std::min)(box.upper(), 0.0) - kReach;
}

double finite_upper(Interval box)
{
    return std::isfinite(box.upper()) ? box.upper() : (std::max)(box.lower(), 0.0) + kReach;
}

double coordinate(Interval box, Engine &engine)
{
    const double low = finite_lower(box);
    const double high = finite_upper(box);
    switch (pick(engine, 4))
    {
    case 0:
        return low;
    case 1:
        return high;
    default:
        return std::clamp(low + (high - low) * unit(engine), low, high);
    }
}

bool is_empty(std::span<const Interval> box)
{
    return std::ranges::any_of(box, [](Interval interval) { return interval.is_empty(); });
}

Values point_of(std::span<const Interval> box, Engine &engine)
{
    Values point(box.size());
    for (std::size_t k = 0; k < box.size(); ++k)
    {
        point[k] = coordinate(box[k], engine);
    }
    return point;
}

Values between(std::span<const Interval> box, const Values &x, const Values &y, double theta)
{
    Values point(box.size());
    for (std::size_t k = 0; k < box.size(); ++k)
    {
        const double inside = std::lerp(y[k], x[k], theta);
        point[k] = std::clamp(inside, finite_lower(box[k]), finite_upper(box[k]));
    }
    return point;
}

bool unreal_below(const Graph &graph, const Values &values, Offset node)
{
    const std::span<const Offset> children = graph.children(node);
    return std::ranges::any_of(children,
                               [&](Offset child) { return !std::isfinite(values[at(child)]); });
}

double weight(const Graph &graph, Offset node, std::size_t k)
{
    const std::span<const double> weights = graph.coefficients(node);
    return weights.empty() ? 1.0 : std::fabs(weights[k]);
}

double size_of(const Graph &graph, const Run &run, Offset node)
{
    const std::span<const Offset> children = graph.children(node);
    const double constant = graph.op(node) == Operator::Sum ? graph.constant(node) : 0.0;
    double size = std::fabs(run.values[at(node)]) + std::fabs(constant);
    for (std::size_t k = 0; k < children.size(); ++k)
    {
        size += weight(graph, node, k) * run.sizes[at(children[k])];
    }
    return size;
}

double weight_of(const Values &x, const Values &y, const Values &z, double theta)
{
    std::size_t widest = 0;
    for (std::size_t k = 1; k < x.size(); ++k)
    {
        if (std::fabs(x[k] - y[k]) > std::fabs(x[widest] - y[widest]))
        {
            widest = k;
        }
    }
    const double span = x[widest] - y[widest];
    return std::fpclassify(span) == FP_ZERO ? theta : (z[widest] - y[widest]) / span;
}

Run evaluated(const Graph &graph, const Values &point)
{
    Run run{.values = Values(at(graph.size())), .sizes = Values(at(graph.size()))};
    biyos::test::evaluate(graph, point, run.values);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (unreal_below(graph, run.values, node))
        {
            run.values[at(node)] = std::nan("");
        }
        run.sizes[at(node)] = size_of(graph, run, node);
    }
    return run;
}

double scale_of(Offset node, Runs runs)
{
    double scale = 0.0;
    for (const Run &run : runs)
    {
        scale += run.sizes[at(node)];
    }
    return scale;
}

}

std::string biyos::test::name_of(Curvature curvature)
{
    switch (curvature)
    {
    case Curvature::Unknown:
        return "unknown";
    case Curvature::Convex:
        return "convex";
    case Curvature::Concave:
        return "concave";
    case Curvature::Affine:
        return "affine";
    }
    return "invalid";
}

std::string biyos::test::name_of(Monotonicity monotonicity)
{
    switch (monotonicity)
    {
    case Monotonicity::Unknown:
        return "unknown";
    case Monotonicity::Nondecreasing:
        return "nondecreasing";
    case Monotonicity::Nonincreasing:
        return "nonincreasing";
    case Monotonicity::Constant:
        return "constant";
    }
    return "invalid";
}

void biyos::PrintTo(Curvature curvature, std::ostream *out)
{
    *out << biyos::test::name_of(curvature);
}

void biyos::PrintTo(Monotonicity monotonicity, std::ostream *out)
{
    *out << biyos::test::name_of(monotonicity);
}

namespace
{

using biyos::test::name_of;

std::string point_text(const Values &point)
{
    std::string message;
    for (const double value : point)
    {
        message += " " + text(value);
    }
    return message;
}

void expect_chord(const Claims &claims, Offset node, Runs runs, double theta, const Values &z)
{
    const Curvature claim = claims.curvatures[at(node)];
    const double fx = runs[0].values[at(node)];
    const double fy = runs[1].values[at(node)];
    const double fz = runs[2].values[at(node)];
    if (claim == Curvature::Unknown || !std::isfinite(fx) || !std::isfinite(fy) ||
        !std::isfinite(fz))
    {
        return;
    }
    const double chord = std::lerp(fy, fx, theta);
    const double slack = kSlack * scale_of(node, runs) + kFloor;
    const bool above = fz > chord + slack;
    const bool below = fz < chord - slack;
    if (!(is_convex(claim) && above) && !(is_concave(claim) && below))
    {
        return;
    }
    if (claims.reported++ < kReported)
    {
        ADD_FAILURE() << "node " << node << " claimed " << name_of(claim) << ": f(z) = " << text(fz)
                      << " against the chord " << text(chord) << " at theta " << text(theta)
                      << ", f(x) = " << text(fx) << ", f(y) = " << text(fy) << ", z at"
                      << point_text(z);
    }
}

void expect_pair(const Monotonicities &claims,
                 std::size_t position,
                 const Values &low,
                 const Values &high)
{
    const Graph &graph = claims.graph;
    const Offset root = graph.size() - 1;
    const std::array runs{evaluated(graph, low), evaluated(graph, high)};
    const double first = runs[0].values[at(root)];
    const double last = runs[1].values[at(root)];
    if (!std::isfinite(first) || !std::isfinite(last))
    {
        return;
    }
    const Monotonicity claim = claims.monotonicities[position];
    const double slack = kSlack * scale_of(root, runs) + kFloor;
    const bool fell = last < first - slack;
    const bool rose = last > first + slack;
    if (!(rises(claim) && fell) && !(falls(claim) && rose))
    {
        return;
    }
    if (claims.reported++ < kReported)
    {
        ADD_FAILURE() << "child " << position << " claimed " << name_of(claim) << ": "
                      << text(first) << " at" << point_text(low) << ", " << text(last) << " at"
                      << point_text(high);
    }
}

}

void biyos::test::expect_chords(const Claims &claims, Engine &engine)
{
    if (is_empty(claims.box))
    {
        return;
    }
    for (int round = 0; round < kChords; ++round)
    {
        const Values x = point_of(claims.box, engine);
        const Values y = point_of(claims.box, engine);
        const double theta = pick(engine, 2) == 0 ? 0.5 : unit(engine);
        const Values z = between(claims.box, x, y, theta);
        const double weight = weight_of(x, y, z, theta);
        const std::array runs{evaluated(claims.graph, x),
                              evaluated(claims.graph, y),
                              evaluated(claims.graph, z)};
        for (Offset node = 0; node < claims.graph.size(); ++node)
        {
            expect_chord(claims, node, runs, weight, z);
        }
    }
}

void biyos::test::expect_monotonicities(const Monotonicities &claims, Engine &engine)
{
    if (is_empty(claims.box))
    {
        return;
    }
    const Graph &graph = claims.graph;
    const std::span<const Offset> children = graph.children(graph.size() - 1);
    for (int round = 0; round < kPairs; ++round)
    {
        const Values base = point_of(claims.box, engine);
        for (std::size_t k = 0; k < claims.monotonicities.size(); ++k)
        {
            const auto variable = static_cast<std::size_t>(graph.variable(children[k]));
            const double first = coordinate(claims.box[variable], engine);
            const double second = coordinate(claims.box[variable], engine);
            Values low = base;
            Values high = base;
            low[variable] = (std::min)(first, second);
            high[variable] = (std::max)(first, second);
            expect_pair(claims, k, low, high);
        }
    }
}

void biyos::test::expect_root(const Graph &graph,
                              std::span<const Interval> box,
                              Curvature curvature,
                              std::span<const Monotonicity> monotonicities)
{
    std::vector<Curvature> curvatures(at(graph.size()), Curvature::Unknown);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        const Operator op = graph.op(node);
        if (op == Operator::Variable || op == Operator::Constant)
        {
            curvatures[at(node)] = Curvature::Affine;
        }
    }
    curvatures.back() = curvature;
    Engine engine(kSeed);
    int reported = 0;
    expect_chords({.graph = graph, .box = box, .curvatures = curvatures, .reported = reported},
                  engine);
    expect_monotonicities(
        {.graph = graph, .box = box, .monotonicities = monotonicities, .reported = reported},
        engine);
}
