#include "expr/powers.hpp"

#include "expr/interval.hpp"
#include "expr/rounding.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace biyos
{

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr Interval kNonnegative{0.0, kInfinity};
constexpr Interval kZero{0.0, 0.0};
constexpr Interval kOne{1.0, 1.0};

Interval corners(Interval base, Interval exponent) noexcept
{
    const double first = std::pow(base.lower(), exponent.lower());
    const double second = std::pow(base.lower(), exponent.upper());
    const double third = std::pow(base.upper(), exponent.lower());
    const double fourth = std::pow(base.upper(), exponent.upper());
    const double low = (std::min)({first, second, third, fourth});
    const double high = (std::max)({first, second, third, fourth});
    return {widen_down(low), widen_up(high)};
}

Interval zero_base(Interval exponent) noexcept
{
    const bool positive = exponent.upper() > 0.0;
    const bool zero = exponent.lower() <= 0.0 && exponent.upper() >= 0.0;
    return hull(positive ? kZero : Interval::empty(), zero ? kOne : Interval::empty());
}

Interval positive_base(Interval base, Interval exponent) noexcept
{
    if (base.upper() < 0.0)
    {
        return Interval::empty();
    }
    const Interval inside(base.lower() > 0.0 ? base.lower() : 0.0, base.upper());
    if (inside.upper() <= 0.0)
    {
        return zero_base(exponent);
    }
    return meet(corners(inside, exponent), kNonnegative);
}

bool is_even(double whole) noexcept
{
    return std::fpclassify(std::fmod(whole, 2.0)) == FP_ZERO;
}

bool has_even(Interval whole) noexcept
{
    return whole.lower() < whole.upper() || is_even(whole.lower());
}

bool has_odd(Interval whole) noexcept
{
    return whole.lower() < whole.upper() || !is_even(whole.lower());
}

Interval negative_base(Interval base, Interval exponent) noexcept
{
    const Interval whole(std::ceil(exponent.lower()), std::floor(exponent.upper()));
    if (!(base.lower() < 0.0) || whole.is_empty())
    {
        return Interval::empty();
    }
    const Interval values = positive_base(-base, whole);
    const Interval even = has_even(whole) ? values : Interval::empty();
    const Interval odd = has_odd(whole) ? -values : Interval::empty();
    return hull(even, odd);
}

double signed_power(double base, double exponent) noexcept
{
    return std::copysign(std::pow(std::fabs(base), exponent), base);
}

}

Interval power(Interval base, Interval exponent) noexcept
{
    if (base.is_empty() || exponent.is_empty())
    {
        return Interval::empty();
    }
    return hull(positive_base(base, exponent), negative_base(base, exponent));
}

Interval sign_power(Interval base, double exponent) noexcept
{
    if (base.is_empty())
    {
        return base;
    }
    const double lower = widen_down(signed_power(base.lower(), exponent));
    const double upper = widen_up(signed_power(base.upper(), exponent));
    return meet(Interval(lower, upper), signs(base, 0.0));
}

}
