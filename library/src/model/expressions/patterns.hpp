#pragma once

#include "core/numbers.hpp"
#include "model/expressions/callbacks.hpp"

#include <span>
#include <vector>

namespace biyos
{

std::vector<IndexPair> jacobian_pairs(Index callback,
                                      const Callback &held,
                                      std::span<const Index> functions,
                                      std::span<const Index> variables,
                                      Index total);
std::vector<IndexPair> hessian_pairs(Index callback,
                                     const Callback &held,
                                     std::span<const Index> rows,
                                     std::span<const Index> columns,
                                     Index total);

}
