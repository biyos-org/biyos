#pragma once

#include "expr/interval.hpp"

namespace biyos
{

Interval exp(Interval value) noexcept;
Interval log(Interval value) noexcept;
Interval sqrt(Interval value) noexcept;
Interval asin(Interval value) noexcept;
Interval acos(Interval value) noexcept;
Interval atan(Interval value) noexcept;
Interval sinh(Interval value) noexcept;
Interval cosh(Interval value) noexcept;
Interval tanh(Interval value) noexcept;
Interval asinh(Interval value) noexcept;
Interval acosh(Interval value) noexcept;
Interval atanh(Interval value) noexcept;
Interval erf(Interval value) noexcept;
Interval entropy(Interval value) noexcept;

}
