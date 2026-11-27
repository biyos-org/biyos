#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "reference.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace
{

using biyos::forward;
using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::Tape;
using biyos::Tolerances;
using biyos::Workspace;
using biyos::test::binary;
using biyos::test::bits;
using biyos::test::branch;
using biyos::test::compare_tolerances;
using biyos::test::constant;
using biyos::test::default_tolerances;
using biyos::test::jacobian_of;
using biyos::test::kComparisons;
using biyos::test::tolerances_of;
using biyos::test::unary;
using biyos::test::values_of;

using Truths = std::vector<Real>;
using Inputs = std::array<bool, 3>;

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

constexpr std::size_t kInputs = 3;
constexpr std::array<Inputs, 8> kReads{{
    {true, true, false},
    {true, true, false},
    {true, false, false},
    {true, true, true},
    {true, true, true},
    {false, false, true},
    {false, false, true},
    {true, true, true},
}};

struct Logic
{
    Graph graph;
    std::vector<Offset> roots;

    Logic()
    {
        const Offset zero = constant(graph, 0.0);
        std::array<Offset, kInputs> p{};
        for (std::size_t k = 0; k < kInputs; ++k)
        {
            const Offset x = graph.add_variable(static_cast<biyos::Index>(k));
            p[k] = binary(graph, Operator::Less, x, zero);
        }
        const std::array pair{p[0], p[1]};
        const std::array tail{p[1], p[2]};
        const Offset either = graph.add(Operator::Or, tail);
        const std::array nested{p[0], either};
        roots = {
            graph.add(Operator::And, pair),
            graph.add(Operator::Or, pair),
            unary(graph, Operator::Not, p[0]),
            graph.add(Operator::And, p),
            graph.add(Operator::Or, p),
            graph.add(Operator::And, std::span(&p[2], 1)),
            graph.add(Operator::Or, std::span(&p[2], 1)),
            unary(graph, Operator::Not, graph.add(Operator::And, nested)),
        };
    }

    Tape tape() const { return Tape(graph, roots); }
};

Real indicator(bool value)
{
    return value ? 1.0 : 0.0;
}

Truths expected(const Inputs &t)
{
    return {
        indicator(t[0] && t[1]),
        indicator(t[0] || t[1]),
        indicator(!t[0]),
        indicator(t[0] && t[1] && t[2]),
        indicator(t[0] || t[1] || t[2]),
        indicator(t[2]),
        indicator(t[2]),
        indicator(!(t[0] && (t[1] || t[2]))),
    };
}

Inputs inputs_of(unsigned mask)
{
    return {(mask & 1U) != 0, (mask & 2U) != 0, (mask & 4U) != 0};
}

std::vector<Real> point_of(const Inputs &t)
{
    std::vector<Real> point(kInputs);
    for (std::size_t k = 0; k < kInputs; ++k)
    {
        point[k] = t[k] ? -1.0 : 1.0;
    }
    return point;
}

Real chosen(const Tape &tape, Real x, Real then, Real otherwise)
{
    return values_of(tape, std::array{x, then, otherwise})[0];
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
    const Tolerances wide = compare_tolerances(0.1, 0.0);
    EXPECT_EQ(bits(truths(0.0, 0.05, wide)), bits(kEqual));
    EXPECT_EQ(bits(truths(0.0, 0.2, wide)), bits(kBelow));
    const Tolerances exact = compare_tolerances(0.0, 0.0);
    EXPECT_EQ(bits(truths(1.0, 1.0, exact)), bits(kEqual));
    EXPECT_EQ(bits(truths(1.0, std::nextafter(1.0, 2.0), exact)), bits(kBelow));
    EXPECT_EQ(bits(truths(std::nextafter(1.0, 2.0), 1.0, exact)), bits(kAbove));
    EXPECT_EQ(bits(truths(0.0, 0.05, tolerances_of(0.1, 0.0))), bits(kBelow));
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

TEST(LogicTest, Truths)
{
    const Logic made;
    const Tape tape = made.tape();
    for (unsigned mask = 0; mask < 8; ++mask)
    {
        const Inputs t = inputs_of(mask);
        SCOPED_TRACE(mask);
        EXPECT_EQ(bits(values_of(tape, point_of(t))), bits(expected(t)));
    }
}

TEST(LogicTest, Undefined)
{
    const Logic made;
    const Tape tape = made.tape();
    for (std::size_t k = 0; k < kInputs; ++k)
    {
        for (unsigned mask = 0; mask < 8; ++mask)
        {
            const Inputs t = inputs_of(mask);
            std::vector<Real> point = point_of(t);
            point[k] = kNan;
            const Truths found = values_of(tape, point);
            const Truths wanted = expected(t);
            for (std::size_t r = 0; r < found.size(); ++r)
            {
                SCOPED_TRACE(testing::Message()
                             << "input " << k << ", mask " << mask << ", root " << r);
                if (kReads[r][k])
                {
                    EXPECT_TRUE(std::isnan(found[r]));
                    continue;
                }
                EXPECT_EQ(bits(found[r]), bits(wanted[r]));
            }
        }
    }
}

TEST(LogicTest, Slopes)
{
    const Logic made;
    const Tape tape = made.tape();
    for (unsigned mask = 0; mask < 8; ++mask)
    {
        for (const Real slope : jacobian_of(tape, point_of(inputs_of(mask))))
        {
            EXPECT_DOUBLE_EQ(slope, 0.0) << mask;
        }
    }
    std::vector<Real> point = point_of(inputs_of(2));
    point[0] = kNan;
    for (std::size_t r = 0; r < made.roots.size(); ++r)
    {
        const Tape single(made.graph, std::span(&made.roots[r], 1));
        for (const Real slope : jacobian_of(single, point))
        {
            EXPECT_EQ(std::isnan(slope), kReads[r][0]) << r;
        }
    }
}

TEST(BranchTest, Taken)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset negative = binary(graph, Operator::Less, x, constant(graph, 0.0));
    const Offset root = branch(graph, negative, graph.add_variable(1), graph.add_variable(2));
    const Tape tape(graph, std::span(&root, 1));
    EXPECT_EQ(bits(chosen(tape, -1.0, 2.0, 3.0)), bits(2.0));
    EXPECT_EQ(bits(chosen(tape, 1.0, 2.0, 3.0)), bits(3.0));
    EXPECT_EQ(bits(chosen(tape, -1.0, 2.0, kNan)), bits(2.0));
    EXPECT_EQ(bits(chosen(tape, 1.0, kInfinity, 3.0)), bits(3.0));
    EXPECT_TRUE(std::isnan(chosen(tape, kNan, 2.0, 3.0)));
}
