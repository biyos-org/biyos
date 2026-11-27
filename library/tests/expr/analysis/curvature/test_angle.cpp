#include "chords.hpp"
#include "core/numbers.hpp"
#include "expr/analysis/curvature/curvature.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "reference.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
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
using biyos::Real;
using biyos::Shapes;
using biyos::to_size;
using biyos::test::Box;
using biyos::test::enclosures;
using biyos::test::text;

using Values = std::vector<Real>;
using Grid = std::vector<Values>;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kStep = 0x1p-4;
constexpr Real kReach = 4.0;
constexpr Real kSlack = 0x1p-40;
constexpr Interval kZero{0.0, 0.0};
constexpr Interval kOne{1.0, 1.0};
constexpr Interval kMinusOne{-1.0, -1.0};
constexpr Interval kTwo{2.0, 2.0};
constexpr Interval kMinusTwo{-2.0, -2.0};

constexpr Curvature kAffine = Curvature::Affine;
constexpr Curvature kConvex = Curvature::Convex;
constexpr Curvature kConcave = Curvature::Concave;
constexpr Curvature kNeither = Curvature::Unknown;
constexpr Monotonicity kRising = Monotonicity::Nondecreasing;
constexpr Monotonicity kFalling = Monotonicity::Nonincreasing;
constexpr Monotonicity kFlat = Monotonicity::Constant;
constexpr Monotonicity kUnsure = Monotonicity::Unknown;

struct Row
{
    Interval y;
    Interval x;
    Curvature curvature;
    Monotonicity along_y;
    Monotonicity along_x;
};

const std::array kRows{
    Row{kOne, {0.0, 2.0}, kConvex, kFlat, kFalling},
    Row{kOne, {0.0, kInfinity}, kConvex, kFlat, kFalling},
    Row{kOne, {-2.0, 0.0}, kConcave, kFlat, kFalling},
    Row{kOne, {-1.0, 1.0}, kNeither, kFlat, kFalling},
    Row{kMinusOne, {0.0, 2.0}, kConcave, kFlat, kRising},
    Row{kMinusOne, {-2.0, 0.0}, kConvex, kFlat, kRising},
    Row{kMinusOne, {-kInfinity, 0.0}, kConvex, kFlat, kRising},
    Row{kMinusOne, {-1.0, 1.0}, kNeither, kFlat, kRising},
    Row{kZero, {0.0, 2.0}, kAffine, kFlat, kFlat},
    Row{kZero, {-2.0, -kStep}, kAffine, kFlat, kFlat},
    Row{kZero, {-2.0, 0.0}, kNeither, kFlat, kFalling},
    Row{kZero, {-1.0, 1.0}, kNeither, kFlat, kFalling},
    Row{{-2.0, 0.0}, kTwo, kConvex, kRising, kFlat},
    Row{{-kInfinity, 0.0}, kTwo, kConvex, kRising, kFlat},
    Row{{0.0, 2.0}, kTwo, kConcave, kRising, kFlat},
    Row{{-1.0, 1.0}, kTwo, kNeither, kRising, kFlat},
    Row{{0.0, 2.0}, kMinusTwo, kConvex, kFalling, kFlat},
    Row{{0.0, kInfinity}, kMinusTwo, kConvex, kFalling, kFlat},
    Row{{-2.0, -kStep}, kMinusTwo, kConcave, kFalling, kFlat},
    Row{{-2.0, 0.0}, kMinusTwo, kNeither, kUnsure, kFlat},
    Row{{-1.0, 1.0}, kMinusTwo, kNeither, kUnsure, kFlat},
    Row{{kStep, 2.0}, kZero, kAffine, kFlat, kFlat},
    Row{{-2.0, -kStep}, kZero, kAffine, kFlat, kFlat},
    Row{{0.0, 2.0}, kZero, kNeither, kRising, kFlat},
    Row{{-2.0, 0.0}, kZero, kNeither, kRising, kFlat},
    Row{kOne, kMinusTwo, kAffine, kFlat, kFlat},
    Row{kZero, kZero, kAffine, kFlat, kFlat},
    Row{{0.0, 2.0}, {0.0, 2.0}, kNeither, kRising, kFalling},
    Row{{-2.0, -1.0}, {-2.0, -1.0}, kNeither, kFalling, kRising},
    Row{{0.0, 2.0}, {-2.0, -1.0}, kNeither, kFalling, kFalling},
    Row{{-1.0, 1.0}, {-2.0, -1.0}, kNeither, kUnsure, kUnsure},
    Row{{-1.0, 1.0}, {1.0, 2.0}, kNeither, kRising, kUnsure},
    Row{{-2.0, 0.0}, {-1.0, 1.0}, kNeither, kUnsure, kUnsure},
    Row{{-1.0, 1.0}, {-1.0, 1.0}, kNeither, kUnsure, kUnsure},
    Row{{0.0, 1.0}, {-1.0, 0.0}, kNeither, kUnsure, kFalling},
    Row{{-2.0, -1.0}, {-1.0, 1.0}, kNeither, kUnsure, kRising},
};

struct Chain
{
    std::size_t varying;
    Real point;
    Real shift;
    Real weight;
    Curvature curvature;
};

const std::array kChains{
    Chain{1, -1.0, -4.0, 1.0, kConvex},
    Chain{1, -1.0, 4.0, -1.0, kConcave},
    Chain{1, 1.0, -4.0, 1.0, kConcave},
    Chain{1, -1.0, 1.0, 1.0, kNeither},
    Chain{0, 2.0, 4.0, -1.0, kConcave},
    Chain{0, 2.0, -4.0, 1.0, kConvex},
    Chain{0, -2.0, 4.0, -1.0, kConvex},
    Chain{0, -2.0, -4.0, 1.0, kConcave},
    Chain{1, 0.0, 1.0, 1.0, kAffine},
    Chain{0, 0.0, 1.0, 1.0, kAffine},
};

