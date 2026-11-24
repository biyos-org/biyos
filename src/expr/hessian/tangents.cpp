#include "expr/hessian/tangents.hpp"

#include "core/numeric/zero.hpp"
#include "expr/derivatives/partials.hpp"
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

struct Pair
{
    std::span<double> value;
    std::span<double> tangent;
};

std::span<double> part(std::span<double> items, std::size_t k, const Frame &frame) noexcept
{
    return row(items, k, frame.lanes);
}

std::span<const double> tangent(const Tangents &tangents,
                                std::uint32_t slot,
                                const Frame &frame) noexcept
{
    return row(tangents.slots, slot, frame.lanes);
}

double unsigned_zero(double value) noexcept
{
    return value + 0.0;
}

double cube(double value) noexcept
{
    return value * value * value;
}

template <typename F>
void curve(const Instruction &instruction,
           const Frame &frame,
           const Tangents &tangents,
           std::span<const double> partials,
           std::span<double> out,
           F f) noexcept
{
    const std::span<const double> x = row(frame, instruction.first);
    const std::span<const double> t = tangent(tangents, instruction.first, frame);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        out[l] = scaled(t[l], f(x[l], frame.result[l], partials[l]));
    }
}

void advance(Pair running, std::span<const double> x, std::span<const double> t) noexcept
{
    for (std::size_t l = 0; l < x.size(); ++l)
    {
        running.tangent[l] = scaled(running.tangent[l], x[l]) + scaled(t[l], running.value[l]);
        running.value[l] *= x[l];
    }
}

void product(const Instruction &instruction,
             const Frame &frame,
             const Tangents &tangents,
             std::span<double> out) noexcept
{
    const std::span<const std::uint32_t> factors = listed(instruction, frame.operands);
    const Pair running{.value = part(tangents.spare, 0, frame),
                       .tangent = part(tangents.spare, 1, frame)};
    std::ranges::fill(running.value, constant_of(instruction));
    std::ranges::fill(running.tangent, 0.0);
    for (std::size_t k = 0; k < factors.size(); ++k)
    {
        std::ranges::copy(running.value, part(tangents.spare, k + 2, frame).begin());
        std::ranges::copy(running.tangent, part(out, k, frame).begin());
        advance(running, row(frame, factors[k]), tangent(tangents, factors[k], frame));
    }
    std::ranges::fill(running.value, 1.0);
    std::ranges::fill(running.tangent, 0.0);
    for (std::size_t k = factors.size(); k > 0; --k)
    {
        const std::span<const double> prefix = part(tangents.spare, k + 1, frame);
        const std::span<double> mine = part(out, k - 1, frame);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            mine[l] = scaled(mine[l], running.value[l]) + scaled(running.tangent[l], prefix[l]);
        }
        advance(running, row(frame, factors[k - 1]), tangent(tangents, factors[k - 1], frame));
    }
}

void quotient(const Instruction &instruction,
              const Frame &frame,
              const Tangents &tangents,
              std::span<const double> partials,
              std::span<double> out) noexcept
{
    const std::span<const double> tx = tangent(tangents, instruction.first, frame);
    const std::span<const double> ty = tangent(tangents, instruction.second, frame);
    const std::span<double> first = part(out, 0, frame);
    const std::span<double> second = part(out, 1, frame);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        const double inverse = partials[l];
        const double square = inverse * inverse;
        first[l] = -scaled(ty[l], square);
        second[l] = scaled(ty[l], 2.0 * frame.result[l] * square) - scaled(tx[l], square);
    }
}

double power_curve(double exponent, double base) noexcept
{
    const double factor = exponent * (exponent - 1.0);
    if (is_zero(factor))
    {
        return 0.0;
    }
    return factor * std::pow(base, exponent - 2.0);
}

double cross_curve(double result, double base, double exponent) noexcept
{
    if (is_zero(result))
    {
        return 0.0;
    }
    return std::pow(base, exponent - 1.0) * (1.0 + exponent * std::log(base));
}

