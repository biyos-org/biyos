#include "expr/evaluation/hessian/tangents.hpp"

#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/evaluation/derivatives/partials.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace biyos
{

namespace
{

struct Running
{
    Real value;
    std::span<Real> tangent;
};

std::span<Real> part(std::span<Real> items, std::size_t k, const Tangents &tangents) noexcept
{
    return row(items, k, tangents.lanes);
}

std::span<const Real> tangent(const Tangents &tangents, std::uint32_t slot) noexcept
{
    return row(tangents.slots, slot, tangents.lanes);
}

Real operand(const Frame &frame, std::uint32_t slot) noexcept
{
    return frame.slots[slot];
}

Real unsigned_zero(Real value) noexcept
{
    return value + 0.0;
}

Real cube(Real value) noexcept
{
    return value * value * value;
}

void curve(const Instruction &instruction,
           const Tangents &tangents,
           Real factor,
           std::span<Real> out) noexcept
{
    const std::span<const Real> t = tangent(tangents, instruction.first);
    for (std::size_t l = 0; l < tangents.lanes; ++l)
    {
        out[l] = scaled(t[l], factor);
    }
}

void advance(Running &running, Real x, std::span<const Real> t) noexcept
{
    for (std::size_t l = 0; l < t.size(); ++l)
    {
        running.tangent[l] = scaled(running.tangent[l], x) + scaled(t[l], running.value);
    }
    running.value *= x;
}

void product(const Instruction &instruction,
             const Frame &frame,
             const Tangents &tangents,
             std::span<Real> out) noexcept
{
    const std::span<const std::uint32_t> factors = listed(instruction, frame.operands);
    Running running{.value = constant_of(instruction), .tangent = tangents.running};
    std::ranges::fill(running.tangent, 0.0);
    for (std::size_t k = 0; k < factors.size(); ++k)
    {
        tangents.prefix[k] = running.value;
        std::ranges::copy(running.tangent, part(out, k, tangents).begin());
        advance(running, operand(frame, factors[k]), tangent(tangents, factors[k]));
    }
    running.value = 1.0;
    std::ranges::fill(running.tangent, 0.0);
    for (std::size_t k = factors.size(); k > 0; --k)
    {
        const Real prefix = tangents.prefix[k - 1];
        const std::span<Real> mine = part(out, k - 1, tangents);
        for (std::size_t l = 0; l < tangents.lanes; ++l)
        {
            mine[l] = scaled(mine[l], running.value) + scaled(running.tangent[l], prefix);
        }
        advance(running, operand(frame, factors[k - 1]), tangent(tangents, factors[k - 1]));
    }
}

Bends quotient_of(const Frame &frame, std::span<const Real> partials) noexcept
{
    const Real square = partials[0] * partials[0];
    return {.xx = 0.0, .xy = -square, .yy = 2.0 * frame.result[0] * square};
}

Bends angle_of(std::span<const Real> partials) noexcept
{
    const Real along_y = partials[0];
    const Real along_x = partials[1];
    const Real cross = 2.0 * along_y * along_x;
    return {.xx = cross, .xy = along_x * along_x - along_y * along_y, .yy = -cross};
}

void quotient(const Instruction &instruction,
              const Tangents &tangents,
              Bends bends,
              std::span<Real> out) noexcept
{
    const std::span<const Real> tx = tangent(tangents, instruction.first);
    const std::span<const Real> ty = tangent(tangents, instruction.second);
    const std::span<Real> first = part(out, 0, tangents);
    const std::span<Real> second = part(out, 1, tangents);
    for (std::size_t l = 0; l < tangents.lanes; ++l)
    {
        first[l] = scaled(ty[l], bends.xy);
        second[l] = scaled(ty[l], bends.yy) + scaled(tx[l], bends.xy);
    }
}

void pair(const Instruction &instruction,
          const Tangents &tangents,
          Bends bends,
          std::span<Real> out) noexcept
{
    const std::span<const Real> tx = tangent(tangents, instruction.first);
    const std::span<const Real> ty = tangent(tangents, instruction.second);
    const std::span<Real> first = part(out, 0, tangents);
    const std::span<Real> second = part(out, 1, tangents);
    for (std::size_t l = 0; l < tangents.lanes; ++l)
    {
        first[l] = scaled(tx[l], bends.xx) + scaled(ty[l], bends.xy);
        second[l] = scaled(tx[l], bends.xy) + scaled(ty[l], bends.yy);
    }
}

Real power_curve(Real exponent, Real base) noexcept
{
    if (!curved(exponent))
    {
        return 0.0;
    }
    return exponent * (exponent - 1.0) * std::pow(base, exponent - 2.0);
}

Real cross_curve(Real result, Real base, Real exponent) noexcept
{
    if (is_zero(result))
    {
        return 0.0;
    }
    return std::pow(base, exponent - 1.0) * (1.0 + exponent * std::log(base));
}

Real exponent_curve(Real result, Real base) noexcept
{
    if (is_zero(result))
    {
        return 0.0;
    }
    const Real log = std::log(base);
    return result * log * log;
}

Bends power_of(const Instruction &instruction, const Frame &frame) noexcept
{
    const Real base = operand(frame, instruction.first);
    const Real exponent = operand(frame, instruction.second);
    const Real x = unsigned_zero(base);
    return {.xx = power_curve(exponent, x),
            .xy = cross_curve(frame.result[0], x, exponent),
            .yy = exponent_curve(frame.result[0], base)};
}

Real sign_curve(Real exponent, Real value) noexcept
{
    if (!curved(exponent))
    {
        return 0.0;
    }
    return exponent * (exponent - 1.0) *
           std::copysign(std::pow(std::fabs(value), exponent - 2.0), value);
}

Bends bend(Real xx) noexcept
{
    return {.xx = xx, .xy = 0.0, .yy = 0.0};
}

void curves_of(const Instruction &instruction,
               const Frame &frame,
               const Tangents &tangents,
               std::span<const Real> partials,
               std::span<Real> out) noexcept
{
    switch (instruction.op)
    {
    case Operator::Constant:
    case Operator::Variable:
        return;
    case Operator::Sum:
    case Operator::Negation:
    case Operator::Abs:
    case Operator::Max:
    case Operator::Min:
    case Operator::PiecewiseLinear:
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
    case Operator::And:
    case Operator::Or:
    case Operator::Not:
    case Operator::If:
        std::ranges::fill(out, 0.0);
        return;
    case Operator::Product:
        product(instruction, frame, tangents, out);
        return;
    case Operator::Division:
        quotient(instruction, tangents, bends_of(instruction, frame, partials), out);
        return;
    case Operator::Power:
    case Operator::Atan2:
        pair(instruction, tangents, bends_of(instruction, frame, partials), out);
        return;
    case Operator::ConstantPower:
    case Operator::Exp:
    case Operator::Log:
    case Operator::Sqrt:
    case Operator::Sin:
    case Operator::Cos:
    case Operator::Tan:
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
        curve(instruction, tangents, bends_of(instruction, frame, partials).xx, out);
        return;
    }
}

}

std::size_t widest_product(const Tape &tape) noexcept
{
    std::size_t widest = 0;
    for (const Block &block : tape.blocks())
    {
        for (const Instruction &instruction : block.instructions)
        {
            if (instruction.op == Operator::Product)
            {
                widest = (std::max)(widest, std::size_t{instruction.second - instruction.first});
            }
        }
    }
    return widest;
}

Real scaled(Real tangent, Real factor) noexcept
{
    return is_zero(tangent) ? 0.0 : tangent * factor;
}

bool curved(Real exponent) noexcept
{
    return !is_zero(exponent * (exponent - 1.0));
}

Bends bends_of(const Instruction &instruction,
               const Frame &frame,
               std::span<const Real> partials) noexcept
{
    const Real c = constant_of(instruction);
    switch (instruction.op)
    {
    case Operator::Constant:
    case Operator::Variable:
    case Operator::Sum:
    case Operator::Product:
    case Operator::Negation:
    case Operator::Abs:
    case Operator::Max:
    case Operator::Min:
    case Operator::PiecewiseLinear:
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
    case Operator::And:
    case Operator::Or:
    case Operator::Not:
    case Operator::If:
        return bend(0.0);
    case Operator::Division:
        return quotient_of(frame, partials);
    case Operator::Atan2:
        return angle_of(partials);
    case Operator::Power:
        return power_of(instruction, frame);
    case Operator::ConstantPower:
        return bend(power_curve(c, unsigned_zero(operand(frame, instruction.first))));
    case Operator::Exp:
    case Operator::Sinh:
    case Operator::Cosh:
        return bend(frame.result[0]);
    case Operator::Log:
        return bend(-partials[0] * partials[0]);
    case Operator::Sqrt:
        return bend(-2.0 * cube(partials[0]));
    case Operator::Sin:
    case Operator::Cos:
        return bend(-frame.result[0]);
    case Operator::Tan:
        return bend(2.0 * frame.result[0] * partials[0]);
    case Operator::Asin:
    case Operator::Acos:
        return bend(operand(frame, instruction.first) * cube(partials[0]));
    case Operator::Atan:
        return bend(-2.0 * operand(frame, instruction.first) * partials[0] * partials[0]);
    case Operator::Tanh:
        return bend(-2.0 * frame.result[0] * partials[0]);
    case Operator::Asinh:
    case Operator::Acosh:
        return bend(-operand(frame, instruction.first) * cube(partials[0]));
    case Operator::Atanh:
        return bend(2.0 * operand(frame, instruction.first) * partials[0] * partials[0]);
    case Operator::Entropy:
        return bend(1.0 / unsigned_zero(operand(frame, instruction.first)));
    case Operator::SignPower:
        return bend(sign_curve(c, unsigned_zero(operand(frame, instruction.first))));
    case Operator::Erf:
        return bend(-2.0 * operand(frame, instruction.first) * partials[0]);
    }
    return bend(0.0);
}

void partial_tangents(const Instruction &instruction,
                      const Frame &frame,
                      const Tangents &tangents,
                      std::span<const Real> partials,
                      std::span<Real> out) noexcept
{
    curves_of(instruction, frame, tangents, partials, out);
    mark_undefined(frame, out);
}

}
