#pragma once

#include "core/env.hpp"
#include "core/events/hooks.hpp"
#include "core/options/settings.hpp"
#include "model/model.hpp"

#include <string>

namespace biyos
{

void warn_leftovers(Env &env,
                    Owner owner,
                    const Model &model,
                    const Settings &settings,
                    const std::string &file);

}
