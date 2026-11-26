#include "io/draft/draft.hpp"

#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "expr/forms/terms.hpp"
#include "model/model.hpp"

#include <cmath>
#include <cstddef>
#include <numeric>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace biyos
{

VariableKind kind_of(const DraftColumn &column) noexcept
{
    const bool integer = column.marked || column.integer;
    if (column.binary)
    {
        return VariableKind::Binary;
    }
    if (column.semi)
    {
        return integer ? VariableKind::SemiInteger : VariableKind::SemiContinuous;
    }
    return integer ? VariableKind::Integer : VariableKind::Continuous;
}

Real lower_of(const DraftColumn &column) noexcept
{
    if (column.lower)
    {
        return *column.lower;
    }
    return negative_upper(column) ? -kInfinity : 0.0;
}

Real upper_of(const DraftColumn &column) noexcept
{
    if (column.upper)
    {
        return *column.upper;
    }
    return column.marked && !column.lower ? 1.0 : kInfinity;
}

Constraint constraint_of(Index function, const DraftRow &row) noexcept
{
    const Real rhs = row.rhs.value_or(0.0);
    const Real range = row.range.value_or(0.0);
    switch (row.sense)
    {
    case RowSense::Free:
        return Constraint{.function = function, .lower = -kInfinity, .upper = kInfinity};
    case RowSense::Less:
        return Constraint{
            .function = function,
            .lower = row.range ? rhs - std::fabs(range) : -kInfinity,
            .upper = rhs,
        };
    case RowSense::Greater:
        return Constraint{
            .function = function,
            .lower = rhs,
            .upper = row.range ? rhs + std::fabs(range) : kInfinity,
        };
    case RowSense::Equal:
        break;
    }
    if (range < 0.0)
    {
        return Constraint{.function = function, .lower = rhs + range, .upper = rhs};
    }
    return Constraint{.function = function, .lower = rhs, .upper = rhs + range};
}

namespace
{

std::optional<Index> objective_row(const Draft &draft) noexcept
{
    for (std::size_t k = 0; k < draft.rows.size(); ++k)
    {
        if (draft.rows[k].sense == RowSense::Free)
        {
            return to_index(k);
        }
    }
    return std::nullopt;
}

void add_variables(Model &model, const Draft &draft)
{
    std::vector<Variable> variables;
    variables.reserve(draft.columns.size());
    for (const DraftColumn &column : draft.columns)
    {
        variables.push_back(Variable{
            .kind = kind_of(column),
            .lower = lower_of(column),
            .upper = upper_of(column),
        });
    }
    model.add_variables(variables);
    std::vector<Index> indices(variables.size());
    std::iota(indices.begin(), indices.end(), Index{0});
    model.set_names(Family::Variable, indices, draft.column_names.names());
}

void add_functions(Model &model, const Draft &draft)
{
    std::vector<Real> constants;
    constants.reserve(draft.rows.size());
    for (const DraftRow &row : draft.rows)
    {
        const bool free = row.sense == RowSense::Free;
        constants.push_back(free ? 0.0 - row.rhs.value_or(0.0) : 0.0);
    }
    std::vector<Offset> starts(draft.rows.size() + 1, 0);
    for (const DraftEntry &entry : draft.entries)
    {
        ++starts[to_size(entry.row) + 1];
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    std::vector<Offset> next(starts.begin(), starts.end() - 1);
    std::vector<LinearTerm> terms(draft.entries.size());
    for (const DraftEntry &entry : draft.entries)
    {
        const auto at = to_size(next[to_size(entry.row)]++);
        terms[at] = LinearTerm{.variable = entry.column, .coefficient = entry.value};
    }
    model.add_functions(constants, starts, std::move(terms));
}

void add_constraints(Model &model, const Draft &draft, std::optional<Index> objective)
{
    std::vector<Constraint> constraints;
    std::vector<std::string_view> names;
    for (std::size_t k = 0; k < draft.rows.size(); ++k)
    {
        const auto row = to_index(k);
        if (row != objective)
        {
            constraints.push_back(constraint_of(row, draft.rows[k]));
            names.push_back(draft.row_names.names()[k]);
        }
    }
    model.add_constraints(constraints);
    std::vector<Index> indices(constraints.size());
    std::iota(indices.begin(), indices.end(), Index{0});
    model.set_names(Family::Constraint, indices, names);
}

void add_objective(Model &model, const Draft &draft, Index row)
{
    Objective objective = default_objective(row, 0);
    objective.sense = draft.sense;
    model.add_objectives(std::span(&objective, 1));
    const Index index = 0;
    const std::string_view name = draft.row_names.names()[to_size(row)];
    model.set_names(Family::Objective, std::span(&index, 1), std::span(&name, 1));
}

void add_sets(Model &model, const Draft &draft)
{
    std::vector<SpecialOrderedSetKind> kinds;
    std::vector<Offset> starts{0};
    std::vector<Member> members;
    for (const DraftSet &set : draft.sets)
    {
        kinds.push_back(set.kind);
        members.insert(members.end(), set.members.begin(), set.members.end());
        starts.push_back(to_offset(members.size()));
    }
    model.add_special_ordered_sets(kinds, starts, members);
}

void name_sets(Model &model, const Draft &draft)
{
    std::vector<Index> indices;
    std::vector<std::string_view> names;
    for (std::size_t k = 0; k < draft.sets.size(); ++k)
    {
        if (draft.sets[k].name)
        {
            indices.push_back(to_index(k));
            names.push_back(*draft.sets[k].name);
        }
    }
    model.set_names(Family::SpecialOrderedSet, indices, names);
}

}

bool negative_upper(const DraftColumn &column) noexcept
{
    return !column.lower && column.upper && *column.upper < 0.0;
}

Model build(const Draft &draft)
{
    Model model;
    if (draft.name)
    {
        model.set_name(*draft.name);
    }
    const std::optional<Index> objective = objective_row(draft);
    add_variables(model, draft);
    add_functions(model, draft);
    add_constraints(model, draft, objective);
    if (objective)
    {
        add_objective(model, draft, *objective);
    }
    add_sets(model, draft);
    name_sets(model, draft);
    return model;
}

}
