#pragma once

#include "core/events/stream.hpp"
#include "core/options/settings.hpp"
#include "model/model.hpp"

#include <string>
#include <string_view>

namespace biyos
{

void warn_leftovers(Stream stream,
                    const Model &model,
                    const Settings &settings,
                    const std::string &file,
                    std::string_view format);

}
