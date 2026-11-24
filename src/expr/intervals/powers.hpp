#pragma once

#include "expr/intervals/interval.hpp"

namespace biyos
{

Interval power(Interval base, Interval exponent) noexcept;
Interval sign_power(Interval base, double exponent) noexcept;

}
