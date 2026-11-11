#pragma once

#include <array>
#include <charconv>
#include <string>

namespace biyos
{

template <typename T> std::string text_of(T value)
{
    std::array<char, 32> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

}