Graph angle_graph()
{
    Graph graph;
    const std::array children{graph.add_variable(0), graph.add_variable(1)};
    graph.add(Operator::Atan2, children);
    return graph;
}

Graph chain_graph(const Chain &chain)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset square = graph.add(Operator::ConstantPower, 2.0, std::span(&x, 1));
    const std::array weights{chain.weight};
    const Offset point = graph.add(Operator::Constant, chain.point, {});
    const Offset inner = graph.add_sum(chain.shift, std::span(&square, 1), weights);
    const std::array children =
        chain.varying == 0 ? std::array{inner, point} : std::array{point, inner};
    graph.add(Operator::Atan2, children);
    return graph;
}

Values ticks_of(Interval range)
{
    const Real low = (std::max)(range.lower(), -kReach);
    const Real high = (std::min)(range.upper(), kReach);
    Values ticks;
    for (Real tick = low; tick <= high; tick += kStep)
    {
        ticks.push_back(tick);
    }
    return ticks;
}

Real value_at(const Graph &graph, std::span<const Real> point, Values &values)
{
    biyos::test::evaluate(graph, point, values);
    return values.back();
}

Grid grid_of(const Graph &graph, const Values &ys, const Values &xs)
{
    Grid grid(ys.size(), Values(xs.size()));
    Values values(to_size(graph.size()));
    for (std::size_t i = 0; i < ys.size(); ++i)
    {
        for (std::size_t j = 0; j < xs.size(); ++j)
        {
            const std::array point{ys[i], xs[j], 0.0};
            grid[i][j] = value_at(graph, point, values);
        }
    }
    return grid;
}

Values line_of(const Grid &grid, std::size_t child, std::size_t at)
{
    if (child == 1)
    {
        return grid[at];
    }
    Values line(grid.size());
    for (std::size_t i = 0; i < grid.size(); ++i)
    {
        line[i] = grid[i][at];
    }
    return line;
}

bool bends_as(const Values &line, Curvature claim)
{
    for (std::size_t i = 1; i + 1 < line.size(); ++i)
    {
        const Real second = line[i - 1] - 2.0 * line[i] + line[i + 1];
        if ((is_convex(claim) && second < -kSlack) || (is_concave(claim) && second > kSlack))
        {
            return false;
        }
    }
    return true;
}

bool moves_as(const Values &line, Monotonicity claim)
{
    for (std::size_t i = 0; i + 1 < line.size(); ++i)
    {
        const Real step = line[i + 1] - line[i];
        if ((rises(claim) && step < -kSlack) || (falls(claim) && step > kSlack))
        {
            return false;
        }
    }
    return true;
}

Values chain_values(const Graph &graph, const Values &ticks)
{
    Values line(ticks.size());
    Values values(to_size(graph.size()));
    for (std::size_t i = 0; i < ticks.size(); ++i)
    {
        const std::array point{ticks[i], 0.0, 0.0};
        line[i] = value_at(graph, point, values);
    }
    return line;
}

std::string row_text(const Row &row)
{
    return "y in [" + text(row.y.lower()) + ", " + text(row.y.upper()) + "], x in [" +
           text(row.x.lower()) + ", " + text(row.x.upper()) + "]";
}

void expect_grid(const Row &row, const Grid &grid)
{
    const std::array claims{row.along_y, row.along_x};
    const std::array counts{grid.front().size(), grid.size()};
    for (std::size_t child = 0; child < claims.size(); ++child)
    {
        for (std::size_t at = 0; at < counts[child]; ++at)
        {
            const Values line = line_of(grid, child, at);
            EXPECT_TRUE(bends_as(line, row.curvature)) << "child " << child << " line " << at;
            EXPECT_TRUE(moves_as(line, claims[child])) << "child " << child << " line " << at;
        }
    }
}

void expect_row(const Row &row)
{
    SCOPED_TRACE(row_text(row));
    const Graph graph = angle_graph();
    const Offset root = graph.size() - 1;
    Shapes shapes(graph);
    shapes.classify(graph, enclosures(graph, Box{row.y, row.x, kZero}));
    EXPECT_EQ(shapes.curvature(root), row.curvature);
    EXPECT_EQ(shapes.monotonicity(root, 0), row.along_y);
    EXPECT_EQ(shapes.monotonicity(root, 1), row.along_x);
    expect_grid(row, grid_of(graph, ticks_of(row.y), ticks_of(row.x)));
}

}

TEST(CurvatureTest, Angles)
{
    for (const Row &row : kRows)
    {
        expect_row(row);
    }
}

TEST(CurvatureTest, AngleChains)
{
    const Interval range{-1.0, 1.0};
    for (const Chain &chain : kChains)
    {
        SCOPED_TRACE("child " + std::to_string(chain.varying) + ", point " + text(chain.point) +
                     ", shift " + text(chain.shift) + ", weight " + text(chain.weight));
        const Graph graph = chain_graph(chain);
        const Offset root = graph.size() - 1;
        Shapes shapes(graph);
        shapes.classify(graph, enclosures(graph, Box{range, kZero, kZero}));
        EXPECT_EQ(shapes.curvature(root), chain.curvature);
        EXPECT_TRUE(bends_as(chain_values(graph, ticks_of(range)), chain.curvature));
    }
}
