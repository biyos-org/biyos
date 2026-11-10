#pragma once

#include "core/events.hpp"

#include <string>

namespace biyos
{

// Each formatter turns one event into one line with no trailing newline. A byte below 0x20 is
// written as `\n`, `\t` or `\u00XX`, so a line never breaks.
// Text, for people: `info core: text name=value`, the text left out when empty.
std::string format_text(const Event &event);

}
