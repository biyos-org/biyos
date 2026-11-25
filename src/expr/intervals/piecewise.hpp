#pragma once

#include "expr/graph/points.hpp"
#include "expr/intervals/interval.hpp"

#include <cstddef>

namespace biyos
{

Interval slope(const Pwl &pwl, std::size_t segment) noexcept;
Interval piecewise(const Pwl &pwl, Interval child) noexcept;

}
