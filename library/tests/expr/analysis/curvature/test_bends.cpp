#include "chords.hpp"
#include "core/numbers.hpp"
#include "expr/analysis/curvature/bends.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/analysis/intervals/rounding.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>

namespace
{

using biyos::bend;
using biyos::Bend;
using biyos::Curvature;
using biyos::Interval;
using biyos::kAbs;
using biyos::kAcos;
using biyos::kAcosh;
using biyos::kAsin;
using biyos::kAsinh;
using biyos::kAtan;
using biyos::kAtanh;
using biyos::kCosh;
using biyos::kEntropy;
using biyos::kErf;
using biyos::kExp;
using biyos::kLog;
using biyos::kNegation;
using biyos::kSinh;
using biyos::kSqrt;
using biyos::kTanh;
using biyos::Monotonicity;
using biyos::Operator;
using biyos::Profile;
using biyos::Real;
using biyos::shifted;
using biyos::test::expect_root;
using biyos::test::operator_graph;
using biyos::test::Parameters;
using biyos::test::text;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kInverseE = 0x1.78b56362cef38p-2;
constexpr Interval kZero{0.0, 0.0};
constexpr Interval kInverseEnclosure{0x1.78b56362cef37p-2, kInverseE};
constexpr int kSteps = 4;
constexpr Real kReach = 0x1p-20;
constexpr Parameters kFixed{.constant = 1.0, .weights = {1.0, 1.0, 1.0}, .exponent = 2.0};

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
    Operator op;
    const Bend *shape;
    Interval child;
    Curvature curvature;
    Monotonicity monotonicity;
};

const std::array kRows{
    Row{Operator::Negation, &kNegation, {-1.0, 2.0}, kAffine, kFalling},
    Row{Operator::Exp, &kExp, {-1.0, 2.0}, kConvex, kRising},
    Row{Operator::Exp, &kExp, Interval::entire(), kConvex, kRising},
    Row{Operator::Exp, &kExp, {0.5, 0.5}, kAffine, kFlat},
    Row{Operator::Log, &kLog, {0.5, 4.0}, kConcave, kRising},
    Row{Operator::Log, &kLog, {0.0, 4.0}, kConcave, kRising},
    Row{Operator::Log, &kLog, {-1.0, kInfinity}, kConcave, kRising},
    Row{Operator::Log, &kLog, {-1.0, 0.0}, kNeither, kUnsure},
    Row{Operator::Sqrt, &kSqrt, {0.0, 4.0}, kConcave, kRising},
    Row{Operator::Sqrt, &kSqrt, {-1.0, 0.0}, kAffine, kFlat},
    Row{Operator::Abs, &kAbs, {0.0, 3.0}, kAffine, kRising},
    Row{Operator::Abs, &kAbs, {-3.0, 0.0}, kAffine, kFalling},
    Row{Operator::Abs, &kAbs, {-1.0, 2.0}, kConvex, kUnsure},
    Row{Operator::Asin, &kAsin, {-1.0, -0.5}, kConcave, kRising},
    Row{Operator::Asin, &kAsin, {0.0, 1.0}, kConvex, kRising},
    Row{Operator::Asin, &kAsin, {-0.5, 0.5}, kNeither, kRising},
    Row{Operator::Asin, &kAsin, {-3.0, -0.5}, kConcave, kRising},
    Row{Operator::Acos, &kAcos, {-1.0, 0.0}, kConvex, kFalling},
    Row{Operator::Acos, &kAcos, {0.5, 1.0}, kConcave, kFalling},
    Row{Operator::Acos, &kAcos, {-0.5, 0.5}, kNeither, kFalling},
    Row{Operator::Atan, &kAtan, {-kInfinity, 0.0}, kConvex, kRising},
    Row{Operator::Atan, &kAtan, {0.0, 3.0}, kConcave, kRising},
    Row{Operator::Atan, &kAtan, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Sinh, &kSinh, {-2.0, 0.0}, kConcave, kRising},
    Row{Operator::Sinh, &kSinh, {0.0, 2.0}, kConvex, kRising},
    Row{Operator::Sinh, &kSinh, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Cosh, &kCosh, {-2.0, 0.0}, kConvex, kFalling},
    Row{Operator::Cosh, &kCosh, {0.0, 2.0}, kConvex, kRising},
    Row{Operator::Cosh, &kCosh, {-1.0, 2.0}, kConvex, kUnsure},
    Row{Operator::Tanh, &kTanh, {-2.0, 0.0}, kConvex, kRising},
    Row{Operator::Tanh, &kTanh, {0.0, kInfinity}, kConcave, kRising},
    Row{Operator::Tanh, &kTanh, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Asinh, &kAsinh, {-2.0, 0.0}, kConvex, kRising},
    Row{Operator::Asinh, &kAsinh, {0.0, 2.0}, kConcave, kRising},
    Row{Operator::Asinh, &kAsinh, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Acosh, &kAcosh, {1.0, 5.0}, kConcave, kRising},
    Row{Operator::Acosh, &kAcosh, {0.0, 5.0}, kConcave, kRising},
    Row{Operator::Atanh, &kAtanh, {-0.9, 0.0}, kConcave, kRising},
    Row{Operator::Atanh, &kAtanh, {0.0, 1.0}, kConvex, kRising},
    Row{Operator::Atanh, &kAtanh, {-1.0, 0.0}, kConcave, kRising},
    Row{Operator::Atanh, &kAtanh, {-0.5, 0.5}, kNeither, kRising},
    Row{Operator::Entropy, &kEntropy, {0.0, 0.3}, kConvex, kFalling},
    Row{Operator::Entropy, &kEntropy, {-1.0, 0.3}, kConvex, kFalling},
    Row{Operator::Entropy, &kEntropy, {kInverseE, 2.0}, kConvex, kRising},
    Row{Operator::Entropy, &kEntropy, {0.0, kInverseE}, kConvex, kUnsure},
    Row{Operator::Entropy, &kEntropy, {0.0, 1.0}, kConvex, kUnsure},
    Row{Operator::Erf, &kErf, {-2.0, 0.0}, kConvex, kRising},
    Row{Operator::Erf, &kErf, {0.0, 2.0}, kConcave, kRising},
    Row{Operator::Erf, &kErf, {-1.0, 1.0}, kNeither, kRising},
};

