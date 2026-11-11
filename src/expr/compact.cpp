#include "expr/compact.hpp"

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"

#include <cstddef>
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

}

std::vector<bool> live(const Graph &graph, std::span<const Offset> roots)
{
    std::vector<bool> marks(at(graph.size()), false);
    for (const Offset root : roots)
    {
        check_child(root, graph.size());
        marks[at(root)] = true;
    }
    for (Offset node = graph.size() - 1; node >= 0; --node)
    {
        if (!marks[at(node)])
        {
            continue;
        }
        for (const Offset child : graph.children(node))
        {
            marks[at(child)] = true;
        }
    }
    return marks;
}

}
