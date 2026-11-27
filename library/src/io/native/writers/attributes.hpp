#pragma once

#include "model/model.hpp"

#include <ostream>

namespace biyos::native
{

void print_name(const Model &model, std::ostream &output);
void print_names(const Model &model, std::ostream &output, Family family);
void print_tolerances(const Model &model, std::ostream &output, Family family);

}
