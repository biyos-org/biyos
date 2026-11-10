#pragma once

#include "core/events.hpp"

#include <string>

namespace biyos
{

// Each formatter turns one event into one line with no trailing newline. A byte below 0x20 is
// written as `\n`, `\t` or `\u00XX`, so a line never breaks.
// Text, for people: `info core: text name=value`, the text left out when empty.
// JSON, for tools: one object with kind, level, module, text and fields, field values as strings.
// Ill-formed UTF-8 becomes U+FFFD in JSON, so every line parses, and passes through in text.
std::string format_text(const Event &event);
std::string format_json(const Event &event);

}
