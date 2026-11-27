#include "expr/analysis/intervals/interval.hpp"

#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "core/numeric/zero.hpp"
#include "expr/analysis/intervals/rounding.hpp"

#include <algorithm>
#include <cmath>

namespace biyos
{

namespace
{
using Bound = Real (*)(Real, Real) noexcept;

Real sum_down(Real left, Real right) noexcept
{
    if (is_zero(left) || is_zero(right))
    {
        return left + right;
    }
    return round_down(left + right);
}

Real sum_up(Real left, Real right) noexcept
{
    if (is_zero(left) || is_zero(right))
    {
        return left + right;
    }
    return round_up(left + right);
}

Real product_down(Real left, Real right) noexcept
{
    if (is_zero(left) || is_zero(right))
    {
        return 0.0;
    }
    return round_down(left * right);
}

Real product_up(Real left, Real right) noexcept
{
    if (is_zero(left) || is_zero(right))
    {
        return 0.0;
    }
    return round_up(left * right);
}

Real quotient_down(Real left, Real right) noexcept
{
    if (is_zero(left))
    {
        return 0.0;
    }
    return round_down(left / right);
}

Real quotient_up(Real left, Real right) noexcept
{
    if (is_zero(left))
    {
        return 0.0;
    }
    return round_up(left / right);
}

Real lowest(Interval left, Interval right, Bound bound) noexcept
{
    const Real first =
        std::fmin(bound(left.lower(), right.lower()), bound(left.lower(), right.upper()));
    const Real last =
        std::fmin(bound(left.upper(), right.lower()), bound(left.upper(), right.upper()));
    return std::fmin(first, last);
}

Real highest(Interval left, Interval right, Bound bound) noexcept
{
    const Real first =
        std::fmax(bound(left.lower(), right.lower()), bound(left.lower(), right.upper()));
    const Real last =
        std::fmax(bound(left.upper(), right.lower()), bound(left.upper(), right.upper()));
    return std::fmax(first, last);
}

bool is_zero_point(Interval value) noexcept
{
    return !(value.lower() < 0.0) && !(value.upper() > 0.0);
}

bool straddles(Interval value) noexcept
{
    return value.lower() < 0.0 && value.upper() > 0.0;
}

Interval ray(Interval left, Interval right) noexcept
{
    const bool positive = !(left.lower() < 0.0);
    if (right.upper() > 0.0)
    {
        return positive ? Interval(quotient_down(left.lower(), right.upper()), kInfinity)
                        : Interval(-kInfinity, quotient_up(left.upper(), right.upper()));
    }
    return positive ? Interval(-kInfinity, quotient_up(left.lower(), right.lower()))
                    : Interval(quotient_down(left.upper(), right.lower()), kInfinity);
}

}

Interval operator+(Interval left, Interval right) noexcept
{
    if (left.is_empty() || right.is_empty())
    {
        return Interval::empty();
    }
    return {sum_down(left.lower(), right.lower()), sum_up(left.upper(), right.upper())};
}

Interval operator-(Interval value) noexcept
{
    return {-value.upper(), -value.lower()};
}

Interval operator*(Interval left, Interval right) noexcept
{
    if (left.is_empty() || right.is_empty())
    {
        return Interval::empty();
    }
    return {lowest(left, right, product_down), highest(left, right, product_up)};
}

Interval operator/(Interval left, Interval right) noexcept
{
    if (left.is_empty() || right.is_empty() || is_zero_point(right))
    {
        return Interval::empty();
    }
    if (right.lower() > 0.0 || right.upper() < 0.0)
    {
        return {lowest(left, right, quotient_down), highest(left, right, quotient_up)};
    }
    if (is_zero_point(left))
    {
        return {0.0, 0.0};
    }
    if (straddles(left) || straddles(right))
    {
        return Interval::entire();
    }
    return ray(left, right);
}

Interval hull(Interval left, Interval right) noexcept
{
    if (left.is_empty())
    {
        return right;
    }
    if (right.is_empty())
    {
        return left;
    }
    return {(std::min)(left.lower(), right.lower()), (std::max)(left.upper(), right.upper())};
}

Interval meet(Interval left, Interval right) noexcept
{
    if (left.is_empty() || right.is_empty())
    {
        return Interval::empty();
    }
    return {(std::max)(left.lower(), right.lower()), (std::min)(left.upper(), right.upper())};
}

Interval maximum(Interval left, Interval right) noexcept
{
    if (left.is_empty() || right.is_empty())
    {
        return Interval::empty();
    }
    return {(std::max)(left.lower(), right.lower()), (std::max)(left.upper(), right.upper())};
}

Interval minimum(Interval left, Interval right) noexcept
{
    if (left.is_empty() || right.is_empty())
    {
        return Interval::empty();
    }
    return {(std::min)(left.lower(), right.lower()), (std::min)(left.upper(), right.upper())};
}

Interval abs(Interval value) noexcept
{
    if (value.is_empty() || value.lower() >= 0.0)
    {
        return value;
    }
    if (value.upper() <= 0.0)
    {
        return -value;
    }
    return {0.0, (std::max)(-value.lower(), value.upper())};
}

Interval signs(Interval value, Real root) noexcept
{
    const Real lower = value.lower() >= root ? 0.0 : -kInfinity;
    const Real upper = value.upper() <= root ? 0.0 : kInfinity;
    return {lower, upper};
}

}
