#include "expr/graph/uses.hpp"

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"

#include <cstddef>
#include <cstdint>

namespace biyos
{

namespace
{

constexpr std::uint8_t kShared = 2;

}

Uses::Uses(const Graph &graph)
{
    extend(graph);
}

Offset Uses::size() const noexcept
{
    return to_offset(counts_.size());
}

bool Uses::shared(Offset node) const noexcept
{
    return counts_[to_size(node)] >= kShared;
}

void Uses::extend(const Graph &graph)
{
    const Offset start = size();
    counts_.resize(to_size(graph.size()), 0);
    for (Offset node = start; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            std::uint8_t &n = counts_[to_size(child)];
            if (n < kShared)
            {
                ++n;
            }
        }
    }
}

}
