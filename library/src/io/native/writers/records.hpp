#pragma once

#include "model/model.hpp"

#include <ostream>

namespace biyos::native
{

void print_variables(const Model &model, std::ostream &output);
void print_constraints(const Model &model, std::ostream &output);
void print_objectives(const Model &model, std::ostream &output);
void print_indicators(const Model &model, std::ostream &output);
void print_sets(const Model &model, std::ostream &output);
void print_cones(const Model &model, std::ostream &output);
void print_complements(const Model &model, std::ostream &output);

}
