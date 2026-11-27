#pragma once

#include "core/numbers.hpp"
#include "expr/graph/node.hpp"

#include <string_view>

namespace biyos::nl
{

enum class Arity
{
    One,
    Two,
    Three,
    Many,
};

enum class Rule
{
    Same,
    Plus,
    Minus,
    Sum,
    Times,
    Raise,
    Log10,
    Less,
    Pieces,
    Refused,
};

constexpr int kPlusCode = 0;
constexpr int kMinusCode = 1;
constexpr int kTimesCode = 2;
constexpr int kPowerCode = 5;
constexpr int kLog10Code = 42;
constexpr int kSumCode = 54;
constexpr int kPiecesCode = 64;
constexpr int kAndListCode = 70;
constexpr int kOrListCode = 71;

struct Opcode
{
    int code;
    std::string_view name;
    Rule rule;
    Arity arity = Arity::One;
    Operator op = Operator::Constant;
};

const Opcode *find_opcode(Offset code) noexcept;
int code_of(Operator op) noexcept;

}
