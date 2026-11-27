#include "expr/analysis/curvature/arithmetic.hpp"

#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"

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

Monotonicity product_monotonicity(Real coefficient, Counts others) noexcept
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
    const Real lead = seen(operands[leader], mirrored).lower();
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

Curvature along_x(Interval y, Interval x) noexcept
{
    const bool nonnegative = x.lower() >= 0.0;
    const bool nonpositive = x.upper() <= 0.0;
    const bool negative = x.upper() < 0.0;
    switch (sign_of(y))
    {
    case Sign::Positive:
        return curvature_of(nonnegative, nonpositive);
    case Sign::Negative:
        return curvature_of(nonpositive, nonnegative);
    case Sign::Zero:
        return nonnegative || negative ? Curvature::Affine : Curvature::Unknown;
    case Sign::Mixed:
        return Curvature::Unknown;
    }
    return Curvature::Unknown;
}

Curvature along_y(Interval y, Interval x) noexcept
{
    const bool nonnegative = y.lower() >= 0.0;
    const bool nonpositive = y.upper() <= 0.0;
    const bool positive = y.lower() > 0.0;
    const bool negative = y.upper() < 0.0;
    switch (sign_of(x))
    {
    case Sign::Positive:
        return curvature_of(nonpositive, nonnegative);
    case Sign::Negative:
        return curvature_of(nonnegative, negative);
    case Sign::Zero:
        return positive || negative ? Curvature::Affine : Curvature::Unknown;
    case Sign::Mixed:
        return Curvature::Unknown;
    }
    return Curvature::Unknown;
}

Curvature angle_bend(Interval y, Interval x) noexcept
{
    if (is_point(y) && is_point(x))
    {
        return Curvature::Affine;
    }
    if (is_point(y))
    {
        return along_x(y, x);
    }
    return is_point(x) ? along_y(y, x) : Curvature::Unknown;
}

Monotonicity y_monotonicity(Interval y, Interval x) noexcept
{
    const bool one_side = y.lower() >= 0.0 || y.upper() < 0.0;
    return monotonicity_of(x.lower() >= 0.0, x.upper() < 0.0 && one_side);
}

Monotonicity x_monotonicity(Interval y) noexcept
{
    return monotonicity_of(y.upper() < 0.0, y.lower() >= 0.0);
}

}

Curvature sum_curvature(std::span<const Real> weights, std::span<Argument> arguments) noexcept
{
    for (std::size_t k = 0; k < weights.size(); ++k)
    {
        arguments[k] = {.monotonicity = monotonicity_of({weights[k], weights[k]})};
    }
    return Curvature::Affine;
}

Curvature product_curvature(Real coefficient,
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

Curvature angle_curvature(Interval y, Interval x, std::span<Argument> arguments) noexcept
{
    const Curvature curvature = angle_bend(y, x);
    const bool level = curvature == Curvature::Affine;
    arguments[0] = {.monotonicity = level ? Monotonicity::Constant : y_monotonicity(y, x)};
    arguments[1] = {.monotonicity = level ? Monotonicity::Constant : x_monotonicity(y)};
    return curvature;
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
