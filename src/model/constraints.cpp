#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "constraint";

void check_function(Index index, Index function, Index functions)
{
    if (function < 0 || function >= functions)
    {
        refuse(kNoun, index, item("function", function) + " is unknown");
    }
}

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
    check_function(index, constraint.function, functions);
    check_bounds(index, constraint);
}

}

Index Model::constraint_count() const noexcept
{
    return static_cast<Index>(constraints_.size());
}

const Constraint &Model::constraint(Index index) const
{
    check_index(kNoun, index, constraint_count());
    return constraints_[static_cast<std::size_t>(index)];
}

std::span<const Constraint> Model::constraints(Index first, Index count) const
{
    check_span(kNoun, first, count, constraint_count());
    const auto start = static_cast<std::size_t>(first);
    return std::span(constraints_).subspan(start, static_cast<std::size_t>(count));
}

void Model::add_constraints(std::span<const Constraint> added)
{
    check_count(kNoun, constraints_, added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(static_cast<Index>(constraints_.size() + k), added[k], function_count());
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
        check_index(kNoun, indices[k], constraint_count());
        check(indices[k], changed[k], function_count());
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        constraints_[static_cast<std::size_t>(indices[k])] = changed[k];
    }
}

}
