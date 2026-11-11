#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"

#include <span>
#include <vector>

namespace biyos
{

std::vector<bool> live(const Graph &graph, std::span<const Offset> roots);

}
