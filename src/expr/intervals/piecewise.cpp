#include "expr/intervals/piecewise.hpp"

#include "core/numeric/limits.hpp"
#include "core/numeric/zero.hpp"
#include "expr/graph/points.hpp"
#include "expr/intervals/interval.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace biyos
{

namespace
{

struct Piece
{
    std::size_t anchor;
    std::size_t segment;
    double from;
    double to;
};

Interval point(double value) noexcept
{
    return {value, value};
}

bool is_flat(const PiecewiseLinear &pwl, std::size_t segment) noexcept
{
    return is_zero(pwl.y[segment + 1] - pwl.y[segment]);
}

std::size_t piece_of(std::span<const double> x, double t) noexcept
{
    const std::span<const double> starts = x.subspan(1);
    return static_cast<std::size_t>(std::ranges::upper_bound(starts, t) - starts.begin());
}

double before(double x) noexcept
{
    return std::nextafter(x, -kInfinity);
}

Piece piece_at(const PiecewiseLinear &pwl, std::size_t anchor, Interval child) noexcept
{
    const std::size_t last = pwl.x.size() - 1;
    return {
        .anchor = anchor,
        .segment = (std::min)(anchor, last - 1),
        .from = anchor == 0 ? child.lower() : (std::max)(child.lower(), pwl.x[anchor]),
        .to = anchor == last ? child.upper() : (std::min)(child.upper(), before(pwl.x[anchor + 1])),
    };
}

Interval rise_over(Interval slope, double from, double t) noexcept
{
    if (std::isinf(t - from))
    {
        return slope * point(t) + -(slope * point(from));
    }
    return slope * (point(t) + point(-from));
}

Interval value_at(const PiecewiseLinear &pwl, const Piece &piece, double t) noexcept
{
    const double x = pwl.x[piece.anchor];
    const double y = pwl.y[piece.anchor];
    if (is_zero(t - x))
    {
        return point(y);
    }
    return point(y) + rise_over(slope(pwl, piece.segment), x, t);
}

double lowest(const PiecewiseLinear &pwl, const Piece &piece, double t) noexcept
{
    return std::isinf(t) ? -kInfinity : value_at(pwl, piece, t).lower();
}

double highest(const PiecewiseLinear &pwl, const Piece &piece, double t) noexcept
{
    return std::isinf(t) ? kInfinity : value_at(pwl, piece, t).upper();
}

Interval range_of(const PiecewiseLinear &pwl, const Piece &piece) noexcept
{
    if (is_flat(pwl, piece.segment))
    {
        return point(pwl.y[piece.anchor]);
    }
    const bool rising = pwl.y[piece.segment + 1] > pwl.y[piece.segment];
    const double low = rising ? piece.from : piece.to;
    const double high = rising ? piece.to : piece.from;
    return {lowest(pwl, piece, low), highest(pwl, piece, high)};
}

}

Interval slope(const PiecewiseLinear &pwl, std::size_t segment) noexcept
{
    if (is_flat(pwl, segment))
    {
        return {0.0, 0.0};
    }
    const Interval rise = point(pwl.y[segment + 1]) + point(-pwl.y[segment]);
    return rise / (point(pwl.x[segment + 1]) + point(-pwl.x[segment]));
}

Interval piecewise(const PiecewiseLinear &pwl, Interval child) noexcept
{
    if (child.is_empty())
    {
        return Interval::empty();
    }
    const std::size_t first = piece_of(pwl.x, child.lower());
    const std::size_t last = piece_of(pwl.x, child.upper());
    Interval total = Interval::empty();
    for (std::size_t anchor = first; anchor <= last; ++anchor)
    {
        total = hull(total, range_of(pwl, piece_at(pwl, anchor, child)));
    }
    return total;
}

}
