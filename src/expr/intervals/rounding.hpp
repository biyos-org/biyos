#pragma once

#include "core/numbers.hpp"

namespace biyos
{

constexpr int kRoundedSteps = 1;
constexpr int kLibmUlps = 14;
constexpr int kLibmSteps = 256;

Real shifted(Real value, int steps) noexcept;
Real round_down(Real value) noexcept;
Real round_up(Real value) noexcept;
Real widen_down(Real value) noexcept;
Real widen_up(Real value) noexcept;

}
