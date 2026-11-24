#include "reference.hpp"

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

namespace
{

using biyos::Graph;
using biyos::Offset;
using biyos::Operator;

constexpr double kUndefined = std::numeric_limits<double>::quiet_NaN();

struct Frame
{
    const Graph &graph;
    std::span<const double> point;
    std::span<const double> values;
};

std::size_t at(Offset value)
{
    return static_cast<std::size_t>(value);
}

bool is_zero(double value)
{
    return std::fpclassify(value) == FP_ZERO;
}

double first(const Frame &frame, Offset node)
{
    return frame.values[at(frame.graph.children(node)[0])];
}

double second(const Frame &frame, Offset node)
{
    return frame.values[at(frame.graph.children(node)[1])];
}

double sum_of(const Frame &frame, Offset node)
{
    const std::span<const Offset> terms = frame.graph.children(node);
    const std::span<const double> weights = frame.graph.coefficients(node);
    double total = frame.graph.constant(node);
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        total += weights[k] * frame.values[at(terms[k])];
    }
    return total;
}

double product_of(const Frame &frame, Offset node)
{
    double total = frame.graph.constant(node);
    for (const Offset factor : frame.graph.children(node))
    {
        total *= frame.values[at(factor)];
    }
    return total;
}

double largest(const Frame &frame, Offset node)
{
    const std::span<const Offset> operands = frame.graph.children(node);
    double best = frame.values[at(operands[0])];
    for (const Offset operand : operands.subspan(1))
    {
        const double value = frame.values[at(operand)];
        if (value > best || std::isnan(value))
        {
            best = value;
        }
    }
    return best;
}

double smallest(const Frame &frame, Offset node)
{
    const std::span<const Offset> operands = frame.graph.children(node);
    double best = frame.values[at(operands[0])];
    for (const Offset operand : operands.subspan(1))
    {
        const double value = frame.values[at(operand)];
        if (value < best || std::isnan(value))
        {
            best = value;
        }
    }
    return best;
}

double quotient(double left, double right)
{
    return is_zero(right) ? kUndefined : left / right;
}

double power(double base, double exponent)
{
    return is_zero(base) && exponent < 0.0 ? kUndefined : std::pow(base, exponent);
}

double logarithm(double value)
{
    return is_zero(value) ? kUndefined : std::log(value);
}

double inverse_tanh(double value)
{
    const double result = std::atanh(value);
    return std::isinf(result) ? kUndefined : result;
}

double entropy(double value)
{
    return is_zero(value) ? 0.0 : value * std::log(value);
}

double sign_power(double value, double exponent)
{
    return std::copysign(std::pow(std::fabs(value), exponent), value);
}

double unary(Operator op, double value)
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
        break;
    }
    return kUndefined;
}

double value_of(const Frame &frame, Offset node)
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
    }
    return kUndefined;
}

}

void biyos::test::evaluate(const Graph &graph,
                           std::span<const double> point,
                           std::span<double> values)
{
    const Frame frame{.graph = graph, .point = point, .values = values};
    for (Offset node = 0; node < graph.size(); ++node)
    {
        values[at(node)] = value_of(frame, node);
    }
}
