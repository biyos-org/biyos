#include "bounds.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/analysis/intervals/logic.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/tape.hpp"
#include "reference.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace
{

using biyos::complement;
using biyos::Graph;
using biyos::Interval;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::taken;
using biyos::Tape;
using biyos::Tolerances;
using biyos::Workspace;
using biyos::test::compare_tolerances;
using biyos::test::default_tolerances;
using biyos::test::exact;
using biyos::test::interval_text;
using biyos::test::text;

struct Relation
{
    const char *name;
    Operator op;
    biyos::Comparison enclose;
};

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kReach = 1e3;
constexpr int kInterior = 3;
constexpr int kSteps = 200;

constexpr std::array kBases{0.0, 1.0, -1.0, -0.5, 1e-3, 1e3, -1e3};

constexpr std::array kWideMarks{
    -1e3,
    -10.0,
    -5.0,
    -2.5,
    -1.0,
    -0.9,
    -0.5,
    0.0,
    0.5,
    0.9,
    1.0,
    2.5,
    5.0,
    10.0,
    100.0,
};

const std::array kRelations{
    Relation{.name = "le", .op = Operator::LessEqual, .enclose = biyos::less_equal},
    Relation{.name = "ge", .op = Operator::GreaterEqual, .enclose = biyos::greater_equal},
    Relation{.name = "eq", .op = Operator::Equal, .enclose = biyos::equal},
    Relation{.name = "lt", .op = Operator::Less, .enclose = biyos::less},
    Relation{.name = "gt", .op = Operator::Greater, .enclose = biyos::greater},
    Relation{.name = "ne", .op = Operator::NotEqual, .enclose = biyos::not_equal},
};

std::vector<Offset> roots_of(Graph &graph)
{
    const Offset a = graph.add_variable(0);
    const Offset b = graph.add_variable(1);
    std::vector<Offset> roots;
    for (const Relation &relation : kRelations)
    {
        roots.push_back(graph.add(relation.op, std::array{a, b}));
    }
    return roots;
}

class Oracle
{
  public:
    explicit Oracle(const Tolerances &tolerances)
        : tolerances_(tolerances), roots_(roots_of(graph_)), tape_(graph_, roots_),
          workspace_(tape_), truths_(roots_.size())
    {
    }

    std::span<const Real> truths(Real x, Real y)
    {
        forward(tape_, std::array{x, y}, tolerances_, workspace_, truths_);
        return truths_;
    }

  private:
    const Tolerances &tolerances_;
    Graph graph_;
    std::vector<Offset> roots_;
    Tape tape_;
    Workspace workspace_;
    std::vector<Real> truths_;
};

std::vector<Tolerances> narrow_bands()
{
    return {default_tolerances(),
            compare_tolerances(0.1, 0.0),
            compare_tolerances(0.0, 0.5),
            compare_tolerances(0.05, 0.2)};
}

std::vector<Tolerances> wide_bands()
{
    return {compare_tolerances(0.0, 1.5),
            compare_tolerances(2.0, 0.0),
            compare_tolerances(0.5, 1.0)};
}

Real upper_edge(const Tolerances &tolerances, Real base)
{
    Real inside = base;
    Real outside = base + kReach * (1.0 + std::abs(base));
    for (int step = 0; step < kSteps; ++step)
    {
        const Real middle = inside + (outside - inside) / 2;
        if (middle <= inside || middle >= outside)
        {
            break;
        }
        if (tolerances.at_most(middle, base))
        {
            inside = middle;
        }
        else
        {
            outside = middle;
        }
    }
    return inside;
}

std::vector<Real> marks_of(const Tolerances &tolerances, Real base)
{
    const Real high = upper_edge(tolerances, base);
    const Real low = -upper_edge(tolerances, -base);
    return {
        low - (base - low),
        std::nextafter(low, -kInfinity),
        low,
        std::nextafter(low, kInfinity),
        (low + base) / 2,
        base,
        (base + high) / 2,
        std::nextafter(high, -kInfinity),
        high,
        std::nextafter(high, kInfinity),
        high + (high - base),
    };
}

std::vector<Interval> boxes_of(std::span<const Real> marks)
{
    std::vector<Interval> boxes;
    for (std::size_t i = 0; i < marks.size(); ++i)
    {
        for (std::size_t j = i; j < marks.size(); ++j)
        {
            boxes.emplace_back((std::min)(marks[i], marks[j]), (std::max)(marks[i], marks[j]));
        }
    }
    return boxes;
}

void add_inside(std::vector<Real> &samples, Interval box, Real value)
{
    if (box.lower() <= value && value <= box.upper())
    {
        samples.push_back(value);
    }
}

std::vector<Real> samples_of(Interval box)
{
    std::vector<Real> samples;
    add_inside(samples, box, box.lower());
    add_inside(samples, box, std::nextafter(box.lower(), kInfinity));
    add_inside(samples, box, std::nextafter(box.upper(), -kInfinity));
    add_inside(samples, box, box.upper());
    const Real width = box.upper() - box.lower();
    for (int k = 1; k < kInterior; ++k)
    {
        add_inside(samples, box, box.lower() + width * k / kInterior);
    }
    return samples;
}

bool encloses(Interval found, Real value)
{
    return !found.is_empty() && found.lower() <= value && value <= found.upper();
}

std::vector<Interval> enclosures_of(const Tolerances &tolerances, Interval a, Interval b)
{
    std::vector<Interval> found;
    for (const Relation &relation : kRelations)
    {
        found.push_back(relation.enclose(tolerances, a, b));
    }
    return found;
}

testing::AssertionResult missed(const std::vector<Interval> &found,
                                std::span<const Real> truths,
                                Real x,
                                Real y)
{
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        if (!encloses(found[k], truths[k]))
        {
            return testing::AssertionFailure()
                   << kRelations[k].name << " at " << text(x) << ", " << text(y) << " is "
                   << truths[k] << ", outside " << interval_text(found[k]);
        }
    }
    return testing::AssertionSuccess();
}

