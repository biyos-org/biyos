#include "io/nl/writer/defined.hpp"

#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "io/nl/writer/usage.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace biyos::nl
{

namespace
{

using Key = std::pair<Scope, Index>;

Key key_of(const Definition &definition) noexcept
{
    return {definition.scope, definition.record};
}

Definition definition_of(Offset node, const Reach &reach) noexcept
{
    if (reach.constraint != kNobody && reach.objective != kNobody)
    {
        return {.node = node, .scope = Scope::Both, .record = 0};
    }
    if (reach.constraint == kSeveral)
    {
        return {.node = node, .scope = Scope::Constraints, .record = 0};
    }
    if (reach.objective == kSeveral)
    {
        return {.node = node, .scope = Scope::Objectives, .record = 0};
    }
    if (reach.constraint != kNobody)
    {
        return {.node = node, .scope = Scope::Constraint, .record = reach.constraint};
    }
    return {.node = node, .scope = Scope::Objective, .record = reach.objective};
}

bool local(const Definition &definition) noexcept
{
    return definition.scope >= Scope::Constraint;
}

std::vector<bool> marked(const Graph &graph, const Usage &usage)
{
    std::vector<bool> marks(to_size(graph.size()), false);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (!usage.used(node))
        {
            continue;
        }
        const auto children = graph.children(node);
        if (graph.op(node) == Operator::PiecewiseLinear)
        {
            marks[to_size(children.front())] = true;
        }
        if (graph.op(node) == Operator::Power && graph.op(children.back()) == Operator::Constant)
        {
            marks[to_size(children.back())] = true;
        }
    }
    return marks;
}

bool spread(const Graph &graph, Offset node) noexcept
{
    return graph.op(node) == Operator::Sum && !direct(graph, node) && lead_of(graph, node) > 0;
}

}

bool direct(const Graph &graph, Offset node) noexcept
{
    const auto weights = graph.coefficients(node);
    if (graph.op(node) != Operator::Sum || !identical(graph.constant(node), 0.0) ||
        weights.size() < 2)
    {
        return false;
    }
    const auto unit = [](Real weight) { return identical(weight, 1.0); };
    const bool minus = weights.size() == 2 && unit(weights[0]) && identical(weights[1], -1.0);
    return minus || std::ranges::all_of(weights, unit);
}

Index lead_of(const Graph &graph, Offset node) noexcept
{
    if (graph.op(node) != Operator::Sum)
    {
        return 0;
    }
    const auto children = graph.children(node);
    std::size_t lead = 0;
    while (lead < children.size() && graph.op(children[lead]) == Operator::Variable)
    {
        ++lead;
    }
    const bool body = lead + 1 == children.size() && identical(graph.constant(node), 0.0) &&
                      identical(graph.coefficients(node).back(), 1.0);
    return lead == children.size() || body ? to_index(lead) : 0;
}

Defined::Defined(const Model &model, const Usage &usage)
    : constraints_(model.n_constraints()), indices_(to_size(model.graph().size()), kNobody)
{
    const Graph &graph = model.graph();
    const std::vector<bool> marks = marked(graph, usage);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        const bool wanted = usage.shared(node) || marks[to_size(node)] || spread(graph, node);
        if (usage.used(node) && graph.op(node) != Operator::Variable && wanted)
        {
            definitions_.push_back(definition_of(node, usage.node_reach(node)));
        }
    }
    std::ranges::stable_sort(definitions_, {}, key_of);
    for (std::size_t k = 0; k < definitions_.size(); ++k)
    {
        indices_[to_size(definitions_[k].node)] = to_index(k);
    }
}

bool Defined::has(Offset node) const noexcept
{
    return indices_[to_size(node)] != kNobody;
}

Index Defined::index_of(Offset node) const noexcept
{
    return indices_[to_size(node)];
}

Index Defined::link_of(const Definition &definition) const noexcept
{
    switch (definition.scope)
    {
    case Scope::Both:
    case Scope::Constraints:
    case Scope::Objectives:
        break;
    case Scope::Constraint:
        return definition.record + 1;
    case Scope::Objective:
        return constraints_ + definition.record;
    }
    return 0;
}

std::span<const Definition> Defined::common() const noexcept
{
    const auto end = std::ranges::find_if(definitions_, local);
    return {definitions_.begin(), end};
}

std::span<const Definition> Defined::of(Scope scope, Index record) const noexcept
{
    const auto found = std::ranges::equal_range(definitions_, Key{scope, record}, {}, key_of);
    return {found.begin(), found.end()};
}

std::array<Index, kScopes> Defined::counts() const noexcept
{
    std::array<Index, kScopes> counts{};
    for (const Definition &definition : definitions_)
    {
        ++counts[static_cast<std::size_t>(definition.scope)];
    }
    return counts;
}

}
