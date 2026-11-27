#pragma once

#include "expr/analysis/intervals/interval.hpp"
#include "expr/graph/graph.hpp"

#include <span>

namespace biyos
{

void enclose(const Graph &graph, std::span<const Interval> box, std::span<Interval> nodes);

}
