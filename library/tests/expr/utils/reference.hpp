#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"

#include <span>

namespace biyos::test
{

void evaluate(const Graph &graph, std::span<const Real> point, std::span<Real> values);

}
