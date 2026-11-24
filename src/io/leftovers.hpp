#pragma once

#include "core/env.hpp"
#include "core/settings.hpp"
#include "model/model.hpp"

#include <string>

namespace biyos
{

void warn_leftovers(Env &env,
                    const Model &model,
                    const Settings &settings,
                    const std::string &file);

}
