#pragma once

#include "core/numbers.hpp"
#include "expr/curvature/shape.hpp"
#include "expr/intervals/interval.hpp"

#include <span>

namespace biyos
{

Profile power_bend(Interval base, Real exponent) noexcept;
Profile sign_bend(Interval base, Real exponent) noexcept;
Curvature power_curvature(Interval base, Interval exponent, std::span<Argument> arguments) noexcept;

}
