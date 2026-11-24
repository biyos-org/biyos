#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/numeric/zero.hpp"
#include "core/text/text.hpp"
#include "model/checks.hpp"
#include "model/renumber.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "objective";

void check_sense(Index index, ObjectiveSense sense)
{
    if (std::ranges::find(kSenses, sense) == kSenses.end())
    {
        refuse(kNoun, index, "sense " + text_of(static_cast<int>(sense)) + " is unknown");
    }
}

void check_weight(Index index, double weight)
{
    const std::string_view fault = value_fault(weight);
    if (!fault.empty())
    {
        refuse(kNoun, index, "weight " + std::string(fault));
    }
    if (std::signbit(weight) || is_zero(weight))
    {
        refuse(kNoun, index, "weight " + text_of(weight) + " is not positive");
    }
}

void check_tolerance(Index index, std::string_view name, double tolerance)
{
    const bool unlimited = std::isinf(tolerance) && !std::signbit(tolerance);
    const std::string_view fault = unlimited ? std::string_view() : value_fault(tolerance);
    if (!fault.empty())
    {
        refuse(kNoun, index, std::string(name) + " tolerance " + std::string(fault));
    }
    if (std::signbit(tolerance) && !is_zero(tolerance))
    {
        const std::string value = text_of(tolerance);
        refuse(kNoun, index, std::string(name) + " tolerance " + value + " is negative");
    }
}

void check(Index index, const Objective &objective, Index functions)
{
    check_function(kNoun, index, objective.function, functions);
    check_sense(index, objective.sense);
    check_weight(index, objective.weight);
    check_tolerance(index, "absolute", objective.absolute);
    check_tolerance(index, "relative", objective.relative);
}

}

Objective default_objective(Index function, Index index) noexcept
{
    return Objective{
        .function = function,
        .sense = ObjectiveSense::Minimize,
        .priority = index,
        .weight = 1.0,
        .absolute = 0.0,
        .relative = 0.0,
    };
}

Index Model::n_objectives() const noexcept
{
    return static_cast<Index>(objectives_.size());
}

const Objective &Model::objective(Index index) const
{
    check_index(kNoun, index, n_objectives());
    return objectives_[static_cast<std::size_t>(index)];
}

std::span<const Objective> Model::objectives(Index start, Index size) const
{
    check_span(kNoun, start, size, n_objectives());
    const auto offset = static_cast<std::size_t>(start);
    return std::span(objectives_).subspan(offset, static_cast<std::size_t>(size));
}

void Model::check_objective_room(std::size_t added) const
{
    check_n(kNoun, objectives_, added);
}

void Model::add_objectives(std::span<const Objective> added)
{
    check_objective_room(added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(static_cast<Index>(objectives_.size() + k), added[k], n_functions());
    }
    objectives_.insert(objectives_.end(), added.begin(), added.end());
}

void Model::set_objectives(std::span<const Index> indices, std::span<const Objective> changed)
{
    if (indices.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and objectives differ in number");
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(kNoun, indices[k], n_objectives());
        check(indices[k], changed[k], n_functions());
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        objectives_[static_cast<std::size_t>(indices[k])] = changed[k];
    }
}

std::vector<Index> Model::delete_objectives(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, n_objectives(), indices);
    keep(objectives_, map);
    renumber_names(Family::Objective, map);
    return map;
}

}
