#pragma once

#include "core/numbers.hpp"

#include <bit>

namespace biyos
{

inline bool identical(Real a, Real b) noexcept
{
    return std::bit_cast<Bits>(a) == std::bit_cast<Bits>(b);
}

}
