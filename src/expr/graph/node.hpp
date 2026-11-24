#pragma once

#include "biyos/biyos.h"
#include "core/index.hpp"

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
};

enum class Payload
{
    None,
    Constant,
    Variable,
    Coefficients,
};

std::optional<Operator> operator_of(int code) noexcept;
Payload payload_of(Operator op);
void check_shape(Operator op, Payload payload, Offset children);
void check_child(Offset child, Offset size);
void check_variable(Index variable);
void check_constant(double value);
void check_constant(Operator op, double value);
void check_coefficients(std::span<const double> coefficients, Offset children);
std::string n_text(Offset n, std::string_view one, std::string_view many);

}
