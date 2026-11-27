#pragma once

#include "core/numbers.hpp"
#include "expr/graph/node.hpp"
#include "model/model.hpp"

namespace biyos::nl
{

void check_records(const Model &model);
void check_operator(Operator op, Index function);

}
