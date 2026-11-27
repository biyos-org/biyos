#pragma once

#include "core/numbers.hpp"
#include "expr/analysis/intervals/interval.hpp"

namespace biyos
{

Interval power(Interval base, Interval exponent) noexcept;
Interval sign_power(Interval base, Real exponent) noexcept;

}
