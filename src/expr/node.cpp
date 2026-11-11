#include "expr/node.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr Offset kMany = (std::numeric_limits<Offset>::max)();

struct Shape
{
    Payload payload;
    Offset least;
    Offset most;
    bool positive = false;
};

[[noreturn]] void refuse(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "expression", reason);
}

std::string operator_name(Operator op)
{
    return "operator " + text_of(static_cast<int>(op));
}

Shape shape_of(Operator op)
{
    switch (op)
    {
    case Operator::Constant:
        return {.payload = Payload::Constant, .least = 0, .most = 0};
    case Operator::Variable:
        return {.payload = Payload::Variable, .least = 0, .most = 0};
    case Operator::Sum:
        return {.payload = Payload::Coefficients, .least = 1, .most = kMany};
    case Operator::Product:
        return {.payload = Payload::Constant, .least = 1, .most = kMany};
    case Operator::ConstantPower:
        return {.payload = Payload::Constant, .least = 1, .most = 1};
    case Operator::SignPower:
        return {.payload = Payload::Constant, .least = 1, .most = 1, .positive = true};
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
        return {.payload = Payload::None, .least = 1, .most = 1};
    case Operator::Division:
    case Operator::Power:
        return {.payload = Payload::None, .least = 2, .most = 2};
    case Operator::Max:
    case Operator::Min:
        return {.payload = Payload::None, .least = 1, .most = kMany};
    }
    refuse(operator_name(op) + " is unknown");
}

std::string_view payload_name(Payload payload) noexcept
{
    switch (payload)
    {
    case Payload::None:
        return "no payload";
    case Payload::Constant:
        return "a constant";
    case Payload::Variable:
        return "a variable";
    case Payload::Coefficients:
        return "coefficients";
    }
    return "an unknown payload";
}

std::string children_text(Offset count)
{
    return count_text(count, "child", "children");
}

std::string arity_text(const Shape &shape)
{
    const std::string least = children_text(shape.least);
    return shape.most == shape.least ? least : least + " or more";
}

std::string_view fault(double value) noexcept
{
    if (std::isnan(value))
    {
        return "is NaN";
    }
    return std::signbit(value) ? "is minus infinity" : "is infinity";
}

bool positive(double value) noexcept
{
    return !std::signbit(value) && std::fpclassify(value) != FP_ZERO;
}

}

void check_shape(Operator op, Payload payload, Offset children)
{
    const Shape shape = shape_of(op);
    if (shape.payload != payload)
    {
        refuse(operator_name(op) + " takes " + std::string(payload_name(shape.payload)));
    }
    if (children < shape.least || children > shape.most)
    {
        const std::string wanted = arity_text(shape);
        refuse(operator_name(op) + " takes " + wanted + ", not " + text_of(children));
    }
}

void check_child(Offset child, Offset size)
{
    if (child < 0 || child >= size)
    {
        refuse("node " + text_of(child) + " is unknown");
    }
}

void check_variable(Index variable)
{
    if (variable < 0)
    {
        refuse("variable " + text_of(variable) + " is unknown");
    }
}

void check_constant(double value)
{
    if (!std::isfinite(value))
    {
        refuse("constant " + std::string(fault(value)));
    }
}

void check_constant(Operator op, double value)
{
    check_constant(value);
    if (shape_of(op).positive && !positive(value))
    {
        refuse(operator_name(op) + " takes a positive constant, not " + text_of(value));
    }
}

void check_coefficients(std::span<const double> coefficients, Offset children)
{
    const auto count = static_cast<Offset>(coefficients.size());
    if (count != children)
    {
        const std::string given = count_text(count, "coefficient", "coefficients");
        refuse(children_text(children) + " and " + given);
    }
    for (std::size_t k = 0; k < coefficients.size(); ++k)
    {
        if (!std::isfinite(coefficients[k]))
        {
            refuse("coefficient " + text_of(k) + " " + std::string(fault(coefficients[k])));
        }
    }
}

std::string count_text(Offset count, std::string_view one, std::string_view many)
{
    return text_of(count) + " " + std::string(count == 1 ? one : many);
}

}
