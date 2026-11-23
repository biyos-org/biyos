#pragma once

#include "core/index.hpp"
#include "model/callbacks.hpp"

#include <span>
#include <vector>

namespace biyos
{

void check_jacobian(Index callback,
                    std::span<const Index> functions,
                    std::span<const Offset> starts,
                    std::span<const Index> variables,
                    Index total);
std::vector<VariablePair> hessian_pairs(Index callback,
                                        std::span<const Index> first,
                                        std::span<const Index> second,
                                        Index total);

}
