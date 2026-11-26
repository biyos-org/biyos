#pragma once

#include "model/model.hpp"

#include <ostream>

namespace biyos::native
{

void print_functions(const Model &model, std::ostream &output);
void print_linear(const Model &model, std::ostream &output);
void print_quadratic(const Model &model, std::ostream &output);
void print_piecewise(const Model &model, std::ostream &output);
void print_expressions(const Model &model, std::ostream &output);

}
