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
    Pieces,
    Refused,
};

struct Opcode
{
    int code;
    std::string_view name;
    Rule rule;
    Arity arity = Arity::One;
    Operator op = Operator::Constant;
};

const Opcode *find_opcode(Offset code) noexcept;

}
