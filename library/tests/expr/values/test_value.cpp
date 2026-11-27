#include "core/numbers.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"
#include "refusal.hpp"
#include "sweep.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Bits;
using biyos::forward;
using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::Tape;
using biyos::to_size;
using biyos::value_at;
using biyos::Workspace;
using biyos::test::binary;
using biyos::test::constant;
using biyos::test::every_operator;
using biyos::test::kBudgets;
using biyos::test::kRoots;
using biyos::test::listed;
using biyos::test::picks;
using biyos::test::power;
using biyos::test::Random;
using biyos::test::random_graph;
using biyos::test::random_point;
using biyos::test::refusal;
using biyos::test::sign_power;
using biyos::test::unary;
using biyos::test::values_of;

using Point = std::array<Real, 4>;

constexpr Real kInfinity = (std::numeric_limits<Real>::infinity)();
constexpr Real kNan = (std::numeric_limits<Real>::quiet_NaN)();

bool is_nan(Real value)
{
    return std::isnan(value);
}

Bits bits(Real value)
{
    return std::bit_cast<Bits>(value);
}

void expect_same(Real found, Real wanted)
{
    if (std::isnan(wanted))
    {
        EXPECT_TRUE(std::isnan(found)) << found;
        return;
    }
    EXPECT_DOUBLE_EQ(found, wanted);
}

void expect_values(const std::vector<Real> &found, const std::vector<Real> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        SCOPED_TRACE(k);
        expect_same(found[k], wanted[k]);
    }
}

Real entropy_of(Real value)
{
    if (std::isnan(value) || value < 0.0)
    {
        return kNan;
    }
    return value > 0.0 ? value * std::log(value) : 0.0;
}

Real signed_power(Real value, Real exponent)
{
    return std::copysign(std::pow(std::fabs(value), exponent), value);
}

std::vector<Real> cmath_values(const Point &point)
{
    const Real x = point[0];
    const Real y = point[1];
    const Real z = point[2];
    const Real w = point[3];
    return {
        2.5,
        x,
        1.5 + 2.0 * x + -3.0 * y,
        2.0 * x * y * z,
        -z,
        x / y,
        std::pow(x, 2.5),
        std::pow(x, y),
        std::exp(z),
        std::log(x),
        std::sqrt(y),
        std::sin(z),
        std::cos(z),
        std::tan(x),
        std::fabs(z),
        (std::max)({x, y, z}),
        (std::min)({x, y, z}),
        std::asin(w),
        std::acos(w),
        std::atan(z),
        std::sinh(z),
        std::cosh(z),
        std::tanh(z),
        std::asinh(z),
        std::acosh(y),
        std::atanh(w),
        x * std::log(x),
        std::copysign(std::pow(std::fabs(z), 1.5), z),
        std::erf(z),
        y < 0.25 ? 2.0 - 2.0 * (y + 1.0) : -0.5 + 1.2 * (y - 0.25),
        std::atan2(z, x),
    };
}

class Reference
{
  public:
    Reference(const Graph &graph, std::span<const Real> point)
        : graph_(graph), point_(point), values_(to_size(graph.size())),
          done_(to_size(graph.size()), false)
    {
    }

    Real value(Offset node)
    {
        if (!done_[to_size(node)])
        {
            values_[to_size(node)] = compute(node);
            done_[to_size(node)] = true;
        }
        return values_[to_size(node)];
    }

  private:
    Real compute(Offset node);
    Real sum(Offset node);
    Real product(Offset node);
    Real extreme(Offset node, Operator op);
    Real child(Offset node, std::size_t k);

    const Graph &graph_;
    std::span<const Real> point_;
    std::vector<Real> values_;
    std::vector<bool> done_;
};

Real Reference::compute(Offset node)
{
    const Operator op = graph_.op(node);
    switch (op)
    {
    case Operator::Constant:
        return graph_.constant(node);
    case Operator::Variable:
        return point_[static_cast<std::size_t>(graph_.variable(node))];
    case Operator::Sum:
        return sum(node);
    case Operator::Product:
        return product(node);
    case Operator::Negation:
        return -child(node, 0);
    case Operator::Division:
        return child(node, 0) / child(node, 1);
    case Operator::ConstantPower:
        return std::pow(child(node, 0), graph_.constant(node));
    case Operator::Power:
        return std::pow(child(node, 0), child(node, 1));
    case Operator::Exp:
        return std::exp(child(node, 0));
    case Operator::Log:
        return std::log(child(node, 0));
    case Operator::Sqrt:
        return std::sqrt(child(node, 0));
    case Operator::Sin:
        return std::sin(child(node, 0));
    case Operator::Cos:
        return std::cos(child(node, 0));
    case Operator::Tan:
        return std::tan(child(node, 0));
    case Operator::Abs:
        return std::fabs(child(node, 0));
    case Operator::Max:
    case Operator::Min:
        return extreme(node, op);
    case Operator::Asin:
        return std::asin(child(node, 0));
    case Operator::Acos:
        return std::acos(child(node, 0));
    case Operator::Atan:
        return std::atan(child(node, 0));
    case Operator::Sinh:
        return std::sinh(child(node, 0));
    case Operator::Cosh:
        return std::cosh(child(node, 0));
    case Operator::Tanh:
        return std::tanh(child(node, 0));
    case Operator::Asinh:
        return std::asinh(child(node, 0));
    case Operator::Acosh:
        return std::acosh(child(node, 0));
    case Operator::Atanh:
        return std::atanh(child(node, 0));
    case Operator::Entropy:
        return entropy_of(child(node, 0));
    case Operator::SignPower:
        return signed_power(child(node, 0), graph_.constant(node));
    case Operator::Erf:
        return std::erf(child(node, 0));
    case Operator::PiecewiseLinear:
        return value_at(graph_.points().piecewise_linear(graph_.piecewise_linear(node)),
                        child(node, 0));
    case Operator::Atan2:
        return std::atan2(child(node, 0) + 0.0, child(node, 1) + 0.0);
    }
    return kNan;
}

