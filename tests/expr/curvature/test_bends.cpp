#include "chords.hpp"
#include "expr/bends.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/intervals/interval.hpp"
#include "expr/intervals/rounding.hpp"
#include "expr/shape.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>

namespace
{

using biyos::Bend;
using biyos::Curvature;
using biyos::Interval;
using biyos::Monotonicity;
using biyos::Operator;
using biyos::Profile;
using biyos::test::Parameters;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kInverseE = 0x1.78b56362cef38p-2;
constexpr Interval kZero{0.0, 0.0};
constexpr Interval kInverseEnclosure{0x1.78b56362cef37p-2, kInverseE};
constexpr int kSteps = 4;
constexpr double kReach = 0x1p-20;
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
    Row{Operator::Negation, &biyos::kNegation, {-1.0, 2.0}, kAffine, kFalling},
    Row{Operator::Exp, &biyos::kExp, {-1.0, 2.0}, kConvex, kRising},
    Row{Operator::Exp, &biyos::kExp, Interval::entire(), kConvex, kRising},
    Row{Operator::Exp, &biyos::kExp, {0.5, 0.5}, kAffine, kFlat},
    Row{Operator::Log, &biyos::kLog, {0.5, 4.0}, kConcave, kRising},
    Row{Operator::Log, &biyos::kLog, {0.0, 4.0}, kConcave, kRising},
    Row{Operator::Log, &biyos::kLog, {-1.0, kInfinity}, kConcave, kRising},
    Row{Operator::Log, &biyos::kLog, {-1.0, 0.0}, kNeither, kUnsure},
    Row{Operator::Sqrt, &biyos::kSqrt, {0.0, 4.0}, kConcave, kRising},
    Row{Operator::Sqrt, &biyos::kSqrt, {-1.0, 0.0}, kAffine, kFlat},
    Row{Operator::Abs, &biyos::kAbs, {0.0, 3.0}, kAffine, kRising},
    Row{Operator::Abs, &biyos::kAbs, {-3.0, 0.0}, kAffine, kFalling},
    Row{Operator::Abs, &biyos::kAbs, {-1.0, 2.0}, kConvex, kUnsure},
    Row{Operator::Asin, &biyos::kAsin, {-1.0, -0.5}, kConcave, kRising},
    Row{Operator::Asin, &biyos::kAsin, {0.0, 1.0}, kConvex, kRising},
    Row{Operator::Asin, &biyos::kAsin, {-0.5, 0.5}, kNeither, kRising},
    Row{Operator::Asin, &biyos::kAsin, {-3.0, -0.5}, kConcave, kRising},
    Row{Operator::Acos, &biyos::kAcos, {-1.0, 0.0}, kConvex, kFalling},
    Row{Operator::Acos, &biyos::kAcos, {0.5, 1.0}, kConcave, kFalling},
    Row{Operator::Acos, &biyos::kAcos, {-0.5, 0.5}, kNeither, kFalling},
    Row{Operator::Atan, &biyos::kAtan, {-kInfinity, 0.0}, kConvex, kRising},
    Row{Operator::Atan, &biyos::kAtan, {0.0, 3.0}, kConcave, kRising},
    Row{Operator::Atan, &biyos::kAtan, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Sinh, &biyos::kSinh, {-2.0, 0.0}, kConcave, kRising},
    Row{Operator::Sinh, &biyos::kSinh, {0.0, 2.0}, kConvex, kRising},
    Row{Operator::Sinh, &biyos::kSinh, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Cosh, &biyos::kCosh, {-2.0, 0.0}, kConvex, kFalling},
    Row{Operator::Cosh, &biyos::kCosh, {0.0, 2.0}, kConvex, kRising},
    Row{Operator::Cosh, &biyos::kCosh, {-1.0, 2.0}, kConvex, kUnsure},
    Row{Operator::Tanh, &biyos::kTanh, {-2.0, 0.0}, kConvex, kRising},
    Row{Operator::Tanh, &biyos::kTanh, {0.0, kInfinity}, kConcave, kRising},
    Row{Operator::Tanh, &biyos::kTanh, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Asinh, &biyos::kAsinh, {-2.0, 0.0}, kConvex, kRising},
    Row{Operator::Asinh, &biyos::kAsinh, {0.0, 2.0}, kConcave, kRising},
    Row{Operator::Asinh, &biyos::kAsinh, {-1.0, 1.0}, kNeither, kRising},
    Row{Operator::Acosh, &biyos::kAcosh, {1.0, 5.0}, kConcave, kRising},
    Row{Operator::Acosh, &biyos::kAcosh, {0.0, 5.0}, kConcave, kRising},
    Row{Operator::Atanh, &biyos::kAtanh, {-0.9, 0.0}, kConcave, kRising},
    Row{Operator::Atanh, &biyos::kAtanh, {0.0, 1.0}, kConvex, kRising},
    Row{Operator::Atanh, &biyos::kAtanh, {-1.0, 0.0}, kConcave, kRising},
    Row{Operator::Atanh, &biyos::kAtanh, {-0.5, 0.5}, kNeither, kRising},
    Row{Operator::Entropy, &biyos::kEntropy, {0.0, 0.3}, kConvex, kFalling},
    Row{Operator::Entropy, &biyos::kEntropy, {-1.0, 0.3}, kConvex, kFalling},
    Row{Operator::Entropy, &biyos::kEntropy, {kInverseE, 2.0}, kConvex, kRising},
    Row{Operator::Entropy, &biyos::kEntropy, {0.0, kInverseE}, kConvex, kUnsure},
    Row{Operator::Entropy, &biyos::kEntropy, {0.0, 1.0}, kConvex, kUnsure},
    Row{Operator::Erf, &biyos::kErf, {-2.0, 0.0}, kConvex, kRising},
    Row{Operator::Erf, &biyos::kErf, {0.0, 2.0}, kConcave, kRising},
    Row{Operator::Erf, &biyos::kErf, {-1.0, 1.0}, kNeither, kRising},
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
         &biyos::kAsin,
         kZero,
         {kConcave, kRising},
         {kConvex, kRising},
         {kNeither, kRising}},
    Edge{Operator::Acos,
         &biyos::kAcos,
         kZero,
         {kConvex, kFalling},
         {kConcave, kFalling},
         {kNeither, kFalling}},
    Edge{Operator::Atan,
         &biyos::kAtan,
         kZero,
         {kConvex, kRising},
         {kConcave, kRising},
         {kNeither, kRising}},
    Edge{Operator::Sinh,
         &biyos::kSinh,
         kZero,
         {kConcave, kRising},
         {kConvex, kRising},
         {kNeither, kRising}},
    Edge{Operator::Tanh,
         &biyos::kTanh,
         kZero,
         {kConvex, kRising},
         {kConcave, kRising},
         {kNeither, kRising}},
    Edge{Operator::Asinh,
         &biyos::kAsinh,
         kZero,
         {kConvex, kRising},
         {kConcave, kRising},
         {kNeither, kRising}},
    Edge{Operator::Atanh,
         &biyos::kAtanh,
         kZero,
         {kConcave, kRising},
         {kConvex, kRising},
         {kNeither, kRising}},
    Edge{Operator::Erf,
         &biyos::kErf,
         kZero,
         {kConvex, kRising},
         {kConcave, kRising},
         {kNeither, kRising}},
    Edge{Operator::Cosh,
         &biyos::kCosh,
         kZero,
         {kConvex, kFalling},
         {kConvex, kRising},
         {kConvex, kUnsure}},
    Edge{Operator::Abs,
         &biyos::kAbs,
         kZero,
         {kAffine, kFalling},
         {kAffine, kRising},
         {kConvex, kUnsure}},
    Edge{Operator::Entropy,
         &biyos::kEntropy,
         kInverseEnclosure,
         {kConvex, kFalling},
         {kConvex, kRising},
         {kConvex, kUnsure}},
};

