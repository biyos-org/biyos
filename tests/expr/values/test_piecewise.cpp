#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/hessian/hessian.hpp"
#include "expr/tape/tape.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace
{

using biyos::Block;
using biyos::Graph;
using biyos::Hessian;
using biyos::Instruction;
using biyos::Offset;
using biyos::Operator;
using biyos::Tape;
using biyos::test::jacobian_of;
using biyos::test::values_of;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kRounding = 4.0 * std::numeric_limits<double>::epsilon();
constexpr double kStep = 1e-7;
constexpr double kBound = 1e-6;
constexpr std::array kKinks{-2.0, -0.5, 1.0, 3.0};
constexpr std::array kHeights{1.0, -1.0, 0.5, 2.0};
constexpr std::array kShort{0.0, 4.0};
constexpr std::array kRamp{-1.0, 3.0};
constexpr std::array kBudgets{Offset{1}, Offset{2}, Offset{3}, biyos::kBudget};

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

std::size_t segment(double t)
{
    std::size_t k = 0;
    while (k + 2 < kKinks.size() && !(t < kKinks[k + 1]))
    {
        ++k;
    }
    return k;
}

double slope(std::size_t k)
{
    return (kHeights[k + 1] - kHeights[k]) / (kKinks[k + 1] - kKinks[k]);
}

long double line(double t)
{
    const std::size_t k = segment(t);
    const long double rise = static_cast<long double>(kHeights[k + 1]) - kHeights[k];
    const long double run = static_cast<long double>(kKinks[k + 1]) - kKinks[k];
    return kHeights[k] + (static_cast<long double>(t) - kKinks[k]) * rise / run;
}

std::vector<double> probes()
{
    std::vector<double> found{-1e300, -50.0, -2.5, -1.25, 0.0, -0.0, 0.25, 2.0, 7.0, 1e300};
    for (const double x : kKinks)
    {
        found.push_back(x);
        found.push_back(std::nextafter(x, -kInfinity));
        found.push_back(std::nextafter(x, kInfinity));
    }
    return found;
}

struct Single
{
    Graph graph;
    Offset root;
};

Single single(std::span<const double> x, std::span<const double> y)
{
    Single made;
    const Offset pwl = made.graph.add_pwl(x, y);
    const Offset t = made.graph.add_variable(0);
    made.root = made.graph.add_piecewise(pwl, std::span(&t, 1));
    return made;
}

Single single()
{
    Single made;
    const Offset pwl = made.graph.add_pwl(kKinks, kHeights);
    const Offset x = made.graph.add_variable(0);
    made.root = made.graph.add_piecewise(pwl, std::span(&x, 1));
    return made;
}

double value_at(const Tape &tape, double t)
{
    return values_of(tape, std::array{t}).front();
}

double slope_at(const Tape &tape, double t)
{
    return jacobian_of(tape, std::array{t}).front();
}

std::size_t pwl_of(const Instruction &instruction)
{
    return instruction.second == kKinks.size() ? 0 : 1;
}

void expect_table(const Block &block, const Instruction &instruction)
{
    const auto start = static_cast<std::size_t>(instruction.payload);
    const bool wide = pwl_of(instruction) == 0;
    const std::span<const double> x = wide ? std::span<const double>(kKinks) : kShort;
    const std::span<const double> y = wide ? std::span<const double>(kHeights) : kRamp;
    ASSERT_LE(start + 2 * x.size(), block.weights.size());
    for (std::size_t k = 0; k < x.size(); ++k)
    {
        EXPECT_EQ(bits(block.weights[start + k]), bits(x[k]));
        EXPECT_EQ(bits(block.weights[start + x.size() + k]), bits(y[k]));
    }
}

void expect_shared(const Block &block)
{
    std::array<std::size_t, 2> places{block.weights.size(), block.weights.size()};
    std::size_t stored = 0;
    for (const Instruction &instruction : block.instructions)
    {
        if (instruction.op != Operator::PiecewiseLinear)
        {
            continue;
        }
        expect_table(block, instruction);
        const std::size_t pwl = pwl_of(instruction);
        if (places[pwl] == block.weights.size())
        {
            places[pwl] = static_cast<std::size_t>(instruction.payload);
            stored += 2 * instruction.second;
        }
        EXPECT_EQ(instruction.payload, places[pwl]);
    }
    EXPECT_EQ(block.weights.size(), stored);
}

double hessian_at(Hessian &hessian, double t)
{
    std::vector<double> out(hessian.pattern().rows.size(), kNan);
    hessian.values(std::array{t}, std::array{1.0}, out);
    return out.front();
}

}

TEST(PiecewiseTest, Breakpoints)
{
    const Single made = single();
    const Tape tape(made.graph, std::span(&made.root, 1));
    for (std::size_t k = 0; k < kKinks.size(); ++k)
    {
        SCOPED_TRACE(k);
        EXPECT_EQ(bits(value_at(tape, kKinks[k])), bits(kHeights[k]));
    }
}

