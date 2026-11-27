#pragma once

#include "core/numbers.hpp"

#include <cmath>

namespace biyos
{

inline Real angle(Real y, Real x) noexcept
{
    return std::atan2(y + 0.0, x + 0.0);
}

}
