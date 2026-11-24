#pragma once

#include "core/index.hpp"
#include "expr/graph/graph.hpp"
#include "expr/sparsity/reach.hpp"

#include <span>
#include <vector>

namespace biyos
{

struct Elements
{
    std::vector<Offset> nodes;
    std::vector<Offset> roots;
    std::vector<double> weights;
};

struct Entry
{
    Index row;
    Index column;
};

struct Lower
{
    std::vector<Offset> starts;
    std::vector<Index> rows;
};

struct Map
{
    std::vector<Offset> blocks;
    std::vector<Offset> starts;
    std::vector<Offset> declared;
};

Elements elements_of(const Graph &graph, std::span<const Offset> roots);
Lower lower_of(const Pattern &blocks, std::span<const Entry> declared);
Map map_of(const Lower &lower, const Pattern &blocks, std::span<const Entry> declared);
Offset position(const Lower &lower, Index row, Index column) noexcept;

}
