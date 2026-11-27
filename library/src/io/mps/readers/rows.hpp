#pragma once

#include "io/draft/draft.hpp"
#include "io/utils/lines.hpp"
#include "model/model.hpp"

#include <string_view>

namespace biyos
{

void read_row(const Lines &lines, Draft &draft);
ObjectiveSense sense_of(const Lines &lines, std::string_view word);
std::string_view letter_of(RowSense sense) noexcept;

}
