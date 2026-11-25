#pragma once

#include "core/numbers.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

namespace biyos
{

inline constexpr std::size_t kTextWidth = 32;

constexpr std::size_t shortest_width() noexcept
{
    using Limits = std::numeric_limits<Real>;
    constexpr std::string_view kMarks = "-.e-";
    std::size_t exponent = 1;
    for (int k = Limits::max_digits10 - Limits::min_exponent10; k >= 10; k /= 10)
    {
        ++exponent;
    }
    return static_cast<std::size_t>(Limits::max_digits10) + exponent + kMarks.size();
}

static_assert(shortest_width() <= kTextWidth);

template <typename T> std::string text_of(T value)
{
    std::array<char, kTextWidth> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

}
