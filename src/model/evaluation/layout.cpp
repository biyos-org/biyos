#include "model/evaluation/layout.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/sparsity/reach.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

template <typename Term>
std::vector<Offset> starts_of(const Model &model,
                              std::span<const Term> (Model::*terms)(Index, Index) const)
{
    std::vector<Offset> starts{0};
    for (Index k = 0; k < model.n_functions(); ++k)
    {
        starts.push_back(starts.back() + to_offset((model.*terms)(k, k + 1).size()));
    }
    return starts;
}

std::span<const Index> row_of(const Pattern &pattern, std::size_t row) noexcept
{
    const Offset begin = pattern.starts[row];
    return std::span(pattern.variables)
        .subspan(to_size(begin), to_size(pattern.starts[row + 1] - begin));
}

void add_terms(const Model &model, const JacobianLayout &layout, std::vector<IndexPair> &found)
{
    const std::span<const LinearTerm> linear = model.linear_terms(0, model.n_functions());
    const std::span<const QuadraticTerm> quadratic = model.quadratic_terms(0, model.n_functions());
    for (Index k = 0; k < model.n_functions(); ++k)
    {
        const auto row = to_size(k);
        for (Offset t = layout.starts.linear[row]; t < layout.starts.linear[row + 1]; ++t)
        {
            found.push_back({.row = k, .column = linear[to_size(t)].variable});
        }
        for (Offset t = layout.starts.quadratic[row]; t < layout.starts.quadratic[row + 1]; ++t)
        {
            found.push_back({.row = k, .column = quadratic[to_size(t)].row});
            found.push_back({.row = k, .column = quadratic[to_size(t)].column});
        }
    }
}

void add_tape(std::span<const Index> rooted, const Pattern &tape, std::vector<IndexPair> &found)
{
    for (std::size_t e = 0; e < rooted.size(); ++e)
    {
        for (const Index variable : row_of(tape, e))
        {
            found.push_back({.row = rooted[e], .column = variable});
        }
    }
}

void add_callbacks(const Model &model, std::vector<IndexPair> &found)
{
    for (Index c = 0; c < model.n_callbacks(); ++c)
    {
        const Callback &callback = model.callback(c);
        for (const IndexPair &pair : callback.jacobian_pairs)
        {
            const Index function = callback.covered[to_size(pair.row)];
            found.push_back({.row = function, .column = pair.column});
        }
    }
}

Pattern pattern_of(Index functions, std::vector<IndexPair> found)
{
    std::ranges::sort(found);
    const auto repeated = std::ranges::unique(found);
    found.erase(repeated.begin(), repeated.end());
    Pattern pattern{.variables = {}, .starts = std::vector<Offset>(to_size(functions) + 1, 0)};
    pattern.variables.reserve(found.size());
    for (const IndexPair &pair : found)
    {
        ++pattern.starts[to_size(pair.row) + 1];
        pattern.variables.push_back(pair.column);
    }
    std::partial_sum(pattern.starts.begin(), pattern.starts.end(), pattern.starts.begin());
    return pattern;
}

Offset slot_of(const Pattern &pattern, Index function, Index variable) noexcept
{
    const std::span<const Index> row = row_of(pattern, to_size(function));
    const auto found = std::ranges::lower_bound(row, variable);
    return pattern.starts[to_size(function)] + static_cast<Offset>(found - row.begin());
}

void map_terms(const Model &model, JacobianLayout &layout)
{
    const std::span<const LinearTerm> linear = model.linear_terms(0, model.n_functions());
    const std::span<const QuadraticTerm> quadratic = model.quadratic_terms(0, model.n_functions());
    layout.linear.assign(layout.pattern.variables.size(), 0.0);
    for (Index k = 0; k < model.n_functions(); ++k)
    {
        const auto row = to_size(k);
        for (Offset t = layout.starts.linear[row]; t < layout.starts.linear[row + 1]; ++t)
        {
            const LinearTerm &term = linear[to_size(t)];
            layout.linear[to_size(slot_of(layout.pattern, k, term.variable))] += term.coefficient;
        }
        for (Offset t = layout.starts.quadratic[row]; t < layout.starts.quadratic[row + 1]; ++t)
        {
            const QuadraticTerm &term = quadratic[to_size(t)];
            layout.slots.quadratic.push_back(slot_of(layout.pattern, k, term.row));
            layout.slots.quadratic.push_back(slot_of(layout.pattern, k, term.column));
        }
    }
}

void map_tape(std::span<const Index> rooted, const Pattern &tape, JacobianLayout &layout)
{
    for (std::size_t e = 0; e < rooted.size(); ++e)
    {
        for (const Index variable : row_of(tape, e))
        {
            layout.slots.tape.push_back(slot_of(layout.pattern, rooted[e], variable));
        }
    }
}

void map_callbacks(const Model &model, JacobianLayout &layout)
{
    layout.starts.callbacks.push_back(0);
    for (Index c = 0; c < model.n_callbacks(); ++c)
    {
        const Callback &callback = model.callback(c);
        for (const IndexPair &pair : callback.jacobian_pairs)
        {
            const Index function = callback.covered[to_size(pair.row)];
            layout.slots.callbacks.push_back(slot_of(layout.pattern, function, pair.column));
        }
        layout.starts.callbacks.push_back(to_offset(layout.slots.callbacks.size()));
    }
}

}

JacobianLayout layout_of(const Model &model, std::span<const Index> rooted, const Pattern &tape)
{
    JacobianLayout layout;
    layout.starts.linear = starts_of(model, &Model::linear_terms);
    layout.starts.quadratic = starts_of(model, &Model::quadratic_terms);
    std::vector<IndexPair> found;
    add_terms(model, layout, found);
    add_tape(rooted, tape, found);
    add_callbacks(model, found);
    layout.pattern = pattern_of(model.n_functions(), std::move(found));
    map_terms(model, layout);
    map_tape(rooted, tape, layout);
    map_callbacks(model, layout);
    return layout;
}

Entries entries_of(const Model &model)
{
    Entries entries;
    for (const QuadraticTerm &term : model.quadratic_terms(0, model.n_functions()))
    {
        entries.items.push_back({.row = term.row, .column = term.column});
    }
    entries.starts.push_back(to_offset(entries.items.size()));
    for (Index c = 0; c < model.n_callbacks(); ++c)
    {
        for (const IndexPair &pair : model.callback(c).hessian_pairs)
        {
            entries.items.push_back({.row = pair.row, .column = pair.column});
        }
        entries.starts.push_back(to_offset(entries.items.size()));
    }
    return entries;
}

}
