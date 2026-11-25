#include "model/evaluation/maps.hpp"

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

std::size_t at(Offset offset) noexcept
{
    return static_cast<std::size_t>(offset);
}

Offset count_of(std::size_t size) noexcept
{
    return static_cast<Offset>(size);
}

template <typename Term>
std::vector<Offset> starts_of(const Model &model,
                              std::span<const Term> (Model::*terms)(Index, Index) const)
{
    std::vector<Offset> starts{0};
    for (Index k = 0; k < model.n_functions(); ++k)
    {
        starts.push_back(starts.back() + count_of((model.*terms)(k, k + 1).size()));
    }
    return starts;
}

std::span<const Index> row_of(const Pattern &pattern, std::size_t row) noexcept
{
    const Offset begin = pattern.starts[row];
    return std::span(pattern.variables).subspan(at(begin), at(pattern.starts[row + 1] - begin));
}

void add_terms(const Model &model, const Slots &slots, std::vector<IndexPair> &found)
{
    const std::span<const LinearTerm> linear = model.linear_terms(0, model.n_functions());
    const std::span<const QuadraticTerm> quadratic = model.quadratic_terms(0, model.n_functions());
    for (Index k = 0; k < model.n_functions(); ++k)
    {
        const auto row = static_cast<std::size_t>(k);
        for (Offset t = slots.linear_starts[row]; t < slots.linear_starts[row + 1]; ++t)
        {
            found.push_back({.row = k, .column = linear[at(t)].variable});
        }
        for (Offset t = slots.quadratic_starts[row]; t < slots.quadratic_starts[row + 1]; ++t)
        {
            found.push_back({.row = k, .column = quadratic[at(t)].row});
            found.push_back({.row = k, .column = quadratic[at(t)].column});
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
            const Index function = callback.covered[static_cast<std::size_t>(pair.row)];
            found.push_back({.row = function, .column = pair.column});
        }
    }
}

Pattern pattern_of(Index functions, std::vector<IndexPair> found)
{
    std::ranges::sort(found);
    const auto repeated = std::ranges::unique(found);
    found.erase(repeated.begin(), repeated.end());
    Pattern pattern{.variables = {}, .starts = std::vector<Offset>(at(functions) + 1, 0)};
    pattern.variables.reserve(found.size());
    for (const IndexPair &pair : found)
    {
        ++pattern.starts[static_cast<std::size_t>(pair.row) + 1];
        pattern.variables.push_back(pair.column);
    }
    std::partial_sum(pattern.starts.begin(), pattern.starts.end(), pattern.starts.begin());
    return pattern;
}

Offset slot_of(const Pattern &pattern, Index function, Index variable) noexcept
{
    const std::span<const Index> row = row_of(pattern, static_cast<std::size_t>(function));
    const auto found = std::ranges::lower_bound(row, variable);
    return pattern.starts[static_cast<std::size_t>(function)] +
           static_cast<Offset>(found - row.begin());
}

void map_terms(const Model &model, Slots &slots)
{
    const std::span<const LinearTerm> linear = model.linear_terms(0, model.n_functions());
    const std::span<const QuadraticTerm> quadratic = model.quadratic_terms(0, model.n_functions());
    slots.linear.assign(slots.pattern.variables.size(), 0.0);
    for (Index k = 0; k < model.n_functions(); ++k)
    {
        const auto row = static_cast<std::size_t>(k);
        for (Offset t = slots.linear_starts[row]; t < slots.linear_starts[row + 1]; ++t)
        {
            const LinearTerm &term = linear[at(t)];
            slots.linear[at(slot_of(slots.pattern, k, term.variable))] += term.coefficient;
        }
        for (Offset t = slots.quadratic_starts[row]; t < slots.quadratic_starts[row + 1]; ++t)
        {
            slots.quadratic.push_back(slot_of(slots.pattern, k, quadratic[at(t)].row));
            slots.quadratic.push_back(slot_of(slots.pattern, k, quadratic[at(t)].column));
        }
    }
}

void map_tape(std::span<const Index> rooted, const Pattern &tape, Slots &slots)
{
    for (std::size_t e = 0; e < rooted.size(); ++e)
    {
        for (const Index variable : row_of(tape, e))
        {
            slots.tape.push_back(slot_of(slots.pattern, rooted[e], variable));
        }
    }
}

void map_callbacks(const Model &model, Slots &slots)
{
    slots.callback_starts.push_back(0);
    for (Index c = 0; c < model.n_callbacks(); ++c)
    {
        const Callback &callback = model.callback(c);
        for (const IndexPair &pair : callback.jacobian_pairs)
        {
            const Index function = callback.covered[static_cast<std::size_t>(pair.row)];
            slots.callbacks.push_back(slot_of(slots.pattern, function, pair.column));
        }
        slots.callback_starts.push_back(count_of(slots.callbacks.size()));
    }
}

}

Slots slots_of(const Model &model, std::span<const Index> rooted, const Pattern &tape)
{
    Slots slots{
        .pattern = {},
        .linear_starts = starts_of(model, &Model::linear_terms),
        .quadratic_starts = starts_of(model, &Model::quadratic_terms),
        .linear = {},
        .quadratic = {},
        .tape = {},
        .callbacks = {},
        .callback_starts = {},
    };
    std::vector<IndexPair> found;
    add_terms(model, slots, found);
    add_tape(rooted, tape, found);
    add_callbacks(model, found);
    slots.pattern = pattern_of(model.n_functions(), std::move(found));
    map_terms(model, slots);
    map_tape(rooted, tape, slots);
    map_callbacks(model, slots);
    return slots;
}

Declared declared_of(const Model &model)
{
    Declared declared;
    for (const QuadraticTerm &term : model.quadratic_terms(0, model.n_functions()))
    {
        declared.entries.push_back({.row = term.row, .column = term.column});
    }
    declared.callback_starts.push_back(count_of(declared.entries.size()));
    for (Index c = 0; c < model.n_callbacks(); ++c)
    {
        for (const IndexPair &pair : model.callback(c).hessian_pairs)
        {
            declared.entries.push_back({.row = pair.row, .column = pair.column});
        }
        declared.callback_starts.push_back(count_of(declared.entries.size()));
    }
    return declared;
}

}
