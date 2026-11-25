#include "expr/graph/points.hpp"

#include "core/error.hpp"
#include "core/memory/room.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "core/text/text.hpp"
#include "expr/graph/node.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr Offset kLeast = 2;

[[noreturn]] void refuse(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "expression", reason);
}

void check_finite(std::string_view axis, std::size_t k, Real value)
{
    if (!std::isfinite(value))
    {
        refuse(std::string(axis) + " " + text_of(k) + " " + std::string(fault(value)));
    }
}

void check_rise(std::span<const Real> x, std::size_t k)
{
    if (k > 0 && !(x[k - 1] < x[k]))
    {
        refuse("x " + text_of(k) + " is not above x " + text_of(k - 1));
    }
}

void check_segment(const PiecewiseLinear &pwl, std::size_t k)
{
    if (k == 0)
    {
        return;
    }
    const std::string between = " from point " + text_of(k - 1) + " to point " + text_of(k) + " ";
    const Real width = pwl.x[k] - pwl.x[k - 1];
    if (!std::isfinite(width))
    {
        refuse("the pwl's width" + between + std::string(fault(width)));
    }
    const Real rise = pwl.y[k] - pwl.y[k - 1];
    if (!std::isfinite(rise))
    {
        refuse("the pwl's height difference" + between + std::string(fault(rise)));
    }
    const Real slope = slope_of(pwl, k - 1);
    if (!std::isfinite(slope))
    {
        refuse("the pwl's slope" + between + std::string(fault(slope)));
    }
}

Real rise_over(Real slope, Real from, Real t) noexcept
{
    const Real step = t - from;
    if (std::isinf(step))
    {
        return slope * t - slope * from;
    }
    return slope * step;
}

}

Points::Points() : starts_(1, 0) {}

Offset Points::size() const noexcept
{
    return to_offset(starts_.size()) - 1;
}

PiecewiseLinear Points::piecewise_linear(Offset index) const noexcept
{
    return {
        .x = slice(std::span(x_), starts_, to_size(index)),
        .y = slice(std::span(y_), starts_, to_size(index)),
    };
}

Offset Points::add(std::span<const Real> x, std::span<const Real> y)
{
    check_points(x, y);
    reserve_more(starts_, 1);
    reserve_more(x_, x.size());
    reserve_more(y_, y.size());
    x_.insert(x_.end(), x.begin(), x.end());
    y_.insert(y_.end(), y.begin(), y.end());
    starts_.push_back(to_offset(x_.size()));
    return size() - 1;
}

void Points::truncate(Offset size) noexcept
{
    const std::size_t n = to_size(size);
    starts_.resize(n + 1);
    x_.resize(to_size(starts_[n]));
    y_.resize(to_size(starts_[n]));
}

void check_points(std::span<const Real> x, std::span<const Real> y)
{
    const auto n = to_offset(x.size());
    if (x.size() != y.size())
    {
        const std::string given = n_text(to_offset(y.size()), "y value", "y values");
        refuse(n_text(n, "x value", "x values") + " and " + given);
    }
    if (n < kLeast)
    {
        refuse("a pwl takes " + text_of(kLeast) + " points or more, not " + text_of(n));
    }
    for (std::size_t k = 0; k < x.size(); ++k)
    {
        check_finite("x", k, x[k]);
        check_finite("y", k, y[k]);
        check_rise(x, k);
        check_segment({.x = x, .y = y}, k);
    }
}

void check_piecewise_linear(Offset pwl, Offset size)
{
    if (pwl < 0 || pwl >= size)
    {
        refuse("pwl " + text_of(pwl) + " is unknown");
    }
}

std::size_t segment_of(std::span<const Real> x, Real t) noexcept
{
    const std::span<const Real> inner = x.subspan(1, x.size() - 2);
    return static_cast<std::size_t>(std::ranges::upper_bound(inner, t) - inner.begin());
}

Real slope_of(const PiecewiseLinear &pwl, std::size_t segment) noexcept
{
    const Real rise = pwl.y[segment + 1] - pwl.y[segment];
    return rise / (pwl.x[segment + 1] - pwl.x[segment]);
}

Real value_at(const PiecewiseLinear &pwl, Real t) noexcept
{
    const std::size_t k = segment_of(pwl.x, t);
    const std::size_t anchor = t < pwl.x[k + 1] ? k : k + 1;
    const Real slope = slope_of(pwl, k);
    if ((is_zero(slope) && !std::isnan(t)) || is_zero(t - pwl.x[anchor]))
    {
        return pwl.y[anchor];
    }
    return pwl.y[anchor] + rise_over(slope, pwl.x[anchor], t);
}

Real slope_at(const PiecewiseLinear &pwl, Real t) noexcept
{
    return slope_of(pwl, segment_of(pwl.x, t));
}

}
