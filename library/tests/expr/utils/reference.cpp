#include "reference.hpp"

#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

namespace
{

using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::to_size;
using biyos::Tolerances;

constexpr Real kUndefined = std::numeric_limits<Real>::quiet_NaN();

struct Frame
{
    const Graph &graph;
    std::span<const Real> point;
    std::span<const Real> values;
};

bool is_zero(Real value)
{
    return std::fpclassify(value) == FP_ZERO;
}

Real first(const Frame &frame, Offset node)
{
    return frame.values[to_size(frame.graph.children(node)[0])];
}

Real second(const Frame &frame, Offset node)
{
    return frame.values[to_size(frame.graph.children(node)[1])];
}

Real sum_of(const Frame &frame, Offset node)
{
    const std::span<const Offset> terms = frame.graph.children(node);
    const std::span<const Real> weights = frame.graph.coefficients(node);
    Real total = frame.graph.constant(node);
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        total += weights[k] * frame.values[to_size(terms[k])];
    }
    return total;
}

Real product_of(const Frame &frame, Offset node)
{
    Real total = frame.graph.constant(node);
    for (const Offset factor : frame.graph.children(node))
    {
        total *= frame.values[to_size(factor)];
    }
    return total;
}

Real largest(const Frame &frame, Offset node)
{
    const std::span<const Offset> operands = frame.graph.children(node);
    Real best = frame.values[to_size(operands[0])];
    for (const Offset operand : operands.subspan(1))
    {
        const Real value = frame.values[to_size(operand)];
        if (value > best || std::isnan(value))
        {
            best = value;
        }
    }
    return best;
}

Real smallest(const Frame &frame, Offset node)
{
    const std::span<const Offset> operands = frame.graph.children(node);
    Real best = frame.values[to_size(operands[0])];
    for (const Offset operand : operands.subspan(1))
    {
        const Real value = frame.values[to_size(operand)];
        if (value < best || std::isnan(value))
        {
            best = value;
        }
    }
    return best;
}

Real quotient(Real left, Real right)
{
    return is_zero(right) ? kUndefined : left / right;
}

Real power(Real base, Real exponent)
{
    return is_zero(base) && exponent < 0.0 ? kUndefined : std::pow(base, exponent);
}

Real logarithm(Real value)
{
    return is_zero(value) ? kUndefined : std::log(value);
}

Real inverse_tanh(Real value)
{
    const Real result = std::atanh(value);
    return std::isinf(result) ? kUndefined : result;
}

Real entropy(Real value)
{
    return is_zero(value) ? 0.0 : value * std::log(value);
}

Real sign_power(Real value, Real exponent)
{
    return std::copysign(std::pow(std::fabs(value), exponent), value);
}

Real angle(Real y, Real x)
{
    return std::atan2(y + 0.0, x + 0.0);
}

bool holds(Operator op, Real a, Real b)
{
    static const Tolerances tolerances = biyos::test::default_tolerances();
    const bool near = tolerances.approx_equal(a, b);
    switch (op)
    {
    case Operator::LessEqual:
        return a < b || near;
    case Operator::GreaterEqual:
        return b < a || near;
    case Operator::Equal:
        return near;
    case Operator::Less:
        return a < b && !near;
    case Operator::Greater:
        return b < a && !near;
    case Operator::NotEqual:
        return !near;
    case Operator::Constant:
    case Operator::Variable:
    case Operator::Sum:
    case Operator::Product:
    case Operator::Negation:
    case Operator::Division:
    case Operator::ConstantPower:
    case Operator::Power:
    case Operator::Exp:
    case Operator::Log:
    case Operator::Sqrt:
    case Operator::Sin:
    case Operator::Cos:
    case Operator::Tan:
    case Operator::Abs:
    case Operator::Max:
    case Operator::Min:
    case Operator::Asin:
    case Operator::Acos:
    case Operator::Atan:
    case Operator::Sinh:
    case Operator::Cosh:
    case Operator::Tanh:
    case Operator::Asinh:
    case Operator::Acosh:
    case Operator::Atanh:
    case Operator::Entropy:
    case Operator::SignPower:
    case Operator::Erf:
    case Operator::PiecewiseLinear:
    case Operator::Atan2:
        break;
    }
    return false;
}

