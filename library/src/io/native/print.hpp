#pragma once

#include "model/model.hpp"

#include <ostream>

namespace biyos
{

void check_native(const Model &model);
void print_native(const Model &model, std::ostream &output);

}
