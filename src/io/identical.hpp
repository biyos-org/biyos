#pragma once

#include <bit>
#include <cstdint>

namespace biyos
{

inline bool identical(double a, double b) noexcept
{
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

}
