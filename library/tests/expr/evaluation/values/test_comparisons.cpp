#include "core/env/env.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace
{

using biyos::Env;
using biyos::forward;
using biyos::Graph;
using biyos::Lookup;
using biyos::Offset;
using biyos::Operator;
using biyos::Option;
using biyos::Options;
using biyos::Real;
using biyos::Tape;
using biyos::Tolerances;
using biyos::Workspace;
using biyos::test::binary;
using biyos::test::bits;
using biyos::test::default_tolerances;
using biyos::test::jacobian_of;
using biyos::test::kComparisons;

using Truths = std::vector<Real>;

constexpr Real kInfinity = (std::numeric_limits<Real>::infinity)();
constexpr Real kLargest = (std::numeric_limits<Real>::max)();
constexpr Real kNan = (std::numeric_limits<Real>::quiet_NaN)();
constexpr Real kBand = 1e-6;
constexpr std::size_t kLe = 0;
constexpr std::size_t kGe = 1;
constexpr std::size_t kEq = 2;
constexpr std::size_t kLt = 3;
constexpr std::size_t kGt = 4;
constexpr std::size_t kNe = 5;

const Real kPast = std::nextafter(kBand, 1.0);
const Truths kBelow{1.0, 0.0, 0.0, 1.0, 0.0, 1.0};
const Truths kAbove{0.0, 1.0, 0.0, 0.0, 1.0, 1.0};
const Truths kEqual{1.0, 1.0, 1.0, 0.0, 0.0, 0.0};

struct Comparisons
{
    Graph graph;
    std::vector<Offset> roots;

    Comparisons()
    {
        const Offset a = graph.add_variable(0);
        const Offset b = graph.add_variable(1);
        for (const Operator op : kComparisons)
        {
            roots.push_back(binary(graph, op, a, b));
        }
    }

    Tape tape() const { return Tape(graph, roots); }
};

Tolerances absolute(Real feastol)
{
    Env env;
    Options options(env);
    options.set_double(Option::FeasAbs, feastol);
    options.set_double(Option::FeasRel, 0.0);
    return Tolerances(Lookup(options));
}

Truths truths(Real a, Real b, const Tolerances &tolerances)
{
    const Comparisons made;
    const Tape tape = made.tape();
    Workspace workspace(tape);
    Truths values(made.roots.size());
    forward(tape, std::array{a, b}, tolerances, workspace, values);
    return values;
}

Truths truths(Real a, Real b)
{
    return truths(a, b, default_tolerances());
}

void expect_truths(Real a, Real b, const Truths &wanted)
{
    SCOPED_TRACE(testing::Message() << a << " against " << b);
    EXPECT_EQ(bits(truths(a, b)), bits(wanted));
}

void expect_complement(Real truth, Real negation)
{
    if (std::isnan(truth))
    {
        EXPECT_TRUE(std::isnan(negation));
        return;
    }
    EXPECT_EQ(bits(negation), bits(1.0 - truth));
}

}

TEST(ComparisonTest, Apart)
{
    expect_truths(1.0, 2.0, kBelow);
    expect_truths(2.0, 1.0, kAbove);
    expect_truths(-3.0, -3.0, kEqual);
    expect_truths(-0.0, 0.0, kEqual);
}

TEST(ComparisonTest, Band)
{
    expect_truths(5e-7, 0.0, kEqual);
    expect_truths(kBand, 0.0, kEqual);
    expect_truths(0.0, kBand, kEqual);
    expect_truths(kPast, 0.0, kAbove);
    expect_truths(0.0, kPast, kBelow);
    expect_truths(1e9 + 500.0, 1e9, kEqual);
    expect_truths(1e9 + 2000.0, 1e9, kAbove);
    expect_truths(-1e9, -1e9 + 2000.0, kBelow);
}

TEST(ComparisonTest, Infinities)
{
    expect_truths(kInfinity, kInfinity, kEqual);
    expect_truths(-kInfinity, -kInfinity, kEqual);
    expect_truths(-kInfinity, kInfinity, kBelow);
    expect_truths(kInfinity, kLargest, kAbove);
    expect_truths(-kInfinity, -kLargest, kBelow);
    expect_truths(0.0, kInfinity, kBelow);
}

TEST(ComparisonTest, Undefined)
{
    const std::array<std::array<Real, 2>, 4> pairs{{
        {kNan, 0.0},
        {0.0, kNan},
        {kNan, kNan},
        {kNan, kInfinity},
    }};
    for (const auto &[a, b] : pairs)
    {
        for (const Real truth : truths(a, b))
        {
            EXPECT_TRUE(std::isnan(truth)) << a << " against " << b;
        }
    }
}

TEST(ComparisonTest, Negations)
{
    const std::array grid{
        -kInfinity,
        -1e9,
        -1.0,
        -kBand,
        -5e-7,
        -0.0,
        0.0,
        5e-7,
        kBand,
        kPast,
        1.0,
        1e9,
        1e9 + 500.0,
        1e9 + 2000.0,
        kLargest,
        kInfinity,
        kNan,
    };
    for (const Real a : grid)
    {
        for (const Real b : grid)
        {
            SCOPED_TRACE(testing::Message() << a << " against " << b);
            const Truths found = truths(a, b);
            const Truths swapped = truths(b, a);
            expect_complement(found[kGe], found[kLt]);
            expect_complement(found[kLe], found[kGt]);
            expect_complement(found[kEq], found[kNe]);
            EXPECT_EQ(bits(found[kGe]), bits(swapped[kLe]));
            EXPECT_EQ(bits(found[kEq]), bits(swapped[kEq]));
        }
    }
}

TEST(ComparisonTest, Options)
{
    const Tolerances wide = absolute(0.1);
    EXPECT_EQ(bits(truths(0.0, 0.05, wide)), bits(kEqual));
    EXPECT_EQ(bits(truths(0.0, 0.2, wide)), bits(kBelow));
    const Tolerances exact = absolute(0.0);
    EXPECT_EQ(bits(truths(1.0, 1.0, exact)), bits(kEqual));
    EXPECT_EQ(bits(truths(1.0, std::nextafter(1.0, 2.0), exact)), bits(kBelow));
    EXPECT_EQ(bits(truths(std::nextafter(1.0, 2.0), 1.0, exact)), bits(kAbove));
}

TEST(ComparisonTest, Slopes)
{
    const Comparisons made;
    const Tape tape = made.tape();
    const std::array<std::array<Real, 2>, 4> defined{{
        {1.0, 2.0},
        {kBand, 0.0},
        {kInfinity, 1.0},
        {-kInfinity, -kInfinity},
    }};
    for (const auto &point : defined)
    {
        const std::vector<Real> slopes = jacobian_of(tape, point);
        EXPECT_EQ(slopes.size(), 2 * kComparisons.size());
        for (const Real slope : slopes)
        {
            EXPECT_DOUBLE_EQ(slope, 0.0) << point[0] << " against " << point[1];
        }
    }
    for (const Real slope : jacobian_of(tape, std::array{kNan, 1.0}))
    {
        EXPECT_TRUE(std::isnan(slope));
    }
}
