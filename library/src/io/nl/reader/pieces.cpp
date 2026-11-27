#include "io/nl/reader/pieces.hpp"

#include "core/numbers.hpp"
#include "io/nl/source/source.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>

namespace biyos::nl
{

namespace
{

std::vector<Real> knots_of(std::span<const Real> breaks)
{
    const auto split = std::ranges::lower_bound(breaks, 0.0);
    const bool zero = split != breaks.end() && std::fpclassify(*split) == FP_ZERO;
    std::vector<Real> knots(breaks.begin(), split);
    if (!zero)
    {
        knots.push_back(0.0);
    }
    knots.insert(knots.end(), split, breaks.end());
    return knots;
}

Real slope_from(std::span<const Real> slopes, std::span<const Real> breaks, Real x)
{
    const auto passed = std::ranges::upper_bound(breaks, x) - breaks.begin();
    return slopes[static_cast<std::size_t>(passed)];
}

std::vector<Real> heights_of(std::span<const Real> knots,
                             std::span<const Real> slopes,
                             std::span<const Real> breaks)
{
    const auto zero =
        static_cast<std::size_t>(std::ranges::lower_bound(knots, 0.0) - knots.begin());
    std::vector<Real> heights(knots.size(), 0.0);
    for (std::size_t k = zero + 1; k < knots.size(); ++k)
    {
        const Real slope = slope_from(slopes, breaks, knots[k - 1]);
        heights[k] = heights[k - 1] + slope * (knots[k] - knots[k - 1]);
    }
    for (std::size_t k = zero; k > 0; --k)
    {
        const Real slope = slope_from(slopes, breaks, knots[k - 1]);
        heights[k - 1] = heights[k] - slope * (knots[k] - knots[k - 1]);
    }
    return heights;
}

Real step_from(Real x) noexcept
{
    return (std::max)(1.0, std::abs(x));
}

}

Curve curve_of(const Source &source, std::span<const Real> slopes, std::span<const Real> breaks)
{
    if (std::ranges::adjacent_find(breaks, std::ranges::greater_equal{}) != breaks.end())
    {
        source.bad("the breakpoints of a plterm do not increase");
    }
    const std::vector<Real> knots = knots_of(breaks);
    const std::vector<Real> heights = heights_of(knots, slopes, breaks);
    const Real before = step_from(knots.front());
    const Real after = step_from(knots.back());
    Curve curve;
    curve.x.push_back(knots.front() - before);
    curve.y.push_back(heights.front() - slopes.front() * before);
    curve.x.insert(curve.x.end(), knots.begin(), knots.end());
    curve.y.insert(curve.y.end(), heights.begin(), heights.end());
    curve.x.push_back(knots.back() + after);
    curve.y.push_back(heights.back() + slopes.back() * after);
    return curve;
}

}