std::string row_text(const Row &row)
{
    return "operator " + std::to_string(static_cast<int>(row.op)) + " over [" +
           biyos::test::text(row.child.lower()) + ", " + biyos::test::text(row.child.upper()) + "]";
}

void expect_row(const Row &row)
{
    SCOPED_TRACE(row_text(row));
    const Profile profile = biyos::bend(*row.shape, row.child);
    EXPECT_EQ(profile.curvature, row.curvature);
    EXPECT_EQ(profile.argument.monotonicity, row.monotonicity);
    const std::array box{row.child, kZero, kZero};
    const std::array monotonicities{profile.argument.monotonicity};
    biyos::test::expect_root(biyos::test::operator_graph(row.op, kFixed),
                             box,
                             profile.curvature,
                             monotonicities);
}

Row row_of(const Edge &edge, Interval child, Class expected)
{
    return {edge.op, edge.shape, child, expected.curvature, expected.monotonicity};
}

void expect_edge(const Edge &edge, double low, double high)
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
        expect_edge(edge, biyos::shifted(at.lower(), -kSteps), biyos::shifted(at.upper(), kSteps));
        expect_edge(edge, at.lower() - kReach, at.upper() + kReach);
    }
}

TEST(CurvatureTest, CutFlags)
{
    const auto cuts = [](const Bend &shape, Interval child)
    {
        const Profile profile = biyos::bend(shape, child);
        return std::array{profile.argument.cut_below, profile.argument.cut_above};
    };
    using Pair = std::array<bool, 2>;
    EXPECT_EQ(cuts(biyos::kLog, {0.0, 4.0}), (Pair{true, false}));
    EXPECT_EQ(cuts(biyos::kLog, {1.0, kInfinity}), (Pair{false, false}));
    EXPECT_EQ(cuts(biyos::kLog, {-1.0, kInfinity}), (Pair{true, false}));
    EXPECT_EQ(cuts(biyos::kSqrt, {0.0, 4.0}), (Pair{false, false}));
    EXPECT_EQ(cuts(biyos::kSqrt, {-1.0, 4.0}), (Pair{true, false}));
    EXPECT_EQ(cuts(biyos::kAtanh, {0.0, 1.0}), (Pair{false, true}));
    EXPECT_EQ(cuts(biyos::kAtanh, {-1.0, 1.0}), (Pair{true, true}));
    EXPECT_EQ(cuts(biyos::kAtanh, {-0.5, 0.5}), (Pair{false, false}));
    EXPECT_EQ(cuts(biyos::kAsin, {-1.0, 1.0}), (Pair{false, false}));
    EXPECT_EQ(cuts(biyos::kAsin, {-2.0, 2.0}), (Pair{true, true}));
    EXPECT_EQ(cuts(biyos::kAcosh, {0.0, 2.0}), (Pair{true, false}));
    EXPECT_EQ(cuts(biyos::kEntropy, {0.0, 1.0}), (Pair{false, false}));
    EXPECT_EQ(cuts(biyos::kExp, Interval::entire()), (Pair{false, false}));
    EXPECT_EQ(cuts(biyos::kAcosh, {0.0, 1.0}), (Pair{true, false}));
    EXPECT_EQ(cuts(biyos::kSqrt, {-1.0, 0.0}), (Pair{true, false}));
}

TEST(CurvatureTest, Undefined)
{
    EXPECT_EQ(biyos::bend(biyos::kLog, {-2.0, -1.0}).curvature, kNeither);
    EXPECT_EQ(biyos::bend(biyos::kLog, {0.0, 0.0}).argument.monotonicity, kUnsure);
    EXPECT_EQ(biyos::bend(biyos::kAtanh, {1.0, 3.0}).curvature, kNeither);
    EXPECT_EQ(biyos::bend(biyos::kAcosh, {-1.0, 0.5}).curvature, kNeither);
    EXPECT_EQ(biyos::bend(biyos::kExp, Interval::empty()).curvature, kNeither);
    EXPECT_EQ(biyos::bend(biyos::kExp, Interval::empty()).argument.monotonicity, kUnsure);
}
