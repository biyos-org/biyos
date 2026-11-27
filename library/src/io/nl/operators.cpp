#include "io/nl/operators.hpp"

#include "core/numbers.hpp"
#include "expr/graph/node.hpp"

#include <algorithm>
#include <array>

namespace biyos::nl
{

namespace
{

constexpr std::array kOpcodes{
    Opcode{kPlusCode, "plus", Rule::Plus, Arity::Two},
    Opcode{kMinusCode, "minus", Rule::Minus, Arity::Two},
    Opcode{kTimesCode, "mult", Rule::Times, Arity::Two},
    Opcode{3, "div", Rule::Same, Arity::Two, Operator::Division},
    Opcode{4, "rem", Rule::Refused},
    Opcode{kPowerCode, "pow", Rule::Raise, Arity::Two},
    Opcode{6, "less", Rule::Less, Arity::Two},
    Opcode{11, "min", Rule::Same, Arity::Many, Operator::Min},
    Opcode{12, "max", Rule::Same, Arity::Many, Operator::Max},
    Opcode{13, "floor", Rule::Refused},
    Opcode{14, "ceil", Rule::Refused},
    Opcode{15, "abs", Rule::Same, Arity::One, Operator::Abs},
    Opcode{16, "neg", Rule::Same, Arity::One, Operator::Negation},
    Opcode{20, "or", Rule::Same, Arity::Two, Operator::Or},
    Opcode{21, "and", Rule::Same, Arity::Two, Operator::And},
    Opcode{22, "lt", Rule::Same, Arity::Two, Operator::Less},
    Opcode{23, "le", Rule::Same, Arity::Two, Operator::LessEqual},
    Opcode{24, "eq", Rule::Same, Arity::Two, Operator::Equal},
    Opcode{28, "ge", Rule::Same, Arity::Two, Operator::GreaterEqual},
    Opcode{29, "gt", Rule::Same, Arity::Two, Operator::Greater},
    Opcode{30, "ne", Rule::Same, Arity::Two, Operator::NotEqual},
    Opcode{34, "not", Rule::Same, Arity::One, Operator::Not},
    Opcode{35, "if", Rule::Same, Arity::Three, Operator::If},
    Opcode{37, "tanh", Rule::Same, Arity::One, Operator::Tanh},
    Opcode{38, "tan", Rule::Same, Arity::One, Operator::Tan},
    Opcode{39, "sqrt", Rule::Same, Arity::One, Operator::Sqrt},
    Opcode{40, "sinh", Rule::Same, Arity::One, Operator::Sinh},
    Opcode{41, "sin", Rule::Same, Arity::One, Operator::Sin},
    Opcode{kLog10Code, "log10", Rule::Log10},
    Opcode{43, "log", Rule::Same, Arity::One, Operator::Log},
    Opcode{44, "exp", Rule::Same, Arity::One, Operator::Exp},
    Opcode{45, "cosh", Rule::Same, Arity::One, Operator::Cosh},
    Opcode{46, "cos", Rule::Same, Arity::One, Operator::Cos},
    Opcode{47, "atanh", Rule::Same, Arity::One, Operator::Atanh},
    Opcode{48, "atan2", Rule::Same, Arity::Two, Operator::Atan2},
    Opcode{49, "atan", Rule::Same, Arity::One, Operator::Atan},
    Opcode{50, "asinh", Rule::Same, Arity::One, Operator::Asinh},
    Opcode{51, "asin", Rule::Same, Arity::One, Operator::Asin},
    Opcode{52, "acosh", Rule::Same, Arity::One, Operator::Acosh},
    Opcode{53, "acos", Rule::Same, Arity::One, Operator::Acos},
    Opcode{kSumCode, "sum", Rule::Sum, Arity::Many},
    Opcode{55, "intdiv", Rule::Refused},
    Opcode{56, "precision", Rule::Refused},
    Opcode{57, "round", Rule::Refused},
    Opcode{58, "trunc", Rule::Refused},
    Opcode{59, "count", Rule::Refused},
    Opcode{60, "numberof", Rule::Refused},
    Opcode{61, "numberofs", Rule::Refused},
    Opcode{kPiecesCode, "plterm", Rule::Pieces},
    Opcode{65, "ifs", Rule::Refused},
    Opcode{kAndListCode, "and", Rule::Same, Arity::Many, Operator::And},
    Opcode{kOrListCode, "or", Rule::Same, Arity::Many, Operator::Or},
    Opcode{72, "implies", Rule::Refused},
    Opcode{73, "iff", Rule::Refused},
    Opcode{74, "alldiff", Rule::Refused},
};

}

const Opcode *find_opcode(Offset code) noexcept
{
    const auto found = std::ranges::find(kOpcodes, code, &Opcode::code);
    return found == kOpcodes.end() ? nullptr : &*found;
}

int code_of(Operator op) noexcept
{
    for (const Opcode &opcode : kOpcodes)
    {
        if (opcode.rule == Rule::Same && opcode.op == op)
        {
            return opcode.code;
        }
    }
    return -1;
}

}
