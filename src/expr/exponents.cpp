#include "expr/exponents.hpp"

#include "expr/bends.hpp"
#include "expr/interval.hpp"
#include "expr/shape.hpp"
#include "expr/zero.hpp"

#include <cmath>
#include <span>

namespace biyos
{

namespace
{

constexpr Bend kUnknown{
    .domain = kReals,
    .curvature = everywhere(Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Unknown),
};

constexpr Bend kFlat{
    .domain = kReals,
    .curvature = everywhere(Curvature::Affine),
    .monotonicity = everywhere(Monotonicity::Constant),
};

constexpr Bend kLinear{
    .domain = kReals,
    .curvature = everywhere(Curvature::Affine),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

constexpr Bend kParabola{
    .domain = kReals,
    .curvature = everywhere(Curvature::Convex),
    .monotonicity =
        at_zero(Monotonicity::Nonincreasing, Monotonicity::Nondecreasing, Monotonicity::Unknown),
};

constexpr Bend kCubic{
    .domain = kReals,
    .curvature = at_zero(Curvature::Concave, Curvature::Convex, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

constexpr Bend kOddRoot{
    .domain = kReals,
    .curvature = at_zero(Curvature::Convex, Curvature::Concave, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

constexpr Bend kSuperlinear{
    .domain = kNonnegatives,
    .curvature = everywhere(Curvature::Convex),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

constexpr Bend kSublinear{
    .domain = kNonnegatives,
    .curvature = everywhere(Curvature::Concave),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

constexpr Bend kReciprocal{
    .domain = kPositives,
    .curvature = everywhere(Curvature::Convex),
    .monotonicity = everywhere(Monotonicity::Nonincreasing),
};

constexpr Bend kEvenReciprocal{
    .domain = kNegatives,
    .curvature = everywhere(Curvature::Convex),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

constexpr Bend kOddReciprocal{
    .domain = kNegatives,
    .curvature = everywhere(Curvature::Concave),
    .monotonicity = everywhere(Monotonicity::Nonincreasing),
};

bool is_whole(double value) noexcept
{
    return is_zero(std::fmod(value, 1.0));
}

bool is_even(double value) noexcept
{
    return is_zero(std::fmod(value, 2.0));
}

const Bend &whole_power(Interval base, double exponent) noexcept
{
    if (exponent > 0.0)
    {
        if (is_even(exponent))
        {
            return kParabola;
        }
        return exponent > 1.0 ? kCubic : kLinear;
    }
    if (base.lower() >= 0.0)
    {
        return kReciprocal;
    }
    if (base.upper() <= 0.0)
    {
        return is_even(exponent) ? kEvenReciprocal : kOddReciprocal;
    }
    return kUnknown;
}

const Bend &power_shape(Interval base, double exponent) noexcept
{
    if (is_zero(exponent))
    {
        return kFlat;
    }
    if (is_whole(exponent))
    {
        return whole_power(base, exponent);
    }
    if (exponent < 0.0)
    {
        return kReciprocal;
    }
    return exponent > 1.0 ? kSuperlinear : kSublinear;
}

}

Profile power_bend(Interval base, double exponent) noexcept
{
    return bend(power_shape(base, exponent), base);
}

Profile sign_bend(Interval base, double exponent) noexcept
{
    if (exponent > 1.0)
    {
        return bend(kCubic, base);
    }
    return bend(exponent < 1.0 ? kOddRoot : kLinear, base);
}

Curvature power_curvature(Interval base, Interval exponent, std::span<Argument> arguments) noexcept
{
    if (is_point(exponent))
    {
        const Profile profile = power_bend(base, exponent.lower());
        arguments[0] = profile.argument;
        arguments[1] = {.monotonicity = Monotonicity::Constant};
        return profile.curvature;
    }
    if (!(base.lower() > 0.0))
    {
        arguments[0] = {};
        arguments[1] = {};
        return Curvature::Unknown;
    }
    arguments[0] = {.monotonicity = monotonicity_of(exponent)};
    arguments[1] = {.monotonicity = monotonicity_of(base.lower() >= 1.0, base.upper() <= 1.0)};
    if (!is_point(base))
    {
        return Curvature::Unknown;
    }
    return arguments[1].monotonicity == Monotonicity::Constant ? Curvature::Affine
                                                               : Curvature::Convex;
}

}
