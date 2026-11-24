#pragma once

#include "expr/graph/graph.hpp"
#include "expr/intervals/interval.hpp"

#include <span>

namespace biyos
{

void enclose(const Graph &graph, std::span<const Interval> box, std::span<Interval> nodes);

}
