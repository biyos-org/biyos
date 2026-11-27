#include "expr/graph/node.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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
    bool condition = false;
    bool connective = false;
};

[[noreturn]] void refuse(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "expression", reason);
}

std::string operator_name(Operator op)
{
    return "operator " + text_of(static_cast<int>(op));
}

std::optional<Shape> find_shape(Operator op) noexcept
{
    switch (op)
    {
    case Operator::Constant:
        return Shape{.payload = Payload::Constant, .least = 0, .most = 0};
    case Operator::Variable:
        return Shape{.payload = Payload::Variable, .least = 0, .most = 0};
    case Operator::Sum:
        return Shape{.payload = Payload::Coefficients, .least = 1, .most = kMany};
    case Operator::Product:
        return Shape{.payload = Payload::Constant, .least = 1, .most = kMany};
    case Operator::ConstantPower:
        return Shape{.payload = Payload::Constant, .least = 1, .most = 1};
    case Operator::SignPower:
        return Shape{.payload = Payload::Constant, .least = 1, .most = 1, .positive = true};
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
        return Shape{.payload = Payload::None, .least = 1, .most = 1};
    case Operator::PiecewiseLinear:
        return Shape{.payload = Payload::Points, .least = 1, .most = 1};
    case Operator::Division:
    case Operator::Power:
    case Operator::Atan2:
        return Shape{.payload = Payload::None, .least = 2, .most = 2};
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
        return Shape{.payload = Payload::None, .least = 2, .most = 2, .condition = true};
    case Operator::Max:
    case Operator::Min:
        return Shape{.payload = Payload::None, .least = 1, .most = kMany};
    case Operator::And:
    case Operator::Or:
        return Shape{
            .payload = Payload::None,
            .least = 1,
            .most = kMany,
            .condition = true,
            .connective = true,
        };
    case Operator::Not:
        return Shape{
            .payload = Payload::None,
            .least = 1,
            .most = 1,
            .condition = true,
            .connective = true,
        };
    }
    return std::nullopt;
}

Shape shape_of(Operator op)
{
    const std::optional<Shape> shape = find_shape(op);
    if (!shape)
    {
        refuse(operator_name(op) + " is unknown");
    }
    return *shape;
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
    case Payload::Points:
        return "a pwl";
    }
    return "an unknown payload";
}

std::string children_text(Offset n)
{
    return n_text(n, "child", "children");
}

std::string arity_text(const Shape &shape)
{
    const std::string least = children_text(shape.least);
    return shape.most == shape.least ? least : least + " or more";
}

bool positive(Real value) noexcept
{
    return !std::signbit(value) && std::fpclassify(value) != FP_ZERO;
}

}

std::string_view fault(Real value) noexcept
{
    if (std::isnan(value))
    {
        return "is NaN";
    }
    return std::signbit(value) ? "is minus infinity" : "is infinity";
}

std::optional<Operator> operator_of(int code) noexcept
{
    if (code < 0 || code > (std::numeric_limits<std::uint8_t>::max)())
    {
        return std::nullopt;
    }
    const auto op = static_cast<Operator>(code);
    if (!find_shape(op))
    {
        return std::nullopt;
    }
    return op;
}

Payload payload_of(Operator op)
{
    return shape_of(op).payload;
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

void check_operand(Operator op, Offset child, Operator found)
{
    if (shape_of(op).connective && !shape_of(found).condition)
    {
        const std::string node = "node " + text_of(child);
        refuse(operator_name(op) + " takes conditions: " + node + " is " + operator_name(found));
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

void check_constant(Real value)
{
    if (!std::isfinite(value))
    {
        refuse("constant " + std::string(fault(value)));
    }
}

void check_constant(Operator op, Real value)
{
    check_constant(value);
    if (shape_of(op).positive && !positive(value))
    {
        refuse(operator_name(op) + " takes a positive constant, not " + text_of(value));
    }
}

void check_coefficients(std::span<const Real> coefficients, Offset children)
{
    const auto n = to_offset(coefficients.size());
    if (n != children)
    {
        const std::string given = n_text(n, "coefficient", "coefficients");
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

std::string n_text(Offset n, std::string_view one, std::string_view many)
{
    return text_of(n) + " " + std::string(n == 1 ? one : many);
}

}
