#pragma once

#include "core/index.hpp"
#include "model/model.hpp"

#include <span>
#include <vector>

namespace biyos
{

std::vector<Offset> var_starts(const Model &mdl, Index beg, Index n);
void var_terms(const Model &mdl,
               Index beg,
               std::span<const Offset> starts,
               std::span<Index> funs,
               std::span<double> values);

}
