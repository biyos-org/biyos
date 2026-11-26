#pragma once

#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "model/model.hpp"

namespace biyos
{

DraftRow free_row(Real constant);
DraftRow row_shape(Index constraint, const Constraint &bounds, Real constant);

}
