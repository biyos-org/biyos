#pragma once

#include "core/events.hpp"

#include <string>

namespace biyos
{

std::string format_text(const Event &event);
std::string format_json(const Event &event);

}
