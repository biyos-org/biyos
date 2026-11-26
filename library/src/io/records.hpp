#pragma once

#include "core/numbers.hpp"
#include "io/directory.hpp"
#include "io/lines.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

using Fields = std::span<const std::string_view>;

Fields uncommented(Fields fields, std::size_t third) noexcept;
std::string quote(std::string_view text);
Real value_of(const Lines &lines, std::string_view field);
void check_name(const Lines &lines, std::string_view name);
Index find_in(const Directory &directory,
              const Lines &lines,
              std::string_view noun,
              std::string_view name);

}
