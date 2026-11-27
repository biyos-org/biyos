#include "expr/analysis/curvature/segments.hpp"

#include "core/numbers.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/analysis/intervals/piecewise.hpp"
#include "expr/graph/points.hpp"

#include <algorithm>
#include <cstddef>
#include <span>

namespace biyos
{

namespace
{

std::size_t last_segment(std::span<const Real> x, Real t) noexcept
{
    const std::span<const Real> inner = x.subspan(1, x.size() - 2);
    return static_cast<std::size_t>(std::ranges::lower_bound(inner, t) - inner.begin());
}

Profile shape_over(const PiecewiseLinear &pwl, std::size_t first, std::size_t last) noexcept
{
    Interval previous = slope(pwl, first);
    Interval slopes = previous;
    bool convex = true;
    bool concave = true;
    for (std::size_t k = first + 1; k <= last; ++k)
    {
        const Interval next = slope(pwl, k);
        convex = convex && previous.upper() <= next.lower();
        concave = concave && previous.lower() >= next.upper();
        slopes = hull(slopes, next);
        previous = next;
    }
    return {
        .curvature = curvature_of(convex, concave),
        .argument = {.monotonicity = monotonicity_of(slopes)},
    };
}

}

Profile segments(const PiecewiseLinear &pwl, Interval child) noexcept
{
    if (child.is_empty())
    {
        return {};
    }
    if (is_point(child))
    {
        return {
            .curvature = Curvature::Affine,
            .argument = {.monotonicity = Monotonicity::Constant},
        };
    }
    const std::size_t first = segment_of(pwl.x, child.lower());
    return shape_over(pwl, first, last_segment(pwl.x, child.upper()));
}

}
