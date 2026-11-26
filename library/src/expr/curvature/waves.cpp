#include "expr/curvature/waves.hpp"

#include "core/numbers.hpp"
#include "expr/curvature/shape.hpp"
#include "expr/intervals/interval.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>

namespace biyos
{

namespace
{

constexpr Real kHalfPiDown = 0x1.921fb54442d18p+0;
constexpr Real kHalfPiUp = 0x1.921fb54442d19p+0;
constexpr Real kFar = 0x1p52;
static_assert(std::numeric_limits<Real>::digits == 53);
constexpr std::int64_t kWidest = 2;
constexpr std::int64_t kTurn = 4;

Real upper_factor(Real quarter) noexcept
{
    return quarter < 0.0 ? kHalfPiDown : kHalfPiUp;
}

Real lower_factor(Real quarter) noexcept
{
    return quarter < 0.0 ? kHalfPiUp : kHalfPiDown;
}

std::optional<std::int64_t> quarter_below(Real value) noexcept
{
    if (!(std::fabs(value) <= kFar))
    {
        return std::nullopt;
    }
    const Real guess = std::floor(value / kHalfPiDown);
    for (const Real quarter : {guess, guess - 1.0})
    {
        if (std::fma(quarter, upper_factor(quarter), -value) <= 0.0)
        {
            return static_cast<std::int64_t>(quarter);
        }
    }
    return std::nullopt;
}

std::optional<std::int64_t> quarter_above(Real value) noexcept
{
    if (!(std::fabs(value) <= kFar))
    {
        return std::nullopt;
    }
    const Real guess = std::ceil(value / kHalfPiDown);
    for (const Real quarter : {guess, guess + 1.0})
    {
        if (std::fma(quarter, lower_factor(quarter), -value) >= 0.0)
        {
            return static_cast<std::int64_t>(quarter);
        }
    }
    return std::nullopt;
}

std::size_t phase(std::int64_t quarter) noexcept
{
    return static_cast<std::size_t>(((quarter % kTurn) + kTurn) % kTurn);
}

bool is_pole(std::int64_t boundary) noexcept
{
    return boundary % 2 != 0;
}

}

const Wave kSine{
    .curvatures = {Curvature::Concave, Curvature::Concave, Curvature::Convex, Curvature::Convex},
    .monotonicities =
        {
            Monotonicity::Nondecreasing,
            Monotonicity::Nonincreasing,
            Monotonicity::Nonincreasing,
            Monotonicity::Nondecreasing,
        },
    .poles = false,
};

const Wave kCosine{
    .curvatures = {Curvature::Concave, Curvature::Convex, Curvature::Convex, Curvature::Concave},
    .monotonicities =
        {
            Monotonicity::Nonincreasing,
            Monotonicity::Nonincreasing,
            Monotonicity::Nondecreasing,
            Monotonicity::Nondecreasing,
        },
    .poles = false,
};

const Wave kTangent{
    .curvatures = {Curvature::Convex, Curvature::Concave, Curvature::Convex, Curvature::Concave},
    .monotonicities =
        {
            Monotonicity::Nondecreasing,
            Monotonicity::Nondecreasing,
            Monotonicity::Nondecreasing,
            Monotonicity::Nondecreasing,
        },
    .poles = true,
};

Profile wave(const Wave &shape, Interval child) noexcept
{
    if (is_point(child))
    {
        return {.curvature = Curvature::Affine,
                .argument = {.monotonicity = Monotonicity::Constant}};
    }
    const std::optional<std::int64_t> first = quarter_below(child.lower());
    const std::optional<std::int64_t> last = quarter_above(child.upper());
    if (!first.has_value() || !last.has_value() || *last - *first > kWidest)
    {
        return {};
    }
    Curvature curvature = Curvature::Affine;
    Monotonicity monotonicity = Monotonicity::Constant;
    for (std::int64_t quarter = *first; quarter < *last; ++quarter)
    {
        if (shape.poles && quarter > *first && is_pole(quarter))
        {
            return {};
        }
        curvature = common(curvature, shape.curvatures[phase(quarter)]);
        monotonicity = common(monotonicity, shape.monotonicities[phase(quarter)]);
    }
    return {.curvature = curvature, .argument = {.monotonicity = monotonicity}};
}

}
