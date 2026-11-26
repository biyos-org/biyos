#pragma once

#include "core/events/stream.hpp"
#include "model/model.hpp"

#include <string>

namespace biyos
{

std::string difference(const Model &a, const Model &b);
bool compare(Stream stream, const Model &a, const Model &b);

}
