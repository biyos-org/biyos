#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"

#include <span>
#include <vector>

namespace biyos
{

struct Range
{
    Offset begin;
    Offset end;
};

struct Route
{
    std::vector<Range> runs;
    std::vector<Offset> starts;
};

struct Pattern
{
    std::vector<Index> variables;
    std::vector<Offset> starts;
};

struct Reach
{
    Route route;
    Pattern pattern;
};

Reach reach_of(const Graph &graph,
               std::span<const Offset> positions,
               std::span<const Offset> roots);

}
