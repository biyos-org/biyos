#include "model/model.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "core/text/text.hpp"
#include "model/utils/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{
constexpr std::string_view kNoun = "variable";

void check_kind(Index index, VariableKind kind)
{
    if (std::ranges::find(kVariableKinds, kind) == kVariableKinds.end())
    {
        refuse(kNoun, index, "kind " + text_of(static_cast<int>(kind)) + " is unknown");
    }
}

void check_values(Index index, const Variable &variable)
{
    const std::string_view fault = bound_fault(variable.lower, variable.upper);
    if (!fault.empty())
    {
        refuse(kNoun, index, std::string(fault));
    }
}

void check_range(Index index, const Variable &variable)
{
    const Variable range = widest(variable.kind);
    if (range.lower <= variable.lower && variable.upper <= range.upper)
    {
        return;
    }
    const std::string bounds = interval_of(variable.lower, variable.upper);
    const std::string allowed = interval_of(range.lower, range.upper);
    const std::string reason = "bounds " + bounds + " are outside " + allowed;
    refuse(kNoun, index, reason + ", the widest of its kind");
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

Index Model::n_variables() const noexcept
{
    return to_index(variables_.size());
}

const Variable &Model::variable(Index index) const
{
    check_index(kNoun, index, n_variables());
    return variables_[to_size(index)];
}

std::span<const Variable> Model::variables(Index start, Index stop) const
{
    check_span(kNoun, start, stop, n_variables());
    const auto offset = to_size(start);
    return std::span(variables_).subspan(offset, to_size(stop - start));
}

void Model::add_variables(std::span<const Variable> added)
{
    check_n(kNoun, variables_, added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(to_index(variables_.size() + k), added[k]);
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
        check_index(kNoun, indices[k], n_variables());
        check(indices[k], changed[k]);
    }
    check_kinds(indices, changed);
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        variables_[to_size(indices[k])] = changed[k];
    }
}

}
