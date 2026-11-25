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

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

}

Uses::Uses(const Graph &graph)
{
    extend(graph);
}

Offset Uses::size() const noexcept
{
    return static_cast<Offset>(counts_.size());
}

bool Uses::shared(Offset node) const noexcept
{
    return counts_[at(node)] >= kShared;
}

void Uses::extend(const Graph &graph)
{
    const Offset start = size();
    counts_.resize(at(graph.size()), 0);
    for (Offset node = start; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            std::uint8_t &n = counts_[at(child)];
            if (n < kShared)
            {
                ++n;
            }
        }
    }
}

}
