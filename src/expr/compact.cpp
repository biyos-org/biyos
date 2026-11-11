#include "expr/compact.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"

#include <algorithm>
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

void check_apart(const Graph &source, const Graph &target)
{
    if (&source == &target)
    {
        throw Error(ErrorKind::InvalidArgument,
                    "expression",
                    "source and target are the same graph");
    }
}

std::size_t widest(const Graph &graph) noexcept
{
    std::size_t width = 0;
    for (Offset node = 0; node < graph.size(); ++node)
    {
        width = (std::max)(width, graph.children(node).size());
    }
    return width;
}

std::span<const Offset> images(std::span<const Offset> nodes,
                               std::span<const Offset> map,
                               std::vector<Offset> &workspace)
{
    workspace.clear();
    for (const Offset node : nodes)
    {
        workspace.push_back(map[at(node)]);
    }
    return workspace;
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
        check_shape(op, Payload::Variable, static_cast<Offset>(children.size()));
        return target.add_variable(source.variable(node));
    case Payload::Coefficients:
        return target.add_sum(source.constant(node), children, source.coefficients(node));
    }
    return target.add(op, children);
}

Compaction compact(const Graph &graph, std::span<const Offset> roots)
{
    const std::vector<bool> marks = live(graph, roots);
    Compaction result;
    result.map.assign(at(graph.size()), kDead);
    std::vector<Offset> children;
    children.reserve(widest(graph));
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (marks[at(node)])
        {
            const auto mapped = images(graph.children(node), result.map, children);
            result.map[at(node)] = copy(graph, node, mapped, result.graph);
        }
    }
    return result;
}

}
