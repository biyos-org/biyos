#include "io/nl/writer/layout.hpp"

#include "core/numbers.hpp"
#include "io/nl/writer/refusals.hpp"
#include "io/nl/writer/usage.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace biyos::nl
{

namespace
{

enum class Rank
{
    Both,
    BothIntegers,
    Constraints,
    ConstraintIntegers,
    Objectives,
    ObjectiveIntegers,
    Linear,
    Binaries,
    Integers,
};

constexpr std::size_t kRanks = 9;

using Sizes = std::array<Index, kRanks>;

Rank linear_rank(VariableKind kind) noexcept
{
    switch (kind)
    {
    case VariableKind::Continuous:
    case VariableKind::SemiContinuous:
        return Rank::Linear;
    case VariableKind::Binary:
        return Rank::Binaries;
    case VariableKind::Integer:
    case VariableKind::SemiInteger:
        break;
    }
    return Rank::Integers;
}

Rank nonlinear_rank(Rank continuous, VariableKind kind) noexcept
{
    const bool integer = linear_rank(kind) != Rank::Linear;
    return static_cast<Rank>(static_cast<int>(continuous) + (integer ? 1 : 0));
}

Rank rank_of(VariableKind kind, const Reach &reach) noexcept
{
    const bool constraints = reach.constraint != kNobody;
    const bool objectives = reach.objective != kNobody;
    if (constraints && objectives)
    {
        return nonlinear_rank(Rank::Both, kind);
    }
    if (constraints)
    {
        return nonlinear_rank(Rank::Constraints, kind);
    }
    if (objectives)
    {
        return nonlinear_rank(Rank::Objectives, kind);
    }
    return linear_rank(kind);
}

Index size_of(const Sizes &sizes, Rank rank) noexcept
{
    return sizes[static_cast<std::size_t>(rank)];
}

Counts counts_of(const Sizes &sizes) noexcept
{
    const Index both = size_of(sizes, Rank::Both) + size_of(sizes, Rank::BothIntegers);
    const Index constraints =
        both + size_of(sizes, Rank::Constraints) + size_of(sizes, Rank::ConstraintIntegers);
    const Index objectives =
        size_of(sizes, Rank::Objectives) + size_of(sizes, Rank::ObjectiveIntegers);
    return {
        .in_constraints = constraints,
        .in_objectives = objectives > 0 ? constraints + objectives : both,
        .in_both = both,
        .binaries = size_of(sizes, Rank::Binaries),
        .integers = size_of(sizes, Rank::Integers),
        .both_integers = size_of(sizes, Rank::BothIntegers),
        .constraint_integers = size_of(sizes, Rank::ConstraintIntegers),
        .objective_integers = size_of(sizes, Rank::ObjectiveIntegers),
    };
}

}

bool nonlinear(const Model &model, Index function)
{
    return !model.quadratic_terms(function, function + 1).empty() ||
           model.roots(function, function + 1).front() != kNoRoot;
}

Records records_of(const Model &model)
{
    check_records(model);
    Records records{.constraints = {}, .nonlinear_constraints = 0, .nonlinear_objectives = 0};
    for (const bool wanted : {true, false})
    {
        for (Index k = 0; k < model.n_constraints(); ++k)
        {
            if (nonlinear(model, model.constraint(k).function) == wanted)
            {
                records.constraints.push_back(k);
            }
        }
        if (wanted)
        {
            records.nonlinear_constraints = to_index(records.constraints.size());
        }
    }
    for (Index q = 0; q < model.n_objectives(); ++q)
    {
        if (nonlinear(model, model.objective(q).function))
        {
            records.nonlinear_objectives = q + 1;
        }
    }
    return records;
}

Columns columns_of(const Model &model, const Usage &usage)
{
    Columns columns{.order = {}, .position = {}, .counts = {}, .integral_binaries = false};
    std::vector<Rank> ranks;
    Sizes sizes{};
    for (Index k = 0; k < model.n_variables(); ++k)
    {
        const VariableKind kind = model.variable(k).kind;
        const Rank rank = rank_of(kind, usage.variable_reach(k));
        ranks.push_back(rank);
        ++sizes[static_cast<std::size_t>(rank)];
        columns.order.push_back(k);
        columns.integral_binaries |= kind == VariableKind::Binary && rank < Rank::Linear;
    }
    const auto rank_at = [&ranks](Index k) { return ranks[to_size(k)]; };
    std::ranges::stable_sort(columns.order, {}, rank_at);
    columns.position.resize(columns.order.size());
    for (std::size_t k = 0; k < columns.order.size(); ++k)
    {
        columns.position[to_size(columns.order[k])] = to_index(k);
    }
    columns.counts = counts_of(sizes);
    return columns;
}

}
