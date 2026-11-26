#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Sharing
{
    Graph graph;
    std::vector<Offset> map;
};

struct Candidate
{
    const Graph &graph;
    Offset node;
    std::span<const Offset> children;
};

Bits hash_of(const Candidate &candidate);
bool same(const Candidate &candidate, const Graph &shared, Offset image);
std::size_t slot_of(Bits hash, std::size_t size) noexcept;
Sharing share(const Graph &graph);

}
