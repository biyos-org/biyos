#pragma once

#include "core/index.hpp"
#include "model/expressions/callbacks.hpp"

#include <span>
#include <vector>

namespace biyos
{

std::vector<IndexPair> jacobian_pairs(Index callback,
                                      std::span<const Index> covered,
                                      std::span<const Index> functions,
                                      std::span<const Index> variables,
                                      Index total);
std::vector<IndexPair> hessian_pairs(Index callback,
                                     std::span<const Index> first,
                                     std::span<const Index> second,
                                     Index total);

}
