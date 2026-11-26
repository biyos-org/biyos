#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"

#include <cstddef>
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

std::size_t most_children(const Graph &graph) noexcept;
std::span<const Offset> map_children(std::span<const Offset> nodes,
                                     std::span<const Offset> map,
                                     std::vector<Offset> &workspace);
std::vector<bool> live(const Graph &graph, std::span<const Offset> roots);
Offset copy(const Graph &source, Offset node, std::span<const Offset> children, Graph &target);
Compaction compact(const Graph &graph, std::span<const Offset> roots);

}