Real Reference::sum(Offset node)
{
    const std::span<const Real> weights = graph_.coefficients(node);
    Real total = graph_.constant(node);
    for (std::size_t k = 0; k < weights.size(); ++k)
    {
        total += weights[k] * child(node, k);
    }
    return total;
}

Real Reference::product(Offset node)
{
    Real total = graph_.constant(node);
    for (std::size_t k = 0; k < graph_.children(node).size(); ++k)
    {
        total *= child(node, k);
    }
    return total;
}

Real Reference::extreme(Offset node, Operator op)
{
    std::vector<Real> values;
    for (std::size_t k = 0; k < graph_.children(node).size(); ++k)
    {
        values.push_back(child(node, k));
    }
    if (std::ranges::any_of(values, is_nan))
    {
        return kNan;
    }
    return op == Operator::Max ? (std::ranges::max)(values) : (std::ranges::min)(values);
}

Real Reference::child(Offset node, std::size_t k)
{
    return value(graph_.children(node)[k]);
}

void expect_reference(const Graph &graph,
                      std::span<const Offset> roots,
                      std::span<const Real> point,
                      const std::vector<Real> &values)
{
    Reference reference(graph, point);
    for (std::size_t k = 0; k < roots.size(); ++k)
    {
        SCOPED_TRACE(k);
        expect_same(values[k], reference.value(roots[k]));
    }
}

}

TEST(ValueTest, Operators)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    const Tape tape(graph, roots);
    const std::array points{
        Point{0.7, 1.9, -1.3, 0.3},
        Point{2.5, 1.25, 4.0, -0.8},
        Point{1.1, 3.0, -0.6, 0.95},
    };
    for (const Point &point : points)
    {
        expect_values(values_of(tape, point), cmath_values(point));
    }
}

TEST(ValueTest, Domain)
{
    Graph graph;
    const Offset negative = graph.add_variable(0);
    const Offset zero = graph.add_variable(1);
    const Offset one = constant(graph, 1.0);
    const Offset half = constant(graph, 0.5);
    const Offset large = constant(graph, 1000.0);
    const Offset beyond = constant(graph, 1.5);
    const Offset below = constant(graph, -1.5);
    const Offset minus_zero = constant(graph, -0.0);
    const Offset nan = unary(graph, Operator::Log, negative);
    const std::vector<Offset> roots{
        unary(graph, Operator::Log, negative),
        unary(graph, Operator::Log, zero),
        unary(graph, Operator::Sqrt, negative),
        binary(graph, Operator::Division, one, zero),
        binary(graph, Operator::Division, negative, zero),
        binary(graph, Operator::Division, zero, zero),
        power(graph, zero, -1.0),
        power(graph, negative, 0.5),
        binary(graph, Operator::Power, negative, half),
        unary(graph, Operator::Exp, large),
        unary(graph, Operator::Asin, beyond),
        unary(graph, Operator::Asin, one),
        unary(graph, Operator::Acos, below),
        unary(graph, Operator::Acos, negative),
        unary(graph, Operator::Acosh, half),
        unary(graph, Operator::Acosh, one),
        unary(graph, Operator::Atanh, one),
        unary(graph, Operator::Atanh, negative),
        unary(graph, Operator::Atanh, beyond),
        unary(graph, Operator::Entropy, negative),
        unary(graph, Operator::Entropy, zero),
        unary(graph, Operator::Entropy, one),
        sign_power(graph, zero, 2.0),
        sign_power(graph, negative, 3.0),
        unary(graph, Operator::Sinh, large),
        unary(graph, Operator::Cosh, large),
        sign_power(graph, minus_zero, 2.0),
        sign_power(graph, nan, 2.0),
    };
    const Tape tape(graph, roots);
    const std::array point{-1.0, 0.0};
    const std::vector<Real> found = values_of(tape, point);
    expect_values(found,
                  {
                      std::log(-1.0),
                      std::log(0.0),
                      std::sqrt(-1.0),
                      kInfinity,
                      -kInfinity,
                      kNan,
                      std::pow(0.0, -1.0),
                      std::pow(-1.0, 0.5),
                      std::pow(-1.0, 0.5),
                      std::exp(1000.0),
                      std::asin(1.5),
                      std::asin(1.0),
                      std::acos(-1.5),
                      std::acos(-1.0),
                      std::acosh(0.5),
                      std::acosh(1.0),
                      std::atanh(1.0),
                      std::atanh(-1.0),
                      std::atanh(1.5),
                      kNan,
                      0.0,
                      0.0,
                      0.0,
                      -1.0,
                      std::sinh(1000.0),
                      std::cosh(1000.0),
                      0.0,
                      kNan,
                  });
    EXPECT_EQ(std::ranges::count_if(found, is_nan), 11);
    EXPECT_DOUBLE_EQ(found[1], -kInfinity);
    EXPECT_DOUBLE_EQ(found[6], kInfinity);
    EXPECT_DOUBLE_EQ(found[9], kInfinity);
    EXPECT_DOUBLE_EQ(found[15], 0.0);
    EXPECT_DOUBLE_EQ(found[16], kInfinity);
    EXPECT_DOUBLE_EQ(found[17], -kInfinity);
    EXPECT_EQ(bits(found[20]), bits(0.0));
    EXPECT_DOUBLE_EQ(found[24], kInfinity);
    EXPECT_DOUBLE_EQ(found[25], kInfinity);
}

