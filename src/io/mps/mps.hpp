#pragma once

#include "core/env.hpp"
#include "model/model.hpp"

#include <istream>
#include <string>

namespace biyos
{

Model read_mps(Env &env, std::istream &input, const std::string &file);

}
