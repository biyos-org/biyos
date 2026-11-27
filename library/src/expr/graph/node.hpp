#pragma once

#include "biyos/biyos.h"
#include "core/numbers.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

enum class Operator : std::uint8_t
{
    Constant = BIYOS_OP_CONST,
    Variable = BIYOS_OP_VAR,
    Sum = BIYOS_OP_SUM,
    Product = BIYOS_OP_PROD,
    Negation = BIYOS_OP_NEG,
    Division = BIYOS_OP_DIV,
    ConstantPower = BIYOS_OP_CPOW,
    Power = BIYOS_OP_POW,
    Exp = BIYOS_OP_EXP,
    Log = BIYOS_OP_LOG,
    Sqrt = BIYOS_OP_SQRT,
    Sin = BIYOS_OP_SIN,
    Cos = BIYOS_OP_COS,
    Tan = BIYOS_OP_TAN,
    Abs = BIYOS_OP_ABS,
    Max = BIYOS_OP_MAX,
    Min = BIYOS_OP_MIN,
    Asin = BIYOS_OP_ASIN,
    Acos = BIYOS_OP_ACOS,
    Atan = BIYOS_OP_ATAN,
    Sinh = BIYOS_OP_SINH,
    Cosh = BIYOS_OP_COSH,
    Tanh = BIYOS_OP_TANH,
    Asinh = BIYOS_OP_ASINH,
    Acosh = BIYOS_OP_ACOSH,
    Atanh = BIYOS_OP_ATANH,
    Entropy = BIYOS_OP_ENT,
    SignPower = BIYOS_OP_SPOW,
    Erf = BIYOS_OP_ERF,
    PiecewiseLinear = BIYOS_OP_PWL,
    Atan2 = BIYOS_OP_ATAN2,
    LessEqual = BIYOS_OP_LE,
    GreaterEqual = BIYOS_OP_GE,
    Equal = BIYOS_OP_EQ,
    Less = BIYOS_OP_LT,
    Greater = BIYOS_OP_GT,
    NotEqual = BIYOS_OP_NE,
    And = BIYOS_OP_AND,
    Or = BIYOS_OP_OR,
    Not = BIYOS_OP_NOT,
    If = BIYOS_OP_IF,
};

enum class Payload
{
    None,
    Constant,
    Variable,
    Coefficients,
    Points,
};

std::string_view fault(Real value) noexcept;
std::optional<Operator> operator_of(int code) noexcept;
Payload payload_of(Operator op);
void check_shape(Operator op, Payload payload, Offset children);
void check_child(Offset child, Offset size);
void check_operand(Operator op, std::size_t k, Offset child, Operator found);
void check_variable(Index variable);
void check_constant(Real value);
void check_constant(Operator op, Real value);
void check_coefficients(std::span<const Real> coefficients, Offset children);
std::string n_text(Offset n, std::string_view one, std::string_view many);

}