TEST(PiecewiseTest, Rounded)
{
    const std::array x{0.0, 0.1, 0.3, 0.7};
    const std::array y{0.1, 0.7, 0.2, 0.9};
    Graph graph;
    const Offset pwl = graph.add_pwl(x, y);
    const Offset t = graph.add_variable(0);
    const Offset root = graph.add_piecewise(pwl, std::span(&t, 1));
    const Tape tape(graph, std::span(&root, 1));
    for (std::size_t k = 0; k < x.size(); ++k)
    {
        SCOPED_TRACE(k);
        EXPECT_EQ(bits(value_at(tape, x[k])), bits(y[k]));
    }
}

TEST(PiecewiseTest, Values)
{
    const Single made = single();
    const Tape tape(made.graph, std::span(&made.root, 1));
    for (const double t : probes())
    {
        SCOPED_TRACE(t);
        const long double wanted = line(t);
        const auto scale = static_cast<double>(std::fabs(wanted)) + 1.0;
        EXPECT_NEAR(value_at(tape, t), static_cast<double>(wanted), kRounding * scale);
    }
}

TEST(PiecewiseTest, Nonfinite)
{
    const Single made = single();
    const Tape tape(made.graph, std::span(&made.root, 1));
    EXPECT_EQ(bits(value_at(tape, kInfinity)), bits(kInfinity));
    EXPECT_EQ(bits(value_at(tape, -kInfinity)), bits(kInfinity));
    EXPECT_TRUE(std::isnan(value_at(tape, kNan)));
    EXPECT_EQ(bits(slope_at(tape, kInfinity)), bits(slope(2)));
    EXPECT_EQ(bits(slope_at(tape, -kInfinity)), bits(slope(0)));
}

TEST(PiecewiseTest, Flat)
{
    const std::array x{-1.0, 0.0, 2.0, 3.0};
    const std::array y{4.0, 4.0, -1.0, -1.0};
    const Single made = single(x, y);
    const Tape tape(made.graph, std::span(&made.root, 1));
    EXPECT_EQ(bits(value_at(tape, -kInfinity)), bits(4.0));
    EXPECT_EQ(bits(value_at(tape, kInfinity)), bits(-1.0));
    EXPECT_TRUE(std::isnan(value_at(tape, kNan)));
    EXPECT_EQ(bits(slope_at(tape, -kInfinity)), bits(0.0));
    EXPECT_EQ(bits(slope_at(tape, kInfinity)), bits(0.0));
}

TEST(PiecewiseTest, Underflow)
{
    const std::array x{0.0, 1e300};
    const std::array y{0.0, 1e-300};
    const Single made = single(x, y);
    const Tape tape(made.graph, std::span(&made.root, 1));
    EXPECT_EQ(bits(value_at(tape, 0.0)), bits(0.0));
    EXPECT_EQ(bits(value_at(tape, 1e300)), bits(1e-300));
    EXPECT_EQ(bits(value_at(tape, -kInfinity)), bits(0.0));
    EXPECT_EQ(bits(value_at(tape, kInfinity)), bits(1e-300));
}

TEST(PiecewiseTest, Signed)
{
    const std::array x{0.0, 1.0};
    const std::array y{-0.0, -0.0};
    const Single made = single(x, y);
    const Tape tape(made.graph, std::span(&made.root, 1));
    for (const double t : {-1.0, 0.0, 0.5, 1.0, 2.0})
    {
        SCOPED_TRACE(t);
        EXPECT_EQ(bits(value_at(tape, t)), bits(-0.0));
    }
}

