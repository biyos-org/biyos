#include "expr/partials.hpp"

#include "expr/frame.hpp"
#include "expr/node.hpp"
#include "expr/tape.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>

namespace biyos
{

namespace
{

void product(const Instruction &instruction,
             const Frame &frame,
             std::span<double> partials) noexcept
{
    const std::span<const std::uint32_t> factors = listed(instruction, frame.operands);
    double before = constant_of(instruction);
    for (std::size_t k = 0; k < factors.size(); ++k)
    {
        partials[k] = before;
        before *= frame.slots[factors[k]];
    }
    double after = 1.0;
    for (std::size_t k = factors.size(); k > 0; --k)
    {
        partials[k - 1] *= after;
        after *= frame.slots[factors[k - 1]];
    }
}

bool beats(Operator op, double value, double best) noexcept
{
    if (std::isnan(value))
    {
        return true;
    }
    return op == Operator::Max ? value > best : value < best;
}

void extreme(const Instruction &instruction,
             const Frame &frame,
             std::span<double> partials) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, frame.operands);
    std::size_t best = 0;
    for (std::size_t k = 1; k < operands.size(); ++k)
    {
        if (beats(instruction.op, frame.slots[operands[k]], frame.slots[operands[best]]))
        {
            best = k;
        }
    }
    partials[0] = static_cast<double>(best);
}

double unsigned_zero(double value) noexcept
{
    return value + 0.0;
}

bool is_zero(double value) noexcept
{
    return std::fpclassify(value) == FP_ZERO;
}

double power_slope(double exponent, double base) noexcept
{
    if (is_zero(exponent))
    {
        return 0.0;
    }
    return exponent * std::pow(base, exponent - 1.0);
}

void quotient(const Instruction &instruction,
              const Frame &frame,
              double result,
              std::span<double> partials) noexcept
{
    const double divisor = right(instruction, frame);
    partials[0] = 1.0 / divisor;
    partials[1] = -result / divisor;
}

void power(const Instruction &instruction,
           const Frame &frame,
           double result,
           std::span<double> partials) noexcept
{
    const double base = left(instruction, frame);
    partials[0] = power_slope(right(instruction, frame), unsigned_zero(base));
    partials[1] = is_zero(result) ? 0.0 : result * std::log(base);
}

double fixed_power(const Instruction &instruction, const Frame &frame) noexcept
{
    const double base = unsigned_zero(left(instruction, frame));
    return power_slope(constant_of(instruction), base);
}

double signed_power(const Instruction &instruction, const Frame &frame) noexcept
{
    const double exponent = constant_of(instruction);
    return exponent * std::pow(std::fabs(left(instruction, frame)), exponent - 1.0);
}

double arc_slope(double value) noexcept
{
    return 1.0 / std::sqrt((1.0 - value) * (1.0 + value));
}

double atan_slope(double value) noexcept
{
    return 1.0 / (1.0 + value * value);
}

double tanh_slope(double value) noexcept
{
    const double decay = std::exp(-2.0 * std::fabs(value));
    return 4.0 * decay / ((1.0 + decay) * (1.0 + decay));
}

double acosh_slope(double value) noexcept
{
    return 1.0 / (std::sqrt(value - 1.0) * std::sqrt(value + 1.0));
}

double atanh_slope(double value) noexcept
{
    return 1.0 / ((1.0 - value) * (1.0 + value));
}

double erf_slope(double value) noexcept
{
    return 2.0 * std::numbers::inv_sqrtpi * std::exp(-value * value);
}

double sign(double value) noexcept
{
    return value < 0.0 ? -1.0 : 1.0;
}

void slopes_of(const Instruction &instruction,
               const Frame &frame,
               double result,
               std::span<double> partials) noexcept
{
    switch (instruction.op)
    {
    case Operator::Constant:
    case Operator::Variable:
        return;
    case Operator::Sum:
        partials[0] = 1.0;
        return;
    case Operator::Negation:
        partials[0] = -1.0;
        return;
    case Operator::Product:
        product(instruction, frame, partials);
        return;
    case Operator::Division:
        quotient(instruction, frame, result, partials);
        return;
    case Operator::ConstantPower:
        partials[0] = fixed_power(instruction, frame);
        return;
    case Operator::Power:
        power(instruction, frame, result, partials);
        return;
    case Operator::Exp:
        partials[0] = result;
        return;
    case Operator::Log:
        partials[0] = 1.0 / unsigned_zero(left(instruction, frame));
        return;
    case Operator::Sqrt:
        partials[0] = 0.5 / unsigned_zero(result);
        return;
    case Operator::Sin:
        partials[0] = std::cos(left(instruction, frame));
        return;
    case Operator::Cos:
        partials[0] = -std::sin(left(instruction, frame));
        return;
    case Operator::Tan:
        partials[0] = 1.0 + result * result;
        return;
    case Operator::Abs:
        partials[0] = sign(left(instruction, frame));
        return;
    case Operator::Max:
    case Operator::Min:
        extreme(instruction, frame, partials);
        return;
    case Operator::Asin:
        partials[0] = arc_slope(left(instruction, frame));
        return;
    case Operator::Acos:
        partials[0] = -arc_slope(left(instruction, frame));
        return;
    case Operator::Atan:
        partials[0] = atan_slope(left(instruction, frame));
        return;
    case Operator::Sinh:
        partials[0] = std::cosh(left(instruction, frame));
        return;
    case Operator::Cosh:
        partials[0] = std::sinh(left(instruction, frame));
        return;
    case Operator::Tanh:
        partials[0] = tanh_slope(left(instruction, frame));
        return;
    case Operator::Asinh:
        partials[0] = 1.0 / std::hypot(left(instruction, frame), 1.0);
        return;
    case Operator::Acosh:
        partials[0] = acosh_slope(left(instruction, frame));
        return;
    case Operator::Atanh:
        partials[0] = atanh_slope(left(instruction, frame));
        return;
    case Operator::Entropy:
        partials[0] = std::log(left(instruction, frame)) + 1.0;
        return;
    case Operator::SignPower:
        partials[0] = signed_power(instruction, frame);
        return;
    case Operator::Erf:
        partials[0] = erf_slope(left(instruction, frame));
        return;
    }
}

}

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

void partials_of(const Instruction &instruction,
                 const Frame &frame,
                 double result,
                 std::span<double> partials) noexcept
{
    slopes_of(instruction, frame, result, partials);
    if (std::isnan(result))
    {
        std::ranges::fill(partials, (std::numeric_limits<double>::quiet_NaN)());
    }
}

std::size_t chosen(std::span<const double> partials) noexcept
{
    return static_cast<std::size_t>(partials[0]);
}

}
