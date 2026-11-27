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

template <typename F>
void curve(const Instruction &instruction,
           const Frame &frame,
           const Tangents &tangents,
           std::span<const Real> partials,
           std::span<Real> out,
           F f) noexcept
{
    const Real factor = f(operand(frame, instruction.first), frame.result[0], partials[0]);
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

void quotient(const Instruction &instruction,
              const Frame &frame,
              const Tangents &tangents,
              std::span<const Real> partials,
              std::span<Real> out) noexcept
{
    const std::span<const Real> tx = tangent(tangents, instruction.first);
    const std::span<const Real> ty = tangent(tangents, instruction.second);
    const std::span<Real> first = part(out, 0, tangents);
    const std::span<Real> second = part(out, 1, tangents);
    const Real inverse = partials[0];
    const Real square = inverse * inverse;
    const Real across = 2.0 * frame.result[0] * square;
    for (std::size_t l = 0; l < tangents.lanes; ++l)
    {
        first[l] = -scaled(ty[l], square);
        second[l] = scaled(ty[l], across) - scaled(tx[l], square);
    }
}

void angle(const Instruction &instruction,
           const Tangents &tangents,
           std::span<const Real> partials,
           std::span<Real> out) noexcept
{
    const std::span<const Real> ty = tangent(tangents, instruction.first);
    const std::span<const Real> tx = tangent(tangents, instruction.second);
    const Real along_y = partials[0];
    const Real along_x = partials[1];
    const std::span<Real> first = part(out, 0, tangents);
    const std::span<Real> second = part(out, 1, tangents);
    const Real cross = 2.0 * along_y * along_x;
    const Real bend = along_x * along_x - along_y * along_y;
    for (std::size_t l = 0; l < tangents.lanes; ++l)
    {
        first[l] = scaled(ty[l], cross) + scaled(tx[l], bend);
        second[l] = scaled(ty[l], bend) - scaled(tx[l], cross);
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

void power(const Instruction &instruction,
           const Frame &frame,
           const Tangents &tangents,
           std::span<Real> out) noexcept
{
    const Real base = operand(frame, instruction.first);
    const Real exponent = operand(frame, instruction.second);
    const Real x = unsigned_zero(base);
    const Real cross = cross_curve(frame.result[0], x, exponent);
    const Real along_x = power_curve(exponent, x);
    const Real along_y = exponent_curve(frame.result[0], base);
    const std::span<const Real> tx = tangent(tangents, instruction.first);
    const std::span<const Real> ty = tangent(tangents, instruction.second);
    const std::span<Real> first = part(out, 0, tangents);
    const std::span<Real> second = part(out, 1, tangents);
    for (std::size_t l = 0; l < tangents.lanes; ++l)
    {
        first[l] = scaled(tx[l], along_x) + scaled(ty[l], cross);
        second[l] = scaled(tx[l], cross) + scaled(ty[l], along_y);
    }
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

void curves_of(const Instruction &instruction,
               const Frame &frame,
               const Tangents &tangents,
               std::span<const Real> partials,
               std::span<Real> out) noexcept
{
    const Real c = constant_of(instruction);
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
        quotient(instruction, frame, tangents, partials, out);
        return;
    case Operator::Atan2:
        angle(instruction, tangents, partials, out);
        return;
    case Operator::ConstantPower:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [c](Real x, Real, Real) { return power_curve(c, unsigned_zero(x)); });
        return;
    case Operator::Power:
        power(instruction, frame, tangents, out);
        return;
    case Operator::Exp:
        curve(instruction, frame, tangents, partials, out, [](Real, Real y, Real) { return y; });
        return;
    case Operator::Log:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real, Real, Real p) { return -p * p; });
        return;
    case Operator::Sqrt:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real, Real, Real p) { return -2.0 * cube(p); });
        return;
    case Operator::Sin:
    case Operator::Cos:
        curve(instruction, frame, tangents, partials, out, [](Real, Real y, Real) { return -y; });
        return;
    case Operator::Tan:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real, Real y, Real p) { return 2.0 * y * p; });
        return;
    case Operator::Asin:
    case Operator::Acos:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real x, Real, Real p) { return x * cube(p); });
        return;
    case Operator::Atan:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real x, Real, Real p) { return -2.0 * x * p * p; });
        return;
    case Operator::Sinh:
    case Operator::Cosh:
        curve(instruction, frame, tangents, partials, out, [](Real, Real y, Real) { return y; });
        return;
    case Operator::Tanh:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real, Real y, Real p) { return -2.0 * y * p; });
        return;
    case Operator::Asinh:
    case Operator::Acosh:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real x, Real, Real p) { return -x * cube(p); });
        return;
    case Operator::Atanh:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real x, Real, Real p) { return 2.0 * x * p * p; });
        return;
    case Operator::Entropy:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real x, Real, Real) { return 1.0 / unsigned_zero(x); });
        return;
    case Operator::SignPower:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [c](Real x, Real, Real) { return sign_curve(c, unsigned_zero(x)); });
        return;
    case Operator::Erf:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](Real x, Real, Real p) { return -2.0 * x * p; });
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