double exponent_curve(double result, double base) noexcept
{
    if (is_zero(result))
    {
        return 0.0;
    }
    const double log = std::log(base);
    return result * log * log;
}

void power(const Instruction &instruction,
           const Frame &frame,
           const Tangents &tangents,
           std::span<double> out) noexcept
{
    const std::span<const double> base = row(frame, instruction.first);
    const std::span<const double> exponent = row(frame, instruction.second);
    const std::span<const double> tx = tangent(tangents, instruction.first, frame);
    const std::span<const double> ty = tangent(tangents, instruction.second, frame);
    const std::span<double> first = part(out, 0, frame);
    const std::span<double> second = part(out, 1, frame);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        const double x = unsigned_zero(base[l]);
        const double cross = cross_curve(frame.result[l], x, exponent[l]);
        first[l] = scaled(tx[l], power_curve(exponent[l], x)) + scaled(ty[l], cross);
        second[l] = scaled(tx[l], cross) + scaled(ty[l], exponent_curve(frame.result[l], base[l]));
    }
}

double sign_curve(double exponent, double value) noexcept
{
    const double factor = exponent * (exponent - 1.0);
    if (is_zero(factor))
    {
        return 0.0;
    }
    return factor * std::copysign(std::pow(std::fabs(value), exponent - 2.0), value);
}

void curves_of(const Instruction &instruction,
               const Frame &frame,
               const Tangents &tangents,
               std::span<const double> partials,
               std::span<double> out) noexcept
{
    const double c = constant_of(instruction);
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
        std::ranges::fill(out, 0.0);
        return;
    case Operator::Product:
        product(instruction, frame, tangents, out);
        return;
    case Operator::Division:
        quotient(instruction, frame, tangents, partials, out);
        return;
    case Operator::ConstantPower:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [c](double x, double, double) { return power_curve(c, unsigned_zero(x)); });
        return;
    case Operator::Power:
        power(instruction, frame, tangents, out);
        return;
    case Operator::Exp:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double, double y, double) { return y; });
        return;
    case Operator::Log:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double, double, double p) { return -p * p; });
        return;
    case Operator::Sqrt:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double, double, double p) { return -2.0 * cube(p); });
        return;
    case Operator::Sin:
    case Operator::Cos:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double, double y, double) { return -y; });
        return;
    case Operator::Tan:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double, double y, double p) { return 2.0 * y * p; });
        return;
    case Operator::Asin:
    case Operator::Acos:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double x, double, double p) { return x * cube(p); });
        return;
    case Operator::Atan:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double x, double, double p) { return -2.0 * x * p * p; });
        return;
    case Operator::Sinh:
    case Operator::Cosh:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double, double y, double) { return y; });
        return;
    case Operator::Tanh:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double, double y, double p) { return -2.0 * y * p; });
        return;
    case Operator::Asinh:
    case Operator::Acosh:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double x, double, double p) { return -x * cube(p); });
        return;
    case Operator::Atanh:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double x, double, double p) { return 2.0 * x * p * p; });
        return;
    case Operator::Entropy:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double x, double, double) { return 1.0 / unsigned_zero(x); });
        return;
    case Operator::SignPower:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [c](double x, double, double) { return sign_curve(c, unsigned_zero(x)); });
        return;
    case Operator::Erf:
        curve(instruction,
              frame,
              tangents,
              partials,
              out,
              [](double x, double, double p) { return -2.0 * x * p; });
        return;
    }
}

}

std::size_t tangent_rows(const Tape &tape) noexcept
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
    return widest + 2;
}

double scaled(double tangent, double factor) noexcept
{
    return is_zero(tangent) ? 0.0 : tangent * factor;
}

void partial_tangents(const Instruction &instruction,
                      const Frame &frame,
                      const Tangents &tangents,
                      std::span<const double> partials,
                      std::span<double> out) noexcept
{
    curves_of(instruction, frame, tangents, partials, out);
    mark_undefined(frame, out);
}

}
