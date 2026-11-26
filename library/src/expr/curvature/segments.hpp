#pragma once

#include "expr/curvature/shape.hpp"
#include "expr/graph/points.hpp"
#include "expr/intervals/interval.hpp"

namespace biyos
{

Profile segments(const PiecewiseLinear &pwl, Interval child) noexcept;

}
