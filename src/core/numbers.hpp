#pragma once

#include <cstddef>
#include <cstdint>

namespace biyos
{

using Index = std::int32_t;
using Offset = std::int64_t;
using Real = double;
using Bits = std::uint64_t;

static_assert(sizeof(Bits) == sizeof(Real));

template <typename T> std::size_t to_size(T) = delete;
template <typename T> Index to_index(T) = delete;
template <typename T> Offset to_offset(T) = delete;

constexpr std::size_t to_size(Index value) noexcept
{
    return static_cast<std::size_t>(value);
}

constexpr std::size_t to_size(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

constexpr Index to_index(std::size_t value) noexcept
{
    return static_cast<Index>(value);
}

constexpr Offset to_offset(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

}
