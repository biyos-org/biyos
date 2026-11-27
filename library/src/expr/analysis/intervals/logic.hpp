#pragma once

#include "core/numeric/tolerances.hpp"
#include "expr/analysis/intervals/interval.hpp"

namespace biyos
{

using Comparison = Interval (*)(const Tolerances &, Interval, Interval) noexcept;

Interval less_equal(const Tolerances &tolerances, Interval a, Interval b) noexcept;
Interval greater_equal(const Tolerances &tolerances, Interval a, Interval b) noexcept;
Interval equal(const Tolerances &tolerances, Interval a, Interval b) noexcept;
Interval less(const Tolerances &tolerances, Interval a, Interval b) noexcept;
Interval greater(const Tolerances &tolerances, Interval a, Interval b) noexcept;
Interval not_equal(const Tolerances &tolerances, Interval a, Interval b) noexcept;
Interval complement(Interval condition) noexcept;

}
