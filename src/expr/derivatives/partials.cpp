#include "expr/derivatives/partials.hpp"

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

std::span<double> part(std::span<double> partials, std::size_t k, const Frame &frame) noexcept
{
    return row(partials, k, frame.lanes);
}

template <typename F>
void slope(const Instruction &instruction,
           const Frame &frame,
           std::span<double> partials,
           F f) noexcept
{
    const std::span<const double> x = row(frame, instruction.first);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        partials[l] = f(x[l], frame.result[l]);
    }
}

void product(const Instruction &instruction,
             const Frame &frame,
             std::span<double> partials) noexcept
{
    const std::span<const std::uint32_t> factors = listed(instruction, frame.operands);
    const std::span<double> running = frame.spare;
    std::ranges::fill(running, constant_of(instruction));
    for (std::size_t k = 0; k < factors.size(); ++k)
    {
        const std::span<const double> x = row(frame, factors[k]);
        const std::span<double> mine = part(partials, k, frame);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            mine[l] = running[l];
            running[l] *= x[l];
        }
    }
    std::ranges::fill(running, 1.0);
    for (std::size_t k = factors.size(); k > 0; --k)
    {
        const std::span<const double> x = row(frame, factors[k - 1]);
        const std::span<double> mine = part(partials, k - 1, frame);
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
             std::span<double> partials,
             F better) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, frame.operands);
    const std::span<double> best = frame.spare;
    std::ranges::copy(row(frame, operands[0]), best.begin());
    std::ranges::fill(partials, 0.0);
    for (std::size_t k = 1; k < operands.size(); ++k)
    {
        const std::span<const double> x = row(frame, operands[k]);
        const auto index = static_cast<double>(k);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            const bool wins = std::isnan(x[l]) || better(x[l], best[l]);
            partials[l] = wins ? index : partials[l];
            best[l] = wins ? x[l] : best[l];
        }
    }
}

double unsigned_zero(double value) noexcept
{
    return value + 0.0;
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
              std::span<double> partials) noexcept
{
    const std::span<const double> divisor = row(frame, instruction.second);
    const std::span<double> first = part(partials, 0, frame);
    const std::span<double> second = part(partials, 1, frame);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        first[l] = 1.0 / divisor[l];
        second[l] = -frame.result[l] / divisor[l];
    }
}

double exponent_slope(double result, double base) noexcept
{
    return is_zero(result) ? 0.0 : result * std::log(base);
}

void power(const Instruction &instruction, const Frame &frame, std::span<double> partials) noexcept
{
    const std::span<const double> base = row(frame, instruction.first);
    const std::span<const double> exponent = row(frame, instruction.second);
    const std::span<double> first = part(partials, 0, frame);
    const std::span<double> second = part(partials, 1, frame);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        first[l] = power_slope(exponent[l], unsigned_zero(base[l]));
        second[l] = exponent_slope(frame.result[l], base[l]);
    }
}

void fixed_power(const Instruction &instruction,
                 const Frame &frame,
                 std::span<double> partials) noexcept
{
    const double exponent = constant_of(instruction);
    slope(instruction,
          frame,
          partials,
          [exponent](double x, double) { return power_slope(exponent, unsigned_zero(x)); });
}

double sign_slope(double exponent, double value) noexcept
{
    return exponent * std::pow(std::fabs(value), exponent - 1.0);
}

void signed_power(const Instruction &instruction,
                  const Frame &frame,
                  std::span<double> partials) noexcept
{
    const double exponent = constant_of(instruction);
    slope(instruction,
          frame,
          partials,
          [exponent](double x, double) { return sign_slope(exponent, x); });
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

void piecewise(const Instruction &instruction,
               const Frame &frame,
               std::span<double> partials) noexcept
{
    const PiecewiseLinear pwl = pwl_of(instruction, frame.weights);
    slope(instruction, frame, partials, [pwl](double x, double) { return slope_at(pwl, x); });
}

void slopes_of(const Instruction &instruction,
               const Frame &frame,
               std::span<double> partials) noexcept
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
        slope(instruction, frame, partials, [](double, double y) { return y; });
        return;
    case Operator::Log:
        slope(instruction,
              frame,
              partials,
              [](double x, double) { return 1.0 / unsigned_zero(x); });
        return;
    case Operator::Sqrt:
        slope(instruction,
              frame,
              partials,
              [](double, double y) { return 0.5 / unsigned_zero(y); });
        return;
    case Operator::Sin:
        slope(instruction, frame, partials, [](double x, double) { return std::cos(x); });
        return;
    case Operator::Cos:
        slope(instruction, frame, partials, [](double x, double) { return -std::sin(x); });
        return;
    case Operator::Tan:
        slope(instruction, frame, partials, [](double, double y) { return 1.0 + y * y; });
        return;
    case Operator::Abs:
        slope(instruction, frame, partials, [](double x, double) { return sign(x); });
        return;
    case Operator::Max:
        extreme(instruction, frame, partials, std::greater<>{});
        return;
    case Operator::Min:
        extreme(instruction, frame, partials, std::less<>{});
        return;
    case Operator::Asin:
        slope(instruction, frame, partials, [](double x, double) { return arc_slope(x); });
        return;
    case Operator::Acos:
        slope(instruction, frame, partials, [](double x, double) { return -arc_slope(x); });
        return;
    case Operator::Atan:
        slope(instruction, frame, partials, [](double x, double) { return atan_slope(x); });
        return;
    case Operator::Sinh:
        slope(instruction, frame, partials, [](double x, double) { return std::cosh(x); });
        return;
    case Operator::Cosh:
        slope(instruction, frame, partials, [](double x, double) { return std::sinh(x); });
        return;
    case Operator::Tanh:
        slope(instruction, frame, partials, [](double x, double) { return tanh_slope(x); });
        return;
    case Operator::Asinh:
        slope(instruction,
              frame,
              partials,
              [](double x, double) { return 1.0 / std::hypot(x, 1.0); });
        return;
    case Operator::Acosh:
        slope(instruction, frame, partials, [](double x, double) { return acosh_slope(x); });
        return;
    case Operator::Atanh:
        slope(instruction, frame, partials, [](double x, double) { return atanh_slope(x); });
        return;
    case Operator::Entropy:
        slope(instruction, frame, partials, [](double x, double) { return std::log(x) + 1.0; });
        return;
    case Operator::SignPower:
        signed_power(instruction, frame, partials);
        return;
    case Operator::Erf:
        slope(instruction, frame, partials, [](double x, double) { return erf_slope(x); });
        return;
    case Operator::PiecewiseLinear:
        piecewise(instruction, frame, partials);
        return;
    }
}

}

void mark_undefined(const Frame &frame, std::span<double> partials) noexcept
{
    constexpr double kNan = (std::numeric_limits<double>::quiet_NaN)();
    for (std::size_t k = 0; k < partials.size(); k += frame.lanes)
    {
        const std::span<double> mine = partials.subspan(k, frame.lanes);
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
                 std::span<double> partials) noexcept
{
    slopes_of(instruction, frame, partials);
    mark_undefined(frame, partials);
}

std::size_t chosen(double partial) noexcept
{
    return static_cast<std::size_t>(partial);
}

}
