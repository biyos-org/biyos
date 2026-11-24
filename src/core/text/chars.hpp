#pragma once

#include <charconv>

namespace biyos::text
{

std::from_chars_result from_chars(const char *first, const char *last, double &value) noexcept;
std::from_chars_result from_decimal(const char *first, const char *last, double &value) noexcept;

}
