#pragma once

#include "expr/intervals/interval.hpp"
#include "expr/shape.hpp"

#include <span>

namespace biyos
{

Profile power_bend(Interval base, double exponent) noexcept;
Profile sign_bend(Interval base, double exponent) noexcept;
Curvature power_curvature(Interval base, Interval exponent, std::span<Argument> arguments) noexcept;

}
