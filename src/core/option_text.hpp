#pragma once

#include "core/options.hpp"

#include <array>
#include <charconv>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

template <typename T> std::string text_of(T value)
{
    std::array<char, 32> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

std::string subject(std::string_view name);
std::string joined(std::span<const std::string_view> names);
std::string_view noun(const BoolRule &);
std::string_view noun(const IntRule &);
std::string_view noun(const DoubleRule &);
std::string_view noun(const StringRule &);
std::string_view noun(const EnumRule &);
std::string_view noun(const OptionSpec &spec);

}
