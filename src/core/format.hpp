#pragma once

#include "core/events.hpp"

#include <string>

namespace biyos
{

// Each formatter turns one event into one line with no trailing newline. A byte below 0x20 is
// written as `\n`, `\t` or `\u00XX`, so a line never breaks. The time is in seconds with six
// decimals, cut to the microsecond, whatever the locale.
// Text, for people: `0.001234 info core: text name=value`, the text left out when empty.
// JSON, for tools: one object with time, kind, level, module, text and fields, the time a number
// and every other value a string.
// Ill-formed UTF-8 becomes U+FFFD in JSON, so every line parses, and passes through in text.
std::string format_text(const Event &event);
std::string format_json(const Event &event);

}
