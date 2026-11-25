#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/renumber.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "constraint";

void check_bounds(Index index, const Constraint &constraint)
{
    const std::string_view fault = bound_fault(constraint.lower, constraint.upper);
    if (!fault.empty())
    {
        refuse(kNoun, index, std::string(fault));
    }
}

void check(Index index, const Constraint &constraint, Index functions)
{
    check_function(kNoun, index, constraint.function, functions);
    check_bounds(index, constraint);
}

}

Index Model::n_constraints() const noexcept
{
    return static_cast<Index>(constraints_.size());
}

const Constraint &Model::constraint(Index index) const
{
    check_index(kNoun, index, n_constraints());
    return constraints_[static_cast<std::size_t>(index)];
}

std::span<const Constraint> Model::constraints(Index start, Index stop) const
{
    check_span(kNoun, start, stop, n_constraints());
    const auto offset = static_cast<std::size_t>(start);
    return std::span(constraints_).subspan(offset, static_cast<std::size_t>(stop - start));
}

void Model::add_constraints(std::span<const Constraint> added)
{
    check_n(kNoun, constraints_, added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(static_cast<Index>(constraints_.size() + k), added[k], n_functions());
    }
    constraints_.insert(constraints_.end(), added.begin(), added.end());
}

void Model::set_constraints(std::span<const Index> indices, std::span<const Constraint> changed)
{
    if (indices.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and constraints differ in number");
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(kNoun, indices[k], n_constraints());
        check(indices[k], changed[k], n_functions());
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        constraints_[static_cast<std::size_t>(indices[k])] = changed[k];
    }
}

std::vector<Index> Model::delete_constraints(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, n_constraints(), indices);
    keep(constraints_, map);
    renumber_tolerances(Family::Constraint, map);
    renumber_names(Family::Constraint, map);
    return map;
}

}
