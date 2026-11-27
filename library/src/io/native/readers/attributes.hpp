#pragma once

#include "io/native/utils/body.hpp"
#include "model/model.hpp"

namespace biyos::native
{

void read_names(Body &body, Model &model, Family family);
void read_tolerances(Body &body, Model &model, Family family);

}
