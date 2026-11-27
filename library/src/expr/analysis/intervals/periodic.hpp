#pragma once

#include "expr/analysis/intervals/interval.hpp"

namespace biyos
{

Interval sin(Interval value) noexcept;
Interval cos(Interval value) noexcept;
Interval tan(Interval value) noexcept;

}
