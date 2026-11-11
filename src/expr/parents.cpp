#include "expr/parents.hpp"

#include "core/index.hpp"
#include "expr/graph.hpp"

#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

std::vector<Offset> starts_of(const Graph &graph)
{
    std::vector<Offset> starts(at(graph.size()) + 1, 0);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            ++starts[at(child) + 1];
        }
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    return starts;
}

std::vector<Offset> parents_of(const Graph &graph, const std::vector<Offset> &starts)
{
    std::vector<Offset> parents(at(starts.back()));
    std::vector<Offset> next(starts.begin(), starts.end() - 1);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            parents[at(next[at(child)]++)] = node;
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
    return static_cast<Offset>(starts_.size() - 1);
}

std::span<const Offset> Parents::of(Offset node) const noexcept
{
    const Offset first = starts_[at(node)];
    const Offset count = starts_[at(node) + 1] - first;
    return std::span(parents_).subspan(at(first), at(count));
}

}
