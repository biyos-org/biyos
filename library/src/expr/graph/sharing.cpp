#include "expr/graph/sharing.hpp"

#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "expr/graph/compact.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

constexpr Offset kEmpty = -1;
constexpr Bits kMultiplier = 0x9e3779b97f4a7c15;
constexpr int kRotation = 5;
constexpr std::size_t kLoad = 2;

struct Slot
{
    Bits hash = 0;
    Offset node = kEmpty;
};

Bits bits(Real value) noexcept
{
    return std::bit_cast<Bits>(value);
}

Bits mix(Bits hash, Bits word) noexcept
{
    return (std::rotl(hash, kRotation) ^ word) * kMultiplier;
}

Bits mix(Bits hash, std::span<const Real> values) noexcept
{
    for (const Real value : values)
    {
        hash = mix(hash, bits(value));
    }
    return hash;
}

Bits mix_payload(Bits hash, const Graph &graph, Offset node)
{
    switch (payload_of(graph.op(node)))
    {
    case Payload::None:
        return hash;
    case Payload::Constant:
        return mix(hash, bits(graph.constant(node)));
    case Payload::Variable:
        return mix(hash, static_cast<Bits>(graph.variable(node)));
    case Payload::Coefficients:
        return mix(mix(hash, bits(graph.constant(node))), graph.coefficients(node));
    case Payload::Points:
        return mix(hash, static_cast<Bits>(graph.piecewise_linear(node)));
    }
    return hash;
}

bool same_payload(const Graph &graph, Offset node, const Graph &shared, Offset image)
{
    switch (payload_of(graph.op(node)))
    {
    case Payload::None:
        return true;
    case Payload::Constant:
        return identical(graph.constant(node), shared.constant(image));
    case Payload::Variable:
        return graph.variable(node) == shared.variable(image);
    case Payload::Coefficients:
        return identical(graph.constant(node), shared.constant(image)) &&
               std::ranges::equal(graph.coefficients(node), shared.coefficients(image), identical);
    case Payload::Points:
        return graph.piecewise_linear(node) == shared.piecewise_linear(image);
    }
    return false;
}

std::vector<Slot> table_for(Offset nodes)
{
    return std::vector<Slot>(std::bit_ceil(kLoad * to_size(nodes)));
}

bool fits(const Slot &slot, const Candidate &candidate, Bits hash, const Graph &shared)
{
    if (slot.node == kEmpty)
    {
        return true;
    }
    return slot.hash == hash && same(candidate, shared, slot.node);
}

Slot &probe(std::vector<Slot> &table, const Candidate &candidate, Bits hash, const Graph &shared)
{
    const std::size_t mask = table.size() - 1;
    std::size_t index = slot_of(hash, table.size());
    while (!fits(table[index], candidate, hash, shared))
    {
        index = (index + 1) & mask;
    }
    return table[index];
}

Offset merge(const Candidate &candidate, std::vector<Slot> &table, Graph &shared)
{
    const Bits hash = hash_of(candidate);
    Slot &slot = probe(table, candidate, hash, shared);
    if (slot.node == kEmpty)
    {
        const Offset image = copy(candidate.graph, candidate.node, candidate.children, shared);
        slot = {.hash = hash, .node = image};
    }
    return slot.node;
}

}

Bits hash_of(const Candidate &candidate)
{
    const Graph &graph = candidate.graph;
    const auto op = static_cast<Bits>(graph.op(candidate.node));
    Bits hash = mix_payload(op, graph, candidate.node);
    for (const Offset child : candidate.children)
    {
        hash = mix(hash, static_cast<Bits>(child));
    }
    return hash;
}

std::size_t slot_of(Bits hash, std::size_t size) noexcept
{
    return static_cast<std::size_t>(std::rotl(hash, std::countr_zero(size))) & (size - 1);
}

bool same(const Candidate &candidate, const Graph &shared, Offset image)
{
    const Graph &graph = candidate.graph;
    return graph.op(candidate.node) == shared.op(image) &&
           std::ranges::equal(candidate.children, shared.children(image)) &&
           same_payload(graph, candidate.node, shared, image);
}

Sharing share(const Graph &graph)
{
    Sharing result{
        .graph = Graph(graph.points()),
        .map = std::vector<Offset>(to_size(graph.size())),
    };
    std::vector<Slot> table = table_for(graph.size());
    std::vector<Offset> children;
    children.reserve(most_children(graph));
    for (Offset node = 0; node < graph.size(); ++node)
    {
        const auto mapped = map_children(graph.children(node), result.map, children);
        const Candidate candidate{graph, node, mapped};
        result.map[to_size(node)] = merge(candidate, table, result.graph);
    }
    return result;
}

}
