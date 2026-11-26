#pragma once

#include "core/numbers.hpp"

#include <cmath>

namespace biyos
{

inline bool is_zero(Real value) noexcept
{
    return std::fpclassify(value) == FP_ZERO;
}

}