TEST(ValueTest, Extrema)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset z = graph.add_variable(2);
    const Offset nan = unary(graph, Operator::Log, y);
    const Offset minus = constant(graph, -0.0);
    const Offset plus = constant(graph, 0.0);
    const std::vector<Offset> roots{
        listed(graph, Operator::Max, {x, y, z}),
        listed(graph, Operator::Min, {x, y, z}),
        listed(graph, Operator::Max, {y}),
        listed(graph, Operator::Min, {z}),
        listed(graph, Operator::Max, {x, x}),
        listed(graph, Operator::Max, {nan, z}),
        listed(graph, Operator::Max, {z, nan}),
        listed(graph, Operator::Min, {x, nan, y}),
        listed(graph, Operator::Min, {nan}),
        listed(graph, Operator::Max, {minus, plus}),
        listed(graph, Operator::Min, {plus, minus}),
    };
    const Tape tape(graph, roots);
    const std::array point{0.5, -2.0, 3.0};
    const std::vector<Real> found = values_of(tape, point);
    expect_values(found, {3.0, -2.0, -2.0, 3.0, 0.5, kNan, kNan, kNan, kNan, -0.0, 0.0});
    EXPECT_EQ(bits(found[9]), bits(-0.0));
    EXPECT_EQ(bits(found[10]), bits(0.0));
}

TEST(ValueTest, Seeded)
{
    Offset pinned = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        SCOPED_TRACE(seed);
        Random random(seed);
        const Graph graph = random_graph(random);
        const std::vector<Offset> roots = picks(random, graph, kRoots);
        const Tape tape(graph, roots, kBudgets[random.below(kBudgets.size())]);
        pinned += tape.pinned();
        Workspace workspace(tape);
        std::vector<Real> values(roots.size());
        for (int k = 0; k < 3; ++k)
        {
            const std::vector<Real> point = random_point(random);
            forward(tape, point, workspace, values);
            expect_reference(graph, roots, point, values);
        }
    }
    EXPECT_GT(pinned, 0);
}

TEST(ValueTest, Sizes)
{
    Graph graph;
    const Offset y = graph.add_variable(1);
    const Offset shared = unary(graph, Operator::Exp, y);
    const Offset sine = unary(graph, Operator::Sin, shared);
    const Offset cosine = unary(graph, Operator::Cos, shared);
    const Offset ratio = binary(graph, Operator::Division, sine, y);
    const Tape tape(graph, std::array{ratio, cosine}, 4);
    ASSERT_EQ(tape.pinned(), 1);
    ASSERT_EQ(tape.width(), 2U);
    Workspace workspace(tape);
    std::vector<Real> values(2);
    const std::array point{0.5, 0.25};
    const std::span<const Real> short_point = std::span(point).first(1);
    EXPECT_EQ(refusal([&] { forward(tape, short_point, workspace, values); }),
              "invalid argument: point: size 1 does not cover variable 1");
    std::vector<Real> more(3);
    EXPECT_EQ(refusal([&] { forward(tape, point, workspace, more); }),
              "invalid argument: values: size 3, not the root count 2");

    const std::string other = "invalid argument: workspace: sized for another tape";
    Workspace empty(Tape(graph, {}));
    EXPECT_EQ(refusal([&] { forward(tape, point, empty, values); }), other);
    const Tape whole(graph, std::span(&ratio, 1));
    ASSERT_EQ(whole.pinned(), 0);
    ASSERT_EQ(whole.width(), tape.width());
    Workspace unshared(whole);
    EXPECT_EQ(refusal([&] { forward(tape, point, unshared, values); }), other);
    const Tape thin(graph, std::array{sine, cosine}, 3);
    ASSERT_EQ(thin.pinned(), tape.pinned());
    ASSERT_LT(thin.width(), tape.width());
    Workspace few_slots(thin);
    EXPECT_EQ(refusal([&] { forward(tape, point, few_slots, values); }), other);
}
