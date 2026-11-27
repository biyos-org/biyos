#include "io/nl/writer/usage.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "io/nl/writer/refusals.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace biyos::nl
{

namespace
{

constexpr std::uint8_t kShared = 2;

Index joined(Index held, Index added) noexcept
{
    if (added == kNobody || held == added)
    {
        return held;
    }
    return held == kNobody ? added : kSeveral;
}

void join(Reach &held, const Reach &added) noexcept
{
    held.constraint = joined(held.constraint, added.constraint);
    held.objective = joined(held.objective, added.objective);
}

}

bool reached(const Reach &reach) noexcept
{
    return reach.constraint != kNobody || reach.objective != kNobody;
}

Usage::Usage(const Model &model, std::span<const Index> constraints)
{
    reach_functions(model, constraints);
    reach_roots(model);
    reach_nodes(model.graph());
    reach_variables(model);
}

bool Usage::used(Offset node) const noexcept
{
    return reached(nodes_[to_size(node)]);
}

bool Usage::shared(Offset node) const noexcept
{
    return counts_[to_size(node)] >= kShared;
}

const Reach &Usage::node_reach(Offset node) const noexcept
{
    return nodes_[to_size(node)];
}

const Reach &Usage::function_reach(Index function) const noexcept
{
    return functions_[to_size(function)];
}

const Reach &Usage::variable_reach(Index variable) const noexcept
{
    return variables_[to_size(variable)];
}

void Usage::reach_functions(const Model &model, std::span<const Index> constraints)
{
    functions_.assign(to_size(model.n_functions()), kUnreached);
    for (std::size_t m = 0; m < constraints.size(); ++m)
    {
        const Index function = model.constraint(constraints[m]).function;
        join(functions_[to_size(function)], Reach{.constraint = to_index(m), .objective = kNobody});
    }
    for (Index q = 0; q < model.n_objectives(); ++q)
    {
        const Index function = model.objective(q).function;
        join(functions_[to_size(function)], Reach{.constraint = kNobody, .objective = q});
    }
}

void Usage::reach_roots(const Model &model)
{
    const auto size = to_size(model.graph().size());
    nodes_.assign(size, kUnreached);
    counts_.assign(size, 0);
    owners_.assign(size, kNobody);
    for (Index function = 0; function < model.n_functions(); ++function)
    {
        const Offset root = model.roots(function, function + 1).front();
        if (root != kNoRoot && reached(function_reach(function)))
        {
            pass(root, function_reach(function), function);
        }
    }
}

void Usage::reach_nodes(const Graph &graph)
{
    for (Offset node = graph.size() - 1; node >= 0; --node)
    {
        if (!used(node))
        {
            continue;
        }
        const Index owner = owners_[to_size(node)];
        check_operator(graph.op(node), owner);
        for (const Offset child : graph.children(node))
        {
            pass(child, node_reach(node), owner);
        }
    }
}

void Usage::reach_variables(const Model &model)
{
    const Graph &graph = model.graph();
    variables_.assign(to_size(model.n_variables()), kUnreached);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (used(node) && graph.op(node) == Operator::Variable)
        {
            join(variables_[to_size(graph.variable(node))], node_reach(node));
        }
    }
    for (Index function = 0; function < model.n_functions(); ++function)
    {
        for (const QuadraticTerm &term : model.quadratic_terms(function, function + 1))
        {
            join(variables_[to_size(term.row)], function_reach(function));
            join(variables_[to_size(term.column)], function_reach(function));
        }
    }
}

void Usage::pass(Offset node, const Reach &reach, Index owner)
{
    const auto at = to_size(node);
    join(nodes_[at], reach);
    if (counts_[at] < kShared)
    {
        ++counts_[at];
    }
    if (owners_[at] == kNobody)
    {
        owners_[at] = owner;
    }
}

}
