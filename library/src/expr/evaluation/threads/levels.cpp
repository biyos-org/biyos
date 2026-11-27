#include "expr/evaluation/threads/levels.hpp"

#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

std::size_t producer(std::span<const Block> prepass, Offset load) noexcept
{
    const auto after = std::ranges::upper_bound(prepass, load, {}, &Block::begin);
    return static_cast<std::size_t>(after - prepass.begin()) - 1;
}

std::vector<std::size_t> depths_of(std::span<const Block> prepass)
{
    std::vector<std::size_t> depths(prepass.size(), 0);
    for (std::size_t b = 0; b < prepass.size(); ++b)
    {
        for (const Offset load : prepass[b].loads)
        {
            depths[b] = (std::max)(depths[b], depths[producer(prepass, load)] + 1);
        }
    }
    return depths;
}

}

Levels levels_of(const Tape &tape)
{
    const std::vector<std::size_t> depths = depths_of(tape.blocks().first(to_size(tape.prepass())));
    const std::size_t deepest = depths.empty() ? 0 : std::ranges::max(depths) + 1;
    Levels levels{.blocks = std::vector<std::size_t>(depths.size()),
                  .starts = std::vector<Offset>(deepest + 1, 0)};
    for (const std::size_t depth : depths)
    {
        ++levels.starts[depth + 1];
    }
    std::partial_sum(levels.starts.begin(), levels.starts.end(), levels.starts.begin());
    std::vector<Offset> next(levels.starts.begin(), levels.starts.end() - 1);
    for (std::size_t b = 0; b < depths.size(); ++b)
    {
        levels.blocks[to_size(next[depths[b]]++)] = b;
    }
    return levels;
}

std::span<const std::size_t> level(const Levels &levels, std::size_t k) noexcept
{
    return slice(std::span(levels.blocks), levels.starts, k);
}

}
