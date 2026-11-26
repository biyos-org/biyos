#pragma once

#include "io/draft/draft.hpp"
#include "model/model.hpp"

#include <vector>

namespace biyos
{

void add_indicators(Draft &draft, const Model &model, std::vector<Index> &functions);
void add_sets(Draft &draft, const Model &model);

}
