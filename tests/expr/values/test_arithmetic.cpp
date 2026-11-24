#include "chords.hpp"
#include "core/index.hpp"
#include "expr/curvature/arithmetic.hpp"
#include "expr/curvature/shape.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/intervals/interval.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <string>

namespace
{

using biyos::Argument;
using biyos::Curvature;
using biyos::Interval;
using biyos::Monotonicity;
using biyos::Offset;
using biyos::Operands;
using biyos::Operator;
using biyos::test::Parameters;
using biyos::test::text;

using Box = std::array<Interval, 3>;
using Trios = std::array<Monotonicity, 3>;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr Interval kZero{0.0, 0.0};
constexpr std::array<Offset, 3> kChildren{0, 1, 2};

constexpr Curvature kAffine = Curvature::Affine;
constexpr Curvature kConvex = Curvature::Convex;
constexpr Curvature kConcave = Curvature::Concave;
constexpr Curvature kNeither = Curvature::Unknown;
constexpr Monotonicity kRising = Monotonicity::Nondecreasing;
constexpr Monotonicity kFalling = Monotonicity::Nonincreasing;
constexpr Monotonicity kFlat = Monotonicity::Constant;
constexpr Monotonicity kUnsure = Monotonicity::Unknown;

struct Trio
{
    double coefficient;
    Box box;
    Curvature curvature;
    Trios monotonicities;
};

constexpr std::array kProducts{
    Trio{2.0, {{{3.0, 3.0}, {-1.0, 2.0}, {4.0, 4.0}}}, kAffine, {kUnsure, kRising, kUnsure}},
    Trio{2.0, {{{3.0, 3.0}, {1.0, 2.0}, {1.0, 2.0}}}, kNeither, {kRising, kRising, kRising}},
    Trio{-1.0, {{{1.0, 2.0}, {-2.0, -1.0}, {3.0, 3.0}}}, kNeither, {kRising, kFalling, kRising}},
    Trio{0.0, {{{-1.0, 2.0}, {-1.0, 2.0}, {-1.0, 2.0}}}, kAffine, {kFlat, kFlat, kFlat}},
    Trio{1.5, {{{0.0, 0.0}, {-1.0, 2.0}, {-3.0, 5.0}}}, kAffine, {kUnsure, kFlat, kFlat}},
    Trio{1.0, {{{-1.0, 0.0}, {0.0, 2.0}, {2.0, 2.0}}}, kNeither, {kRising, kFalling, kFalling}},
    Trio{-3.0,
         {{{-2.0, -2.0}, {-kInfinity, 1.0}, {5.0, 5.0}}},
         kAffine,
         {kUnsure, kRising, kUnsure}},
};

constexpr std::array kLargest{
    Trio{0.0, {{{0.0, 1.0}, {2.0, 3.0}, {-1.0, 0.0}}}, kAffine, {kFlat, kRising, kFlat}},
    Trio{0.0, {{{0.0, 2.0}, {1.0, 3.0}, {-1.0, 0.0}}}, kConvex, {kRising, kRising, kFlat}},
    Trio{0.0, {{{0.0, 2.0}, {1.0, 3.0}, {-1.0, 1.0}}}, kConvex, {kRising, kRising, kFlat}},
    Trio{0.0, {{{1.0, 1.0}, {1.0, 1.0}, {1.0, 1.0}}}, kAffine, {kRising, kFlat, kFlat}},
    Trio{0.0, {{Interval::entire(), {0.0, 1.0}, {2.0, 3.0}}}, kConvex, {kRising, kFlat, kRising}},
    Trio{0.0, {{{2.0, 2.0}, {2.0, 5.0}, {0.0, 1.0}}}, kAffine, {kFlat, kRising, kFlat}},
    Trio{0.0, {{{2.0, 2.0}, {-kInfinity, 3.0}, {2.0, 2.0}}}, kConvex, {kRising, kRising, kFlat}},
};

constexpr std::array kSmallest{
    Trio{0.0, {{{0.0, 1.0}, {2.0, 3.0}, {-1.0, 0.0}}}, kAffine, {kFlat, kFlat, kRising}},
    Trio{0.0, {{{0.0, 2.0}, {1.0, 3.0}, {-1.0, 1.0}}}, kConcave, {kRising, kFlat, kRising}},
    Trio{0.0,
         {{{0.0, 2.0}, {-1.0, 3.0}, {-kInfinity, 1.0}}},
         kConcave,
         {kRising, kRising, kRising}},
    Trio{0.0,
         {{{-3.0, -3.0}, {-kInfinity, 0.5}, {-3.0, -3.0}}},
         kConcave,
         {kRising, kRising, kFlat}},
};

struct Quotient
{
    Interval numerator;
    Interval denominator;
    Curvature curvature;
    Monotonicity numerator_monotonicity;
    Monotonicity denominator_monotonicity;
    bool cut_below;
    bool cut_above;
};

constexpr std::array kQuotients{
    Quotient{{1.0, 2.0}, {2.0, 2.0}, kAffine, kRising, kFlat, false, false},
    Quotient{{1.0, 2.0}, {-2.0, -2.0}, kAffine, kFalling, kFlat, false, false},
    Quotient{{1.0, 1.0}, {1.0, 2.0}, kConvex, kRising, kFalling, false, false},
    Quotient{{-1.0, -1.0}, {1.0, 2.0}, kConcave, kRising, kRising, false, false},
    Quotient{{1.0, 1.0}, {-2.0, -1.0}, kConcave, kFalling, kFalling, false, false},
    Quotient{{1.0, 1.0}, {0.0, 2.0}, kConvex, kRising, kFalling, true, false},
    Quotient{{-1.0, -1.0}, {-2.0, 0.0}, kConvex, kFalling, kRising, false, true},
    Quotient{{0.0, 0.0}, {1.0, 2.0}, kAffine, kRising, kFlat, false, false},
    Quotient{{1.0, 1.0}, {-1.0, 1.0}, kNeither, kUnsure, kUnsure, false, false},
    Quotient{{1.0, 2.0}, {1.0, 2.0}, kNeither, kRising, kFalling, false, false},
    Quotient{{-1.0, 2.0}, {1.0, kInfinity}, kNeither, kRising, kUnsure, false, false},
};

std::string box_text(std::span<const Interval> box)
{
    std::string message = "box";
    for (const Interval interval : box)
    {
        message += " [" + text(interval.lower()) + ", " + text(interval.upper()) + "]";
    }
    return message;
}

Parameters with(double coefficient)
{
    return {.constant = coefficient, .weights = {1.0, -2.0, 0.0}, .exponent = 1.0};
}

using Rule = Curvature (*)(const Trio &, std::span<Argument>);

void expect_trios(Operator op, std::span<const Trio> trios, Rule rule)
{
    for (const Trio &trio : trios)
    {
        SCOPED_TRACE(box_text(trio.box) + ", coefficient " + text(trio.coefficient));
        std::array<Argument, 3> arguments{};
        const Curvature found = rule(trio, arguments);
        EXPECT_EQ(found, trio.curvature);
        Trios monotonicities{};
        for (std::size_t k = 0; k < monotonicities.size(); ++k)
        {
            monotonicities[k] = arguments[k].monotonicity;
            EXPECT_EQ(monotonicities[k], trio.monotonicities[k]) << "child " << k;
            EXPECT_FALSE(arguments[k].cut_below || arguments[k].cut_above);
        }
        const auto graph = biyos::test::operator_graph(op, with(trio.coefficient));
        biyos::test::expect_root(graph, trio.box, found, monotonicities);
    }
}

Curvature product_rule(const Trio &trio, std::span<Argument> arguments)
{
    const Operands factors{.children = kChildren, .nodes = trio.box};
    return biyos::product_curvature(trio.coefficient, factors, arguments);
}

Curvature largest_rule(const Trio &trio, std::span<Argument> arguments)
{
    return biyos::largest_curvature({.children = kChildren, .nodes = trio.box}, arguments);
}

Curvature smallest_rule(const Trio &trio, std::span<Argument> arguments)
{
    return biyos::smallest_curvature({.children = kChildren, .nodes = trio.box}, arguments);
}

}