testing::AssertionResult sampled(const Tolerances &tolerances,
                                 Oracle &oracle,
                                 Interval a,
                                 Interval b)
{
    const std::vector<Interval> found = enclosures_of(tolerances, a, b);
    for (const Real x : samples_of(a))
    {
        for (const Real y : samples_of(b))
        {
            testing::AssertionResult held = missed(found, oracle.truths(x, y), x, y);
            if (!held)
            {
                return held << " over " << interval_text(a) << " and " << interval_text(b);
            }
        }
    }
    return testing::AssertionSuccess();
}

testing::AssertionResult sampled(const Tolerances &tolerances, std::span<const Real> marks)
{
    Oracle oracle(tolerances);
    const std::vector<Interval> boxes = boxes_of(marks);
    for (const Interval a : boxes)
    {
        for (const Interval b : boxes)
        {
            testing::AssertionResult held = sampled(tolerances, oracle, a, b);
            if (!held)
            {
                return held;
            }
        }
    }
    return testing::AssertionSuccess();
}

}

TEST(DecisionTest, Band)
{
    for (const Tolerances &tolerances : narrow_bands())
    {
        for (const Real base : kBases)
        {
            SCOPED_TRACE("base " + text(base));
            EXPECT_TRUE(sampled(tolerances, marks_of(tolerances, base)));
        }
    }
}

TEST(DecisionTest, Wide)
{
    for (const Tolerances &tolerances : wide_bands())
    {
        EXPECT_TRUE(sampled(tolerances, kWideMarks));
    }
}

TEST(DecisionTest, Decided)
{
    const Tolerances tolerances = default_tolerances();
    const Interval low(1.0, 2.0);
    const Interval high(3.0, 4.0);
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, low, high), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, high, low), 0.0, 0.0));
    EXPECT_TRUE(exact(biyos::greater(tolerances, high, low), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::equal(tolerances, low, high), 0.0, 0.0));
    EXPECT_TRUE(exact(biyos::not_equal(tolerances, high, low), 1.0, 1.0));
    const Interval overlap(1.5, 3.5);
    for (const Relation &relation : kRelations)
    {
        EXPECT_TRUE(exact(relation.enclose(tolerances, low, overlap), 0.0, 1.0)) << relation.name;
    }
}

TEST(DecisionTest, Within)
{
    const Tolerances tolerances = default_tolerances();
    const Interval point(1.0, 1.0);
    const Interval near(1.0, 1.0 + 1e-7);
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, near, point), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::greater_equal(tolerances, point, near), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::equal(tolerances, near, point), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::less(tolerances, point, near), 0.0, 0.0));
    EXPECT_TRUE(exact(biyos::greater(tolerances, near, point), 0.0, 0.0));
    EXPECT_TRUE(exact(biyos::not_equal(tolerances, near, point), 0.0, 0.0));
}

