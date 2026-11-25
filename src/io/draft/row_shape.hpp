#pragma once

#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "model/model.hpp"

namespace biyos
{

DraftRow free_row(double constant);
DraftRow row_shape(Index constraint, const Constraint &bounds, double constant);

}
