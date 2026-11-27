#pragma once

#include "biyos/biyos.h"
#include "model/model.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace biyos::native
{

template <typename Value> struct Word
{
    std::string_view text;
    Value value;
};

template <typename Value> Word(std::string_view, Value) -> Word<Value>;

inline constexpr std::array kVariableWords{
    Word{"cont", VariableKind::Continuous},
    Word{"bin", VariableKind::Binary},
    Word{"int", VariableKind::Integer},
    Word{"semicont", VariableKind::SemiContinuous},
    Word{"semiint", VariableKind::SemiInteger},
};

inline constexpr std::array kSenseWords{
    Word{"min", ObjectiveSense::Minimize},
    Word{"max", ObjectiveSense::Maximize},
};

inline constexpr std::array kSetWords{
    Word{"one", SpecialOrderedSetKind::One},
    Word{"two", SpecialOrderedSetKind::Two},
};

inline constexpr std::array kConeWords{
    Word{"nonneg", ConeKind::Nonnegative},
    Word{"soc", ConeKind::SecondOrder},
    Word{"rsoc", ConeKind::Rotated},
    Word{"exp", ConeKind::Exponential},
    Word{"dexp", ConeKind::DualExponential},
    Word{"pow", ConeKind::Power},
    Word{"dpow", ConeKind::DualPower},
    Word{"psd", ConeKind::Semidefinite},
};

inline constexpr std::array kOperatorWords{
    Word{"const", BIYOS_OP_CONST}, Word{"var", BIYOS_OP_VAR},     Word{"sum", BIYOS_OP_SUM},
    Word{"prod", BIYOS_OP_PROD},   Word{"neg", BIYOS_OP_NEG},     Word{"div", BIYOS_OP_DIV},
    Word{"cpow", BIYOS_OP_CPOW},   Word{"pow", BIYOS_OP_POW},     Word{"exp", BIYOS_OP_EXP},
    Word{"log", BIYOS_OP_LOG},     Word{"sqrt", BIYOS_OP_SQRT},   Word{"sin", BIYOS_OP_SIN},
    Word{"cos", BIYOS_OP_COS},     Word{"tan", BIYOS_OP_TAN},     Word{"abs", BIYOS_OP_ABS},
    Word{"max", BIYOS_OP_MAX},     Word{"min", BIYOS_OP_MIN},     Word{"asin", BIYOS_OP_ASIN},
    Word{"acos", BIYOS_OP_ACOS},   Word{"atan", BIYOS_OP_ATAN},   Word{"sinh", BIYOS_OP_SINH},
    Word{"cosh", BIYOS_OP_COSH},   Word{"tanh", BIYOS_OP_TANH},   Word{"asinh", BIYOS_OP_ASINH},
    Word{"acosh", BIYOS_OP_ACOSH}, Word{"atanh", BIYOS_OP_ATANH}, Word{"ent", BIYOS_OP_ENT},
    Word{"spow", BIYOS_OP_SPOW},   Word{"erf", BIYOS_OP_ERF},     Word{"pwl", BIYOS_OP_PWL},
    Word{"keep", BIYOS_OP_KEEP},   Word{"ref", BIYOS_OP_REF},     Word{"atan2", BIYOS_OP_ATAN2},
    Word{"le", BIYOS_OP_LE},       Word{"ge", BIYOS_OP_GE},       Word{"eq", BIYOS_OP_EQ},
    Word{"lt", BIYOS_OP_LT},       Word{"gt", BIYOS_OP_GT},       Word{"ne", BIYOS_OP_NE},
    Word{"and", BIYOS_OP_AND},     Word{"or", BIYOS_OP_OR},       Word{"not", BIYOS_OP_NOT},
    Word{"if", BIYOS_OP_IF},
};

template <typename Value, std::size_t N>
std::string_view word_of(Value value, const std::array<Word<Value>, N> &words) noexcept
{
    const auto found = std::ranges::find(words, value, &Word<Value>::value);
    return found == words.end() ? std::string_view() : found->text;
}

}
