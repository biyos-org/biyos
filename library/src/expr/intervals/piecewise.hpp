#pragma once

#include "expr/graph/points.hpp"
#include "expr/intervals/interval.hpp"

#include <cstddef>

namespace biyos
{

Interval slope(const PiecewiseLinear &pwl, std::size_t segment) noexcept;
Interval piecewise(const PiecewiseLinear &pwl, Interval child) noexcept;

}