TEST(CurvatureTest, Sums)
{
    const std::array weights{1.0, -2.0, 0.0};
    std::array<Argument, 3> arguments{};
    EXPECT_EQ(biyos::sum_curvature(weights, arguments), kAffine);
    const Trios monotonicities{arguments[0].monotonicity,
                               arguments[1].monotonicity,
                               arguments[2].monotonicity};
    EXPECT_EQ(monotonicities, (Trios{kRising, kFalling, kFlat}));
    const Box box{{{-1.0, 2.0}, {-kInfinity, 3.0}, {0.0, 1.0}}};
    const auto graph = biyos::test::operator_graph(Operator::Sum, with(0.5));
    biyos::test::expect_root(graph, box, kAffine, monotonicities);
}

TEST(CurvatureTest, Products)
{
    expect_trios(Operator::Product, kProducts, product_rule);
}

TEST(CurvatureTest, EmptyFactor)
{
    const Box box{{{1.0, 1.0}, Interval::empty(), {2.0, 3.0}}};
    std::array<Argument, 3> arguments{};
    const Operands factors{.children = kChildren, .nodes = box};
    EXPECT_EQ(biyos::product_curvature(2.0, factors, arguments), kNeither);
    for (const Argument &argument : arguments)
    {
        EXPECT_EQ(argument.monotonicity, kUnsure);
    }
}

TEST(CurvatureTest, Extremes)
{
    expect_trios(Operator::Max, kLargest, largest_rule);
    expect_trios(Operator::Min, kSmallest, smallest_rule);
}

TEST(CurvatureTest, Quotients)
{
    for (const Quotient &row : kQuotients)
    {
        const std::array pair{row.numerator, row.denominator};
        SCOPED_TRACE(box_text(pair));
        std::array<Argument, 2> arguments{};
        const Curvature found =
            biyos::quotient_curvature(row.numerator, row.denominator, arguments);
        EXPECT_EQ(found, row.curvature);
        EXPECT_EQ(arguments[0].monotonicity, row.numerator_monotonicity);
        EXPECT_EQ(arguments[1].monotonicity, row.denominator_monotonicity);
        EXPECT_EQ(arguments[1].cut_below, row.cut_below);
        EXPECT_EQ(arguments[1].cut_above, row.cut_above);
        const Box box{row.numerator, row.denominator, kZero};
        const std::array monotonicities{arguments[0].monotonicity, arguments[1].monotonicity};
        const auto graph = biyos::test::operator_graph(Operator::Division, with(1.0));
        biyos::test::expect_root(graph, box, found, monotonicities);
    }
}
