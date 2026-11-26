#pragma once

#include "io/native/utils/body.hpp"
#include "model/model.hpp"

namespace biyos::native
{

void read_variables(Body &body, Model &model);
void read_constraints(Body &body, Model &model);
void read_objectives(Body &body, Model &model);
void read_indicators(Body &body, Model &model);
void read_sets(Body &body, Model &model);
void read_cones(Body &body, Model &model);
void read_complements(Body &body, Model &model);

}