TEST(PiecewiseTest, Sloped)
{
    const std::array x{0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    const std::array y{-0.0, 1.0, -0.0, 1.0, -1.0, -0.0};
    const Single made = single(x, y);
    const Tape tape(made.graph, std::span(&made.root, 1));
    for (std::size_t k = 0; k < x.size(); ++k)
    {
        SCOPED_TRACE(k);
        EXPECT_EQ(bits(value_at(tape, x[k])), bits(y[k]));
    }
}

TEST(PiecewiseTest, Distant)
{
    const std::array right{1e307, 2e307};
    const std::array left{-2e307, -1e307};
    const std::array y{0.0, 1.0};
    const Single below = single(right, y);
    const Single above = single(left, y);
    const Tape low(below.graph, std::span(&below.root, 1));
    const Tape high(above.graph, std::span(&above.root, 1));
    EXPECT_NEAR(value_at(low, -1.7e308), -18.0, kRounding * 18.0);
    EXPECT_NEAR(value_at(high, 1.7e308), 19.0, kRounding * 19.0);
}

TEST(PiecewiseTest, Shifted)
{
    const std::array x{1e10, 1e10 + 3.0};
    const std::array y{0.0, 1.0};
    const Single made = single(x, y);
    const Tape tape(made.graph, std::span(&made.root, 1));
    EXPECT_NEAR(value_at(tape, 1e10 + 1.0), 1.0 / 3.0, kRounding);
}

TEST(PiecewiseTest, Slopes)
{
    const Single made = single();
    const Tape tape(made.graph, std::span(&made.root, 1));
    for (const double t : probes())
    {
        SCOPED_TRACE(t);
        EXPECT_EQ(bits(slope_at(tape, t)), bits(slope(segment(t))));
    }
    EXPECT_EQ(bits(slope_at(tape, -0.5)), bits(slope(1)));
    EXPECT_EQ(bits(slope_at(tape, 1.0)), bits(slope(2)));
    EXPECT_EQ(bits(slope_at(tape, -2.0)), bits(slope(0)));
    EXPECT_EQ(bits(slope_at(tape, 3.0)), bits(slope(2)));
}

TEST(PiecewiseTest, Differences)
{
    const Single made = single();
    const Tape tape(made.graph, std::span(&made.root, 1));
    for (const double t : {-9.0, -1.25, 0.25, 2.0, 5.5})
    {
        SCOPED_TRACE(t);
        const double ahead = value_at(tape, t + kStep);
        const double behind = value_at(tape, t - kStep);
        EXPECT_NEAR(slope_at(tape, t), (ahead - behind) / (2.0 * kStep), kBound);
    }
}

TEST(PiecewiseTest, Curvature)
{
    Single made = single();
    const std::array pair{made.root, made.root};
    const Offset square = made.graph.add(Operator::Product, 1.0, pair);
    Hessian flat(made.graph, std::span(&made.root, 1), {});
    Hessian curved(made.graph, std::span(&square, 1), {});
    ASSERT_EQ(flat.pattern().rows.size(), 1U);
    for (const double t : probes())
    {
        SCOPED_TRACE(t);
        const double s = slope(segment(t));
        EXPECT_EQ(bits(hessian_at(flat, t)), bits(0.0));
        EXPECT_NEAR(hessian_at(curved, t), 2.0 * s * s, kRounding * 2.0 * s * s);
    }
}

TEST(PiecewiseTest, Pattern)
{
    Graph graph;
    const Offset pwl = graph.add_pwl(kKinks, kHeights);
    const std::array x{graph.add_variable(0), graph.add_variable(1), graph.add_variable(2)};
    const std::array pair{x[0], x[2]};
    const Offset inner = graph.add_sum(0.5, pair, std::array{1.0, -2.0});
    const Offset root = graph.add_piecewise(pwl, std::span(&inner, 1));
    const Tape tape(graph, std::span(&root, 1));
    const std::vector<biyos::Index> wanted{0, 2};
    EXPECT_EQ(tape.pattern().variables, wanted);
    const std::vector<double> gradient = jacobian_of(tape, std::array{0.25, 9.0, 0.5});
    const double s = slope(segment(0.5 + 0.25 - 1.0));
    EXPECT_EQ(bits(gradient[0]), bits(s));
    EXPECT_EQ(bits(gradient[1]), bits(-2.0 * s));
    Hessian hessian(graph, std::span(&root, 1), {});
    const std::vector<Offset> starts{0, 2, 2, 3};
    const std::vector<biyos::Index> rows{0, 2, 2};
    EXPECT_EQ(hessian.pattern().starts, starts);
    EXPECT_EQ(hessian.pattern().rows, rows);
}

TEST(PiecewiseTest, Shared)
{
    Graph graph;
    const Offset wide = graph.add_pwl(kKinks, kHeights);
    const Offset narrow = graph.add_pwl(kShort, kRamp);
    const std::array x{graph.add_variable(0), graph.add_variable(1)};
    std::vector<Offset> roots;
    for (const Offset pwl : {wide, narrow, wide})
    {
        for (const Offset child : x)
        {
            roots.push_back(graph.add_piecewise(pwl, std::span(&child, 1)));
        }
    }
    roots.push_back(graph.add_piecewise(wide, std::span(&roots.back(), 1)));
    const std::array point{-0.5, 2.5};
    for (const Offset budget : kBudgets)
    {
        SCOPED_TRACE(budget);
        const Tape tape(graph, roots, budget);
        for (const Block &block : tape.blocks())
        {
            expect_shared(block);
        }
        const std::vector<double> values = values_of(tape, point);
        EXPECT_EQ(bits(values[0]), bits(kHeights[1]));
        EXPECT_EQ(bits(values[1]), bits(1.625));
        EXPECT_EQ(bits(values[2]), bits(-1.5));
        EXPECT_EQ(bits(values[3]), bits(1.5));
        EXPECT_EQ(bits(values[4]), bits(values[0]));
        EXPECT_EQ(bits(values[5]), bits(values[1]));
        EXPECT_EQ(bits(values[6]), bits(0.96875));
    }
}
