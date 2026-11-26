#pragma once

#include "core/events/stream.hpp"
#include "model/model.hpp"

#include <istream>
#include <string>

namespace biyos
{

enum class MpsForm
{
    Free,
    Fixed,
};

Model read_mps(Stream stream, std::istream &input, const std::string &file, MpsForm form);

}
