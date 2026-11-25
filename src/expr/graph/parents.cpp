#include "expr/graph/parents.hpp"

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"

#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

std::vector<Offset> starts_of(const Graph &graph)
{
    std::vector<Offset> starts(to_size(graph.size()) + 1, 0);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            ++starts[to_size(child) + 1];
        }
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    return starts;
}

std::vector<Offset> parents_of(const Graph &graph, const std::vector<Offset> &starts)
{
    std::vector<Offset> parents(to_size(starts.back()));
    std::vector<Offset> next(starts.begin(), starts.end() - 1);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            parents[to_size(next[to_size(child)]++)] = node;
        }
    }
    return parents;
}

}

Parents::Parents(const Graph &graph)
    : starts_(starts_of(graph)), parents_(parents_of(graph, starts_))
{
}

Offset Parents::size() const noexcept
{
    return to_offset(starts_.size() - 1);
}

std::span<const Offset> Parents::of(Offset node) const noexcept
{
    const Offset start = starts_[to_size(node)];
    const Offset n = starts_[to_size(node) + 1] - start;
    return std::span(parents_).subspan(to_size(start), to_size(n));
}

}
