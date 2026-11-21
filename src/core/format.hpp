#pragma once

#include "core/events.hpp"

#include <string>

namespace biyos
{

std::string fmt_text(const Event &evt);
std::string fmt_json(const Event &evt);

}
