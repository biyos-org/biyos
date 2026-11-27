#pragma once

#include "core/numbers.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"

#include <span>

namespace biyos
{

Profile power_bend(Interval base, Real exponent) noexcept;
Profile sign_bend(Interval base, Real exponent) noexcept;
Curvature power_curvature(Interval base, Interval exponent, std::span<Argument> arguments) noexcept;

}
