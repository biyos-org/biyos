#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"

#include <span>
#include <vector>

namespace biyos
{

constexpr Offset kUnset = -1;

struct Plan
{
    std::vector<Offset> home;
    std::vector<Offset> position;
    std::vector<Offset> pinned;
    std::vector<Offset> prepass;
    std::vector<Offset> partitions;
};

Plan plan_of(const Graph &graph, std::span<const Offset> roots, Offset budget);

}
