#pragma once

#include "core/numbers.hpp"
#include "expr/evaluation/hessian/elements.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Read
{
    Offset part;
    Index row;
    Index color;
};

struct Coloring
{
    Index directions;
    std::vector<Index> colors;
    std::vector<Read> reads;
};

Offset star_work(std::span<const Entry> entries, std::size_t size);
Coloring star_coloring(std::span<const Entry> entries, std::size_t size);

}
