#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/sparsity/reach.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Elements
{
    std::vector<Offset> nodes;
    std::vector<Offset> roots;
    std::vector<Real> weights;
};

struct Entry
{
    Index row;
    Index column;
};

struct Blocks
{
    std::vector<Offset> starts;
    std::vector<Entry> entries;
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

std::span<const Index> variables_of(const Pattern &pattern, Offset element) noexcept;
std::span<const Entry> entries_of(const Blocks &blocks, Offset element) noexcept;
Elements elements_of(const Graph &graph, std::span<const Offset> roots);
void add_dense(std::size_t size, Blocks &blocks);
Blocks dense_of(const Pattern &pattern);
Lower lower_of(const Pattern &pattern, const Blocks &blocks, std::span<const Entry> declared);
Map map_of(const Lower &lower,
           const Pattern &pattern,
           const Blocks &blocks,
           std::span<const Entry> declared);
Offset position(const Lower &lower, Index row, Index column) noexcept;

}
