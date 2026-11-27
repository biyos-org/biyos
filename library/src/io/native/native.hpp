#pragma once

#include "io/utils/lines.hpp"
#include "model/model.hpp"

#include <istream>
#include <string>

namespace biyos
{

Model read_native(Lines &lines);
Model read_native(std::istream &input, const std::string &file);

}
