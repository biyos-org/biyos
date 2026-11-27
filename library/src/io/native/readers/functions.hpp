#pragma once

#include "io/native/utils/body.hpp"
#include "model/model.hpp"

namespace biyos::native
{

void read_functions(Body &body, Model &model);
void read_linear(Body &body, Model &model);
void read_quadratic(Body &body, Model &model);
void read_piecewise(Body &body, Model &model);
void read_expressions(Body &body, Model &model);

}
