#pragma once

#include "io/draft/draft.hpp"
#include "io/lines.hpp"

#include <string_view>

namespace biyos
{

inline constexpr std::string_view kIndicatorCode = "IF";

void read_indicator(const Lines &lines, Draft &draft);

}
