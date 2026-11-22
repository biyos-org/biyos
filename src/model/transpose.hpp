#pragma once

#include "core/index.hpp"
#include "model/model.hpp"

#include <span>
#include <vector>

namespace biyos
{

std::vector<Offset> variable_starts(const Model &model, Index start, Index size);
void variable_terms(const Model &model,
                    Index start,
                    std::span<const Offset> starts,
                    std::span<Index> functions,
                    std::span<double> values);

}
