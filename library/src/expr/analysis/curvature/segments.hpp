#pragma once

#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/graph/points.hpp"

namespace biyos
{

Profile segments(const PiecewiseLinear &pwl, Interval child) noexcept;

}
