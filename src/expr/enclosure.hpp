#pragma once

#include "expr/graph.hpp"
#include "expr/interval.hpp"

#include <span>

namespace biyos
{

void enclose(const Graph &graph, std::span<const Interval> box, std::span<Interval> nodes);

}