Real unary(Operator op, Real value)
{
    switch (op)
    {
    case Operator::Negation:
        return -value;
    case Operator::Exp:
        return std::exp(value);
    case Operator::Log:
        return logarithm(value);
    case Operator::Sqrt:
        return std::sqrt(value);
    case Operator::Sin:
        return std::sin(value);
    case Operator::Cos:
        return std::cos(value);
    case Operator::Tan:
        return std::tan(value);
    case Operator::Abs:
        return std::fabs(value);
    case Operator::Asin:
        return std::asin(value);
    case Operator::Acos:
        return std::acos(value);
    case Operator::Atan:
        return std::atan(value);
    case Operator::Sinh:
        return std::sinh(value);
    case Operator::Cosh:
        return std::cosh(value);
    case Operator::Tanh:
        return std::tanh(value);
    case Operator::Asinh:
        return std::asinh(value);
    case Operator::Acosh:
        return std::acosh(value);
    case Operator::Atanh:
        return inverse_tanh(value);
    case Operator::Entropy:
        return entropy(value);
    case Operator::Erf:
        return std::erf(value);
    case Operator::Constant:
    case Operator::Variable:
    case Operator::Sum:
    case Operator::Product:
    case Operator::Division:
    case Operator::ConstantPower:
    case Operator::Power:
    case Operator::Max:
    case Operator::Min:
    case Operator::SignPower:
    case Operator::PiecewiseLinear:
    case Operator::Atan2:
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
        break;
    }
    return kUndefined;
}

Real value_of(const Frame &frame, Offset node)
{
    const Graph &graph = frame.graph;
    switch (graph.op(node))
    {
    case Operator::Constant:
        return graph.constant(node);
    case Operator::Variable:
        return frame.point[static_cast<std::size_t>(graph.variable(node))];
    case Operator::Sum:
        return sum_of(frame, node);
    case Operator::Product:
        return product_of(frame, node);
    case Operator::Division:
        return quotient(first(frame, node), second(frame, node));
    case Operator::ConstantPower:
        return power(first(frame, node), graph.constant(node));
    case Operator::Power:
        return power(first(frame, node), second(frame, node));
    case Operator::Max:
        return largest(frame, node);
    case Operator::Min:
        return smallest(frame, node);
    case Operator::SignPower:
        return sign_power(first(frame, node), graph.constant(node));
    case Operator::Negation:
    case Operator::Exp:
    case Operator::Log:
    case Operator::Sqrt:
    case Operator::Sin:
    case Operator::Cos:
    case Operator::Tan:
    case Operator::Abs:
    case Operator::Asin:
    case Operator::Acos:
    case Operator::Atan:
    case Operator::Sinh:
    case Operator::Cosh:
    case Operator::Tanh:
    case Operator::Asinh:
    case Operator::Acosh:
    case Operator::Atanh:
    case Operator::Entropy:
    case Operator::Erf:
        return unary(graph.op(node), first(frame, node));
    case Operator::PiecewiseLinear:
        return biyos::value_at(graph.points().piecewise_linear(graph.piecewise_linear(node)),
                               first(frame, node));
    case Operator::Atan2:
        return angle(first(frame, node), second(frame, node));
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
        return biyos::test::comparison(graph.op(node), first(frame, node), second(frame, node));
    }
    return kUndefined;
}

}

Real biyos::test::comparison(Operator op, Real a, Real b)
{
    if (std::isnan(a) || std::isnan(b))
    {
        return kUndefined;
    }
    return holds(op, a, b) ? 1.0 : 0.0;
}

void biyos::test::evaluate(const Graph &graph, std::span<const Real> point, std::span<Real> values)
{
    const Frame frame{.graph = graph, .point = point, .values = values};
    for (Offset node = 0; node < graph.size(); ++node)
    {
        values[to_size(node)] = value_of(frame, node);
    }
}
