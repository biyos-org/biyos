#include "expr/arithmetic.hpp"

#include "core/zero.hpp"
#include "expr/interval.hpp"
#include "expr/shape.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace biyos
{

namespace
{

enum class Sign : std::uint8_t
{
    Zero,
    Mixed,
    Negative,
    Positive,
};

using Counts = std::array<std::size_t, 4>;

Sign sign_of(Interval factor) noexcept
{
    if (!(factor.lower() < 0.0) && !(factor.upper() > 0.0))
    {
        return Sign::Zero;
    }
    if (factor.lower() < 0.0 && factor.upper() > 0.0)
    {
        return Sign::Mixed;
    }
    return factor.upper() <= 0.0 ? Sign::Negative : Sign::Positive;
}

std::size_t &count(Counts &counts, Sign sign) noexcept
{
    return counts[static_cast<std::size_t>(sign)];
}

Monotonicity product_monotonicity(double coefficient, Counts others) noexcept
{
    if (is_zero(coefficient) || count(others, Sign::Zero) > 0)
    {
        return Monotonicity::Constant;
    }
    if (count(others, Sign::Mixed) > 0)
    {
        return Monotonicity::Unknown;
    }
    const bool odd = count(others, Sign::Negative) % 2 == 1;
    return (coefficient < 0.0) != odd ? Monotonicity::Nonincreasing : Monotonicity::Nondecreasing;
}

Interval seen(Interval value, bool mirrored) noexcept
{
    return mirrored ? -value : value;
}

bool leads(Interval candidate, Interval leader) noexcept
{
    if (candidate.lower() > leader.lower())
    {
        return true;
    }
    return !(candidate.lower() < leader.lower()) && candidate.upper() > leader.upper();
}

Curvature extreme(const Operands &operands,
                  std::span<Argument> arguments,
                  bool mirrored,
                  Curvature spread) noexcept
{
    std::size_t leader = 0;
    for (std::size_t k = 1; k < operands.size(); ++k)
    {
        if (leads(seen(operands[k], mirrored), seen(operands[leader], mirrored)))
        {
            leader = k;
        }
    }
    const double lead = seen(operands[leader], mirrored).lower();
    std::size_t active = 0;
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        const bool dominated = k != leader && lead >= seen(operands[k], mirrored).upper();
        arguments[k] = {.monotonicity =
                            dominated ? Monotonicity::Constant : Monotonicity::Nondecreasing};
        active += dominated ? 0U : 1U;
    }
    return active <= 1 ? Curvature::Affine : spread;
}

}

Curvature sum_curvature(std::span<const double> weights, std::span<Argument> arguments) noexcept
{
    for (std::size_t k = 0; k < weights.size(); ++k)
    {
        arguments[k] = {.monotonicity = monotonicity_of({weights[k], weights[k]})};
    }
    return Curvature::Affine;
}

Curvature product_curvature(double coefficient,
                            const Operands &factors,
                            std::span<Argument> arguments) noexcept
{
    if (any_empty(factors))
    {
        std::ranges::fill(arguments, Argument{});
        return Curvature::Unknown;
    }
    Counts counts{};
    std::size_t loose = 0;
    for (std::size_t k = 0; k < factors.size(); ++k)
    {
        ++count(counts, sign_of(factors[k]));
        loose += is_point(factors[k]) ? 0U : 1U;
    }
    for (std::size_t k = 0; k < factors.size(); ++k)
    {
        Counts others = counts;
        --count(others, sign_of(factors[k]));
        arguments[k] = {.monotonicity = product_monotonicity(coefficient, others)};
    }
    const bool zero = is_zero(coefficient) || count(counts, Sign::Zero) > 0;
    return zero || loose <= 1 ? Curvature::Affine : Curvature::Unknown;
}

Curvature quotient_curvature(Interval numerator,
                             Interval denominator,
                             std::span<Argument> arguments) noexcept
{
    if (is_point(denominator))
    {
        arguments[0] = {.monotonicity = monotonicity_of(denominator)};
        arguments[1] = {.monotonicity = Monotonicity::Constant};
        return Curvature::Affine;
    }
    const bool positive = denominator.lower() >= 0.0;
    const bool negative = denominator.upper() <= 0.0;
    if (!positive && !negative)
    {
        arguments[0] = {};
        arguments[1] = {};
        return Curvature::Unknown;
    }
    arguments[0] = {.monotonicity =
                        positive ? Monotonicity::Nondecreasing : Monotonicity::Nonincreasing};
    arguments[1] = {
        .monotonicity = monotonicity_of(-numerator),
        .cut_below = positive && !(denominator.lower() > 0.0),
        .cut_above = negative && !(denominator.upper() < 0.0),
    };
    if (!is_point(numerator))
    {
        return Curvature::Unknown;
    }
    const bool nonnegative = numerator.lower() >= 0.0;
    const bool nonpositive = numerator.upper() <= 0.0;
    return positive ? curvature_of(nonnegative, nonpositive)
                    : curvature_of(nonpositive, nonnegative);
}

Curvature largest_curvature(const Operands &operands, std::span<Argument> arguments) noexcept
{
    return extreme(operands, arguments, false, Curvature::Convex);
}

Curvature smallest_curvature(const Operands &operands, std::span<Argument> arguments) noexcept
{
    return extreme(operands, arguments, true, Curvature::Concave);
}

}
