#pragma once

#include "expr/graph/graph.hpp"

#include <span>

namespace biyos::test
{

void evaluate(const Graph &graph, std::span<const double> point, std::span<double> values);

}