struct Class
{
    Curvature curvature;
    Monotonicity monotonicity;
};

struct Edge
{
    Operator op;
    const Bend *shape;
    Interval at;
    Class below;
    Class above;
    Class across;
};

const std::array kEdges{
    Edge{Operator::Asin,
         &kAsin,
         kZero,
         {kConcave, kRising},
         {kConvex, kRising},
         {kNeither, kRising}},
    Edge{Operator::Acos,
         &kAcos,
         kZero,
         {kConvex, kFalling},
         {kConcave, kFalling},
         {kNeither, kFalling}},
    Edge{Operator::Atan,
         &kAtan,
         kZero,
         {kConvex, kRising},
         {kConcave, kRising},
         {kNeither, kRising}},
    Edge{Operator::Sinh,
         &kSinh,
         kZero,
         {kConcave, kRising},
         {kConvex, kRising},
         {kNeither, kRising}},
    Edge{Operator::Tanh,
         &kTanh,
         kZero,
         {kConvex, kRising},
         {kConcave, kRising},
         {kNeither, kRising}},
    Edge{Operator::Asinh,
         &kAsinh,
         kZero,
         {kConvex, kRising},
         {kConcave, kRising},
         {kNeither, kRising}},
    Edge{Operator::Atanh,
         &kAtanh,
         kZero,
         {kConcave, kRising},
         {kConvex, kRising},
         {kNeither, kRising}},
    Edge{Operator::Erf, &kErf, kZero, {kConvex, kRising}, {kConcave, kRising}, {kNeither, kRising}},
    Edge{Operator::Cosh,
         &kCosh,
         kZero,
         {kConvex, kFalling},
         {kConvex, kRising},
         {kConvex, kUnsure}},
    Edge{Operator::Abs, &kAbs, kZero, {kAffine, kFalling}, {kAffine, kRising}, {kConvex, kUnsure}},
    Edge{Operator::Entropy,
         &kEntropy,
         kInverseEnclosure,
         {kConvex, kFalling},
         {kConvex, kRising},
         {kConvex, kUnsure}},
};

