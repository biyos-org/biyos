#include "io/nl/writer/entries.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "io/nl/writer/usage.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <span>

namespace biyos::nl
{

Entries::Entries(const Model &model, const Usage &usage, std::span<const Index> position)
    : starts_{0}, seen_(to_size(model.graph().size()), kNobody),
      held_(to_size(model.n_variables()), kNobody)
{
    for (Index function = 0; function < model.n_functions(); ++function)
    {
        if (reached(usage.function_reach(function)))
        {
            collect(model, function, position);
        }
        starts_.push_back(to_offset(terms_.size()));
    }
}

std::span<const LinearTerm> Entries::of(Index function) const noexcept
{
    const auto begin = to_size(starts_[to_size(function)]);
    const auto end = to_size(starts_[to_size(function) + 1]);
    return std::span(terms_).subspan(begin, end - begin);
}

void Entries::collect(const Model &model, Index function, std::span<const Index> position)
{
    const auto first = terms_.size();
    for (const LinearTerm &term : model.linear_terms(function, function + 1))
    {
        held_[to_size(term.variable)] = function;
        terms_.push_back(
            {.variable = position[to_size(term.variable)], .coefficient = term.coefficient});
    }
    for (const QuadraticTerm &term : model.quadratic_terms(function, function + 1))
    {
        add_variable(term.row, function, position);
        add_variable(term.column, function, position);
    }
    const Offset root = model.roots(function, function + 1).front();
    if (root != kNoRoot)
    {
        visit(model.graph(), root, function, position);
    }
    const auto added = std::span(terms_).subspan(first);
    std::ranges::sort(added, {}, &LinearTerm::variable);
}

void Entries::visit(const Graph &graph, Offset root, Index stamp, std::span<const Index> position)
{
    stack_.assign(1, root);
    seen_[to_size(root)] = stamp;
    while (!stack_.empty())
    {
        const Offset node = stack_.back();
        stack_.pop_back();
        if (graph.op(node) == Operator::Variable)
        {
            add_variable(graph.variable(node), stamp, position);
        }
        for (const Offset child : graph.children(node))
        {
            if (seen_[to_size(child)] != stamp)
            {
                seen_[to_size(child)] = stamp;
                stack_.push_back(child);
            }
        }
    }
}

void Entries::add_variable(Index variable, Index stamp, std::span<const Index> position)
{
    if (held_[to_size(variable)] != stamp)
    {
        held_[to_size(variable)] = stamp;
        terms_.push_back({.variable = position[to_size(variable)], .coefficient = 0.0});
    }
}

}
