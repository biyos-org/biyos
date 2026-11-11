#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"

#include <span>
#include <vector>

namespace biyos
{

constexpr Offset kDead = -1;

struct Compaction
{
    Graph graph;
    std::vector<Offset> map;
};

std::vector<bool> live(const Graph &graph, std::span<const Offset> roots);
Offset copy(const Graph &source, Offset node, std::span<const Offset> children, Graph &target);
Compaction compact(const Graph &graph, std::span<const Offset> roots);

}