std::string row_text(const Row &row)
{
    return "operator " + std::to_string(static_cast<int>(row.op)) + " over [" +
           text(row.child.lower()) + ", " + text(row.child.upper()) + "]";
}

void expect_row(const Row &row)
{
    SCOPED_TRACE(row_text(row));
    const Profile profile = bend(*row.shape, row.child);
    EXPECT_EQ(profile.curvature, row.curvature);
    EXPECT_EQ(profile.argument.monotonicity, row.monotonicity);
    const std::array box{row.child, kZero, kZero};
    const std::array monotonicities{profile.argument.monotonicity};
    expect_root(operator_graph(row.op, kFixed), box, profile.curvature, monotonicities);
}

Row row_of(const Edge &edge, Interval child, Class expected)
{
    return {edge.op, edge.shape, child, expected.curvature, expected.monotonicity};
}

void expect_edge(const Edge &edge, Real low, Real high)
{
    const Interval at = edge.at;
    expect_row(row_of(edge, {low, at.lower()}, edge.below));
    expect_row(row_of(edge, {at.upper(), high}, edge.above));
    expect_row(row_of(edge, {low, high}, edge.across));
}

}

TEST(CurvatureTest, Bends)
{
    for (const Row &row : kRows)
    {
        expect_row(row);
    }
}

TEST(CurvatureTest, Edges)
{
    for (const Edge &edge : kEdges)
    {
        const Interval at = edge.at;
        expect_edge(edge, shifted(at.lower(), -kSteps), shifted(at.upper(), kSteps));
        expect_edge(edge, at.lower() - kReach, at.upper() + kReach);
    }
}

TEST(CurvatureTest, CutFlags)
{
    const auto cuts = [](const Bend &shape, Interval child)
    {
        const Profile profile = bend(shape, child);
        return std::array{profile.argument.cut_below, profile.argument.cut_above};
    };
    using Pair = std::array<bool, 2>;
    EXPECT_EQ(cuts(kLog, {0.0, 4.0}), (Pair{true, false}));
    EXPECT_EQ(cuts(kLog, {1.0, kInfinity}), (Pair{false, false}));
    EXPECT_EQ(cuts(kLog, {-1.0, kInfinity}), (Pair{true, false}));
    EXPECT_EQ(cuts(kSqrt, {0.0, 4.0}), (Pair{false, false}));
    EXPECT_EQ(cuts(kSqrt, {-1.0, 4.0}), (Pair{true, false}));
    EXPECT_EQ(cuts(kAtanh, {0.0, 1.0}), (Pair{false, true}));
    EXPECT_EQ(cuts(kAtanh, {-1.0, 1.0}), (Pair{true, true}));
    EXPECT_EQ(cuts(kAtanh, {-0.5, 0.5}), (Pair{false, false}));
    EXPECT_EQ(cuts(kAsin, {-1.0, 1.0}), (Pair{false, false}));
    EXPECT_EQ(cuts(kAsin, {-2.0, 2.0}), (Pair{true, true}));
    EXPECT_EQ(cuts(kAcosh, {0.0, 2.0}), (Pair{true, false}));
    EXPECT_EQ(cuts(kEntropy, {0.0, 1.0}), (Pair{false, false}));
    EXPECT_EQ(cuts(kExp, Interval::entire()), (Pair{false, false}));
    EXPECT_EQ(cuts(kAcosh, {0.0, 1.0}), (Pair{true, false}));
    EXPECT_EQ(cuts(kSqrt, {-1.0, 0.0}), (Pair{true, false}));
}

TEST(CurvatureTest, Undefined)
{
    EXPECT_EQ(bend(kLog, {-2.0, -1.0}).curvature, kNeither);
    EXPECT_EQ(bend(kLog, {0.0, 0.0}).argument.monotonicity, kUnsure);
    EXPECT_EQ(bend(kAtanh, {1.0, 3.0}).curvature, kNeither);
    EXPECT_EQ(bend(kAcosh, {-1.0, 0.5}).curvature, kNeither);
    EXPECT_EQ(bend(kExp, Interval::empty()).curvature, kNeither);
    EXPECT_EQ(bend(kExp, Interval::empty()).argument.monotonicity, kUnsure);
}
