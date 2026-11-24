#include "expr/graph/points.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/memory/room.hpp"
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

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

void check_finite(std::string_view axis, std::size_t k, double value)
{
    if (!std::isfinite(value))
    {
        refuse(std::string(axis) + " " + text_of(k) + " " + std::string(fault(value)));
    }
}

void check_rise(std::span<const double> x, std::size_t k)
{
    if (k > 0 && !(x[k - 1] < x[k]))
    {
        refuse("x " + text_of(k) + " is not above x " + text_of(k - 1));
    }
}

void check_segment(const Pwl &pwl, std::size_t k)
{
    if (k == 0)
    {
        return;
    }
    const std::string between = " from point " + text_of(k - 1) + " to point " + text_of(k) + " ";
    const double width = pwl.x[k] - pwl.x[k - 1];
    if (!std::isfinite(width))
    {
        refuse("the pwl's width" + between + std::string(fault(width)));
    }
    const double rise = pwl.y[k] - pwl.y[k - 1];
    if (!std::isfinite(rise))
    {
        refuse("the pwl's height difference" + between + std::string(fault(rise)));
    }
    const double slope = slope_of(pwl, k - 1);
    if (!std::isfinite(slope))
    {
        refuse("the pwl's slope" + between + std::string(fault(slope)));
    }
}

}

Points::Points() : starts_(1, 0) {}

Offset Points::size() const noexcept
{
    return static_cast<Offset>(starts_.size()) - 1;
}

Pwl Points::pwl(Offset index) const noexcept
{
    const std::size_t start = at(starts_[at(index)]);
    const std::size_t n = at(starts_[at(index) + 1]) - start;
    return {
        .x = std::span(x_).subspan(start, n),
        .y = std::span(y_).subspan(start, n),
    };
}

Offset Points::add(std::span<const double> x, std::span<const double> y)
{
    check_points(x, y);
    reserve_more(starts_, 1);
    reserve_more(x_, x.size());
    reserve_more(y_, y.size());
    x_.insert(x_.end(), x.begin(), x.end());
    y_.insert(y_.end(), y.begin(), y.end());
    starts_.push_back(static_cast<Offset>(x_.size()));
    return size() - 1;
}

void Points::truncate(Offset size) noexcept
{
    const std::size_t n = at(size);
    starts_.resize(n + 1);
    x_.resize(at(starts_[n]));
    y_.resize(at(starts_[n]));
}

void check_points(std::span<const double> x, std::span<const double> y)
{
    const auto n = static_cast<Offset>(x.size());
    if (x.size() != y.size())
    {
        const std::string given = n_text(static_cast<Offset>(y.size()), "y value", "y values");
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

void check_pwl(Offset pwl, Offset size)
{
    if (pwl < 0 || pwl >= size)
    {
        refuse("pwl " + text_of(pwl) + " is unknown");
    }
}

std::size_t segment_of(std::span<const double> x, double t) noexcept
{
    const std::span<const double> inner = x.subspan(1, x.size() - 2);
    return static_cast<std::size_t>(std::ranges::upper_bound(inner, t) - inner.begin());
}

double slope_of(const Pwl &pwl, std::size_t segment) noexcept
{
    const double rise = pwl.y[segment + 1] - pwl.y[segment];
    return rise / (pwl.x[segment + 1] - pwl.x[segment]);
}

}
