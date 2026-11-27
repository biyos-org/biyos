#include "expr/evaluation/derivatives/partials.hpp"

#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>
#include <span>

namespace biyos
{

namespace
{

std::span<Real> part(std::span<Real> partials, std::size_t k, const Frame &frame) noexcept
{
    return row(partials, k, frame.lanes);
}

template <typename F>
void slope(const Instruction &instruction,
           const Frame &frame,
           std::span<Real> partials,
           F f) noexcept
{
    const std::span<const Real> x = row(frame, instruction.first);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        partials[l] = f(x[l], frame.result[l]);
    }
}

void product(const Instruction &instruction, const Frame &frame, std::span<Real> partials) noexcept
{
    const std::span<const std::uint32_t> factors = listed(instruction, frame.operands);
    const std::span<Real> running = frame.spare;
    std::ranges::fill(running, constant_of(instruction));
    for (std::size_t k = 0; k < factors.size(); ++k)
    {
        const std::span<const Real> x = row(frame, factors[k]);
        const std::span<Real> mine = part(partials, k, frame);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            mine[l] = running[l];
            running[l] *= x[l];
        }
    }
    std::ranges::fill(running, 1.0);
    for (std::size_t k = factors.size(); k > 0; --k)
    {
        const std::span<const Real> x = row(frame, factors[k - 1]);
        const std::span<Real> mine = part(partials, k - 1, frame);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            mine[l] *= running[l];
            running[l] *= x[l];
        }
    }
}

template <typename F>
void extreme(const Instruction &instruction,
             const Frame &frame,
             std::span<Real> partials,
             F better) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, frame.operands);
    const std::span<Real> best = frame.spare;
    std::ranges::copy(row(frame, operands[0]), best.begin());
    std::ranges::fill(partials, 0.0);
    for (std::size_t k = 1; k < operands.size(); ++k)
    {
        const std::span<const Real> x = row(frame, operands[k]);
        const auto index = static_cast<Real>(k);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            const bool wins = std::isnan(x[l]) || better(x[l], best[l]);
            partials[l] = wins ? index : partials[l];
            best[l] = wins ? x[l] : best[l];
        }
    }
}

Real unsigned_zero(Real value) noexcept
{
    return value + 0.0;
}

Real power_slope(Real exponent, Real base) noexcept
{
    if (is_zero(exponent))
    {
        return 0.0;
    }
    return exponent * std::pow(base, exponent - 1.0);
}

void quotient(const Instruction &instruction, const Frame &frame, std::span<Real> partials) noexcept
{
    const std::span<const Real> divisor = row(frame, instruction.second);
    const std::span<Real> first = part(partials, 0, frame);
    const std::span<Real> second = part(partials, 1, frame);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        first[l] = 1.0 / divisor[l];
        second[l] = -frame.result[l] / divisor[l];
    }
}

void angle(const Instruction &instruction, const Frame &frame, std::span<Real> partials) noexcept
{
    const std::span<const Real> y = row(frame, instruction.first);
    const std::span<const Real> x = row(frame, instruction.second);
    const std::span<Real> first = part(partials, 0, frame);
    const std::span<Real> second = part(partials, 1, frame);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        const Real radius = std::hypot(x[l], y[l]);
        first[l] = x[l] / radius / radius;
        second[l] = -(y[l] / radius) / radius;
    }
}

Real exponent_slope(Real result, Real base) noexcept
{
    return is_zero(result) ? 0.0 : result * std::log(base);
}

void power(const Instruction &instruction, const Frame &frame, std::span<Real> partials) noexcept
{
    const std::span<const Real> base = row(frame, instruction.first);
    const std::span<const Real> exponent = row(frame, instruction.second);
    const std::span<Real> first = part(partials, 0, frame);
    const std::span<Real> second = part(partials, 1, frame);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        first[l] = power_slope(exponent[l], unsigned_zero(base[l]));
        second[l] = exponent_slope(frame.result[l], base[l]);
    }
}

void fixed_power(const Instruction &instruction,
                 const Frame &frame,
                 std::span<Real> partials) noexcept
{
    const Real exponent = constant_of(instruction);
    slope(instruction,
          frame,
          partials,
          [exponent](Real x, Real) { return power_slope(exponent, unsigned_zero(x)); });
}

Real sign_slope(Real exponent, Real value) noexcept
{
    return exponent * std::pow(std::fabs(value), exponent - 1.0);
}

void signed_power(const Instruction &instruction,
                  const Frame &frame,
                  std::span<Real> partials) noexcept
{
    const Real exponent = constant_of(instruction);
    slope(instruction,
          frame,
          partials,
          [exponent](Real x, Real) { return sign_slope(exponent, x); });
}

Real arc_slope(Real value) noexcept
{
    return 1.0 / std::sqrt((1.0 - value) * (1.0 + value));
}

Real atan_slope(Real value) noexcept
{
    return 1.0 / (1.0 + value * value);
}

Real tanh_slope(Real value) noexcept
{
    const Real decay = std::exp(-2.0 * std::fabs(value));
    return 4.0 * decay / ((1.0 + decay) * (1.0 + decay));
}

Real acosh_slope(Real value) noexcept
{
    return 1.0 / (std::sqrt(value - 1.0) * std::sqrt(value + 1.0));
}

