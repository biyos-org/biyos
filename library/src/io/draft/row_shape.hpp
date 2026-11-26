#pragma once

#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "model/model.hpp"

#include <string>

namespace biyos
{

DraftRow free_row(Real constant);
DraftRow row_shape(const std::string &record, const Constraint &bounds, Real constant);

}
