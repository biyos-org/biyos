#pragma once

#include "biyos/biyos.h"
#include "expr/graph/node.hpp"

namespace biyos::test
{

struct Arity
{
    int least;
    bool listed;
    bool conditions = false;
    bool guarded = false;
};

constexpr int kTail = BIYOS_OP_IF - BIYOS_OP_ATAN2 + 1;

constexpr int operator_count(int first)
{
    return BIYOS_OP_ERF - first + 1 + kTail;
}

constexpr int operator_code(int first, int index)
{
    const int code = first + index;
    return code > BIYOS_OP_ERF ? BIYOS_OP_ATAN2 + code - BIYOS_OP_ERF - 1 : code;
}

constexpr Arity arity_of(Operator op)
{
    switch (op)
    {
    case Operator::Constant:
    case Operator::Variable:
        return {.least = 0, .listed = false};
    case Operator::Sum:
    case Operator::Product:
    case Operator::Max:
    case Operator::Min:
        return {.least = 1, .listed = true};
    case Operator::And:
    case Operator::Or:
        return {.least = 1, .listed = true, .conditions = true};
    case Operator::Negation:
    case Operator::ConstantPower:
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
    case Operator::SignPower:
    case Operator::Erf:
    case Operator::PiecewiseLinear:
        return {.least = 1, .listed = false};
    case Operator::Not:
        return {.least = 1, .listed = false, .conditions = true};
    case Operator::Division:
    case Operator::Power:
    case Operator::Atan2:
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
        return {.least = 2, .listed = false};
    case Operator::If:
        return {.least = 3, .listed = false, .guarded = true};
    }
    return {.least = 0, .listed = false};
}

constexpr bool takes_condition(Operator op, int k)
{
    const Arity children = arity_of(op);
    return children.conditions || (children.guarded && k == 0);
}

}
