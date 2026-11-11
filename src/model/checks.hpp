#pragma once

#include "core/index.hpp"

#include <string>
#include <string_view>

namespace biyos
{

std::string item(std::string_view noun, Index index);
std::string_view bound_fault(double lower, double upper) noexcept;
std::string_view value_fault(double value) noexcept;
void check_index(std::string_view noun, Index index, Index size);
void check_span(std::string_view noun, Index first, Index count, Index size);

}
