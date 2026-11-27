#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace biyos::native
{

std::string escaped(std::string_view name);
std::optional<std::string> unescaped(std::string_view field);

}
