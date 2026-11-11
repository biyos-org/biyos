#include "expr/partials.hpp"

#include "expr/node.hpp"
#include "expr/tape.hpp"

#include <cstdint>

namespace biyos
{

std::uint32_t partial_count(const Instruction &instruction) noexcept
{
    switch (instruction.op)
    {
    case Operator::Constant:
    case Operator::Variable:
        return 0;
    case Operator::Product:
        return instruction.second - instruction.first;
    case Operator::Division:
    case Operator::Power:
        return 2;
    case Operator::Sum:
    case Operator::Negation:
    case Operator::ConstantPower:
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
        return 1;
    }
    return 0;
}

}