TEST(DecisionTest, Opposite)
{
    const Tolerances tolerances = default_tolerances();
    const Interval negative(-3.0, -2.0);
    const Interval positive(2.0, 3.0);
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, negative, positive), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, positive, negative), 0.0, 0.0));
}

TEST(DecisionTest, Empty)
{
    const Tolerances tolerances = default_tolerances();
    const Interval held(1.0, 2.0);
    for (const Relation &relation : kRelations)
    {
        EXPECT_TRUE(relation.enclose(tolerances, Interval::empty(), held).is_empty());
        EXPECT_TRUE(relation.enclose(tolerances, held, Interval::empty()).is_empty());
    }
}

TEST(DecisionTest, Complement)
{
    EXPECT_TRUE(exact(complement(Interval(0.0, 0.0)), 1.0, 1.0));
    EXPECT_TRUE(exact(complement(Interval(1.0, 1.0)), 0.0, 0.0));
    EXPECT_TRUE(exact(complement(Interval(0.0, 1.0)), 0.0, 1.0));
    EXPECT_TRUE(complement(Interval::empty()).is_empty());
}

TEST(DecisionTest, Straddle)
{
    const Tolerances tolerances = compare_tolerances(0.0, 1.5);
    const Interval point(-5.0, -5.0);
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, Interval(5.0, 10.0), point), 0.0, 1.0));
    const Interval opposite(5.0, 5.0);
    EXPECT_TRUE(exact(biyos::equal(tolerances, opposite, point), 0.0, 1.0));
    EXPECT_TRUE(exact(biyos::not_equal(tolerances, opposite, point), 0.0, 1.0));
    const Interval near(-0.9, -0.9);
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, Interval(0.9, 100.0), near), 0.0, 1.0));
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, Interval(0.9, 1.5), near), 0.0, 1.0));
    const Tolerances absolute = compare_tolerances(1.5, 0.0);
    const Interval small(-0.2, -0.2);
    EXPECT_TRUE(exact(biyos::less_equal(absolute, Interval(0.5, 1.2), small), 0.0, 1.0));
    const Interval tiny(-2e-16, -2e-16);
    const Tolerances unit = compare_tolerances(0.0, 1.0);
    EXPECT_TRUE(exact(biyos::less_equal(unit, Interval(1.5, 10.0), tiny), 0.0, 1.0));
}

TEST(DecisionTest, SameSign)
{
    const Tolerances tolerances = compare_tolerances(0.0, 1.5);
    const Interval zero(0.0, 0.0);
    const Interval low(-10.0, -5.0);
    const Interval high(5.0, 10.0);
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, high, zero), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, zero, low), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::less_equal(tolerances, high, Interval(20.0, 30.0)), 1.0, 1.0));
    const Interval five(5.0, 5.0);
    const Interval ten(10.0, 10.0);
    EXPECT_TRUE(exact(biyos::equal(tolerances, five, ten), 1.0, 1.0));
    EXPECT_TRUE(exact(biyos::not_equal(tolerances, five, ten), 0.0, 0.0));
}

TEST(DecisionTest, Taken)
{
    const Interval then(1.0, 2.0);
    const Interval otherwise(-3.0, -2.0);
    EXPECT_TRUE(exact(taken(Interval(1.0, 1.0), then, otherwise), 1.0, 2.0));
    EXPECT_TRUE(exact(taken(Interval(0.0, 0.0), then, otherwise), -3.0, -2.0));
    EXPECT_TRUE(exact(taken(Interval(0.0, 1.0), then, otherwise), -3.0, 2.0));
    EXPECT_TRUE(taken(Interval::empty(), then, otherwise).is_empty());
}

TEST(DecisionTest, EmptyBranch)
{
    const Interval held(1.0, 2.0);
    const Interval missed = Interval::empty();
    EXPECT_TRUE(taken(Interval(1.0, 1.0), missed, held).is_empty());
    EXPECT_TRUE(exact(taken(Interval(0.0, 0.0), missed, held), 1.0, 2.0));
    EXPECT_TRUE(exact(taken(Interval(0.0, 1.0), missed, held), 1.0, 2.0));
    EXPECT_TRUE(exact(taken(Interval(0.0, 1.0), held, missed), 1.0, 2.0));
}
