#pragma once

#include "core/index.hpp"
#include "io/draft/draft.hpp"
#include "model/model.hpp"

namespace biyos
{

DraftColumn column_shape(Index variable, const Variable &bounds);

}