Real atanh_slope(Real value) noexcept
{
    return 1.0 / ((1.0 - value) * (1.0 + value));
}

Real erf_slope(Real value) noexcept
{
    return 2.0 * std::numbers::inv_sqrtpi_v<Real> * std::exp(-value * value);
}

Real sign(Real value) noexcept
{
    return value < 0.0 ? -1.0 : 1.0;
}

void piecewise(const Instruction &instruction,
               const Frame &frame,
               std::span<Real> partials) noexcept
{
    const PiecewiseLinear pwl = piecewise_linear_of(instruction, frame.weights);
    slope(instruction, frame, partials, [pwl](Real x, Real) { return slope_at(pwl, x); });
}

void slopes_of(const Instruction &instruction,
               const Frame &frame,
               std::span<Real> partials) noexcept
{
    switch (instruction.op)
    {
    case Operator::Constant:
    case Operator::Variable:
        return;
    case Operator::Sum:
        std::ranges::fill(partials, 1.0);
        return;
    case Operator::Negation:
        std::ranges::fill(partials, -1.0);
        return;
    case Operator::Product:
        product(instruction, frame, partials);
        return;
    case Operator::Division:
        quotient(instruction, frame, partials);
        return;
    case Operator::ConstantPower:
        fixed_power(instruction, frame, partials);
        return;
    case Operator::Power:
        power(instruction, frame, partials);
        return;
    case Operator::Exp:
        slope(instruction, frame, partials, [](Real, Real y) { return y; });
        return;
    case Operator::Log:
        slope(instruction, frame, partials, [](Real x, Real) { return 1.0 / unsigned_zero(x); });
        return;
    case Operator::Sqrt:
        slope(instruction, frame, partials, [](Real, Real y) { return 0.5 / unsigned_zero(y); });
        return;
    case Operator::Sin:
        slope(instruction, frame, partials, [](Real x, Real) { return std::cos(x); });
        return;
    case Operator::Cos:
        slope(instruction, frame, partials, [](Real x, Real) { return -std::sin(x); });
        return;
    case Operator::Tan:
        slope(instruction, frame, partials, [](Real, Real y) { return 1.0 + y * y; });
        return;
    case Operator::Abs:
        slope(instruction, frame, partials, [](Real x, Real) { return sign(x); });
        return;
    case Operator::Max:
        extreme(instruction, frame, partials, std::greater<>{});
        return;
    case Operator::Min:
        extreme(instruction, frame, partials, std::less<>{});
        return;
    case Operator::Asin:
        slope(instruction, frame, partials, [](Real x, Real) { return arc_slope(x); });
        return;
    case Operator::Acos:
        slope(instruction, frame, partials, [](Real x, Real) { return -arc_slope(x); });
        return;
    case Operator::Atan:
        slope(instruction, frame, partials, [](Real x, Real) { return atan_slope(x); });
        return;
    case Operator::Sinh:
        slope(instruction, frame, partials, [](Real x, Real) { return std::cosh(x); });
        return;
    case Operator::Cosh:
        slope(instruction, frame, partials, [](Real x, Real) { return std::sinh(x); });
        return;
    case Operator::Tanh:
        slope(instruction, frame, partials, [](Real x, Real) { return tanh_slope(x); });
        return;
    case Operator::Asinh:
        slope(instruction, frame, partials, [](Real x, Real) { return 1.0 / std::hypot(x, 1.0); });
        return;
    case Operator::Acosh:
        slope(instruction, frame, partials, [](Real x, Real) { return acosh_slope(x); });
        return;
    case Operator::Atanh:
        slope(instruction, frame, partials, [](Real x, Real) { return atanh_slope(x); });
        return;
    case Operator::Entropy:
        slope(instruction, frame, partials, [](Real x, Real) { return std::log(x) + 1.0; });
        return;
    case Operator::SignPower:
        signed_power(instruction, frame, partials);
        return;
    case Operator::Erf:
        slope(instruction, frame, partials, [](Real x, Real) { return erf_slope(x); });
        return;
    case Operator::PiecewiseLinear:
        piecewise(instruction, frame, partials);
        return;
    case Operator::Atan2:
        angle(instruction, frame, partials);
        return;
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
        std::ranges::fill(partials, 0.0);
        return;
    }
}

}

void mark_undefined(const Frame &frame, std::span<Real> partials) noexcept
{
    constexpr Real kNan = (std::numeric_limits<Real>::quiet_NaN)();
    for (std::size_t k = 0; k < partials.size(); k += frame.lanes)
    {
        const std::span<Real> mine = partials.subspan(k, frame.lanes);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            mine[l] = std::isnan(frame.result[l]) ? kNan : mine[l];
        }
    }
}

std::uint32_t n_partials(const Instruction &instruction) noexcept
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
    case Operator::Atan2:
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
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
    case Operator::PiecewiseLinear:
        return 1;
    }
    return 0;
}

void partials_of(const Instruction &instruction,
                 const Frame &frame,
                 std::span<Real> partials) noexcept
{
    slopes_of(instruction, frame, partials);
    mark_undefined(frame, partials);
}

std::size_t chosen(Real partial) noexcept
{
    return static_cast<std::size_t>(partial);
}

}
