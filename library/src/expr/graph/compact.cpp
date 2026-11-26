#include "expr/graph/compact.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

void check_apart(const Graph &source, const Graph &target)
{
    if (&source == &target)
    {
        throw Error(ErrorKind::InvalidArgument,
                    "expression",
                    "source and target are the same graph");
    }
}

}

std::size_t most_children(const Graph &graph) noexcept
{
    std::size_t width = 0;
    for (Offset node = 0; node < graph.size(); ++node)
    {
        width = (std::max)(width, graph.children(node).size());
    }
    return width;
}

std::span<const Offset> map_children(std::span<const Offset> nodes,
                                     std::span<const Offset> map,
                                     std::vector<Offset> &workspace)
{
    workspace.clear();
    for (const Offset node : nodes)
    {
        workspace.push_back(map[to_size(node)]);
    }
    return workspace;
}

std::vector<bool> live(const Graph &graph, std::span<const Offset> roots)
{
    std::vector<bool> marks(to_size(graph.size()), false);
    for (const Offset root : roots)
    {
        check_child(root, graph.size());
        marks[to_size(root)] = true;
    }
    for (Offset node = graph.size() - 1; node >= 0; --node)
    {
        if (!marks[to_size(node)])
        {
            continue;
        }
        for (const Offset child : graph.children(node))
        {
            marks[to_size(child)] = true;
        }
    }
    return marks;
}

Offset copy(const Graph &source, Offset node, std::span<const Offset> children, Graph &target)
{
    check_apart(source, target);
    const Operator op = source.op(node);
    switch (payload_of(op))
    {
    case Payload::None:
        break;
    case Payload::Constant:
        return target.add(op, source.constant(node), children);
    case Payload::Variable:
        check_shape(op, Payload::Variable, to_offset(children.size()));
        return target.add_variable(source.variable(node));
    case Payload::Coefficients:
        return target.add_sum(source.constant(node), children, source.coefficients(node));
    case Payload::Points:
        return target.add_piecewise(source.piecewise_linear(node), children);
    }
    return target.add(op, children);
}

Compaction compact(const Graph &graph, std::span<const Offset> roots)
{
    const std::vector<bool> marks = live(graph, roots);
    Compaction result{
        .graph = Graph(graph.points()),
        .map = std::vector<Offset>(to_size(graph.size()), kDead),
    };
    std::vector<Offset> children;
    children.reserve(most_children(graph));
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (marks[to_size(node)])
        {
            const auto mapped = map_children(graph.children(node), result.map, children);
            result.map[to_size(node)] = copy(graph, node, mapped, result.graph);
        }
    }
    return result;
}

}
