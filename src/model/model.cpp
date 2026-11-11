#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "model/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr auto kRoom = static_cast<std::size_t>((std::numeric_limits<Index>::max)());
constexpr std::string_view kNoun = "variable";

[[noreturn]] void refuse(Index index, const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, item(kNoun, index), reason);
}

std::string interval(double lower, double upper)
{
    return "[" + text_of(lower) + ", " + text_of(upper) + "]";
}

void check_kind(Index index, VariableKind kind)
{
    if (std::ranges::find(kVariableKinds, kind) == kVariableKinds.end())
    {
        refuse(index, "kind " + text_of(static_cast<int>(kind)) + " is unknown");
    }
}

void check_values(Index index, const Variable &variable)
{
    const std::string_view fault = bound_fault(variable.lower, variable.upper);
    if (!fault.empty())
    {
        refuse(index, std::string(fault));
    }
}

void check_range(Index index, const Variable &variable)
{
    const Variable range = widest(variable.kind);
    if (range.lower <= variable.lower && variable.upper <= range.upper)
    {
        return;
    }
    const std::string bounds = interval(variable.lower, variable.upper);
    const std::string allowed = interval(range.lower, range.upper);
    refuse(index, "bounds " + bounds + " are outside " + allowed + ", the widest of its kind");
}

void check(Index index, const Variable &variable)
{
    check_kind(index, variable.kind);
    check_values(index, variable);
    check_range(index, variable);
}

}

Variable widest(VariableKind kind) noexcept
{
    switch (kind)
    {
    case VariableKind::Binary:
        return Variable{.kind = kind, .lower = 0.0, .upper = 1.0};
    case VariableKind::Continuous:
    case VariableKind::Integer:
    case VariableKind::SemiContinuous:
    case VariableKind::SemiInteger:
        break;
    }
    return Variable{.kind = kind, .lower = -kInfinity, .upper = kInfinity};
}

Index Model::variable_count() const noexcept
{
    return static_cast<Index>(variables_.size());
}

const Variable &Model::variable(Index index) const
{
    check_index(kNoun, index, variable_count());
    return variables_[static_cast<std::size_t>(index)];
}

std::span<const Variable> Model::variables(Index first, Index count) const
{
    check_span(kNoun, first, count, variable_count());
    const auto start = static_cast<std::size_t>(first);
    return std::span(variables_).subspan(start, static_cast<std::size_t>(count));
}

void Model::add_variables(std::span<const Variable> added)
{
    if (added.size() > kRoom - variables_.size())
    {
        throw Error(ErrorKind::NoMemory, "model", "no variable index left");
    }
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(static_cast<Index>(variables_.size() + k), added[k]);
    }
    variables_.insert(variables_.end(), added.begin(), added.end());
}

void Model::set_variables(std::span<const Index> indices, std::span<const Variable> changed)
{
    if (indices.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and variables differ in number");
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(kNoun, indices[k], variable_count());
        check(indices[k], changed[k]);
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        variables_[static_cast<std::size_t>(indices[k])] = changed[k];
    }
}

}
