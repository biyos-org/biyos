#pragma once

#include "core/numeric/tolerances.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/graph/graph.hpp"

#include <span>

namespace biyos
{

void enclose(const Graph &graph,
             std::span<const Interval> box,
             const Tolerances &tolerances,
             std::span<Interval> nodes);

}
