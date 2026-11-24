#pragma once

#include <cmath>

namespace biyos
{

inline bool is_zero(double value) noexcept
{
    return std::fpclassify(value) == FP_ZERO;
}

}
