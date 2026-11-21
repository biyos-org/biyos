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
constexpr std::string_view kNoun = "variable";

std::string interval(double lower, double upper)
{
    return "[" + text_of(lower) + ", " + text_of(upper) + "]";
}

void check_kind(Index idx, VariableKind knd)
{
    if (std::ranges::find(kVariableKinds, knd) == kVariableKinds.end())
    {
        refuse(kNoun, idx, "kind " + text_of(static_cast<int>(knd)) + " is unknown");
    }
}

void check_values(Index idx, const Variable &var)
{
    const std::string_view fault = bound_fault(var.lower, var.upper);
    if (!fault.empty())
    {
        refuse(kNoun, idx, std::string(fault));
    }
}

void check_range(Index idx, const Variable &var)
{
    const Variable range = widest(var.kind);
    if (range.lower <= var.lower && var.upper <= range.upper)
    {
        return;
    }
    const std::string bnds = interval(var.lower, var.upper);
    const std::string allowed = interval(range.lower, range.upper);
    const std::string reason = "bounds " + bnds + " are outside " + allowed;
    refuse(kNoun, idx, reason + ", the widest of its kind");
}

void check(Index idx, const Variable &var)
{
    check_kind(idx, var.kind);
    check_values(idx, var);
    check_range(idx, var);
}

}

Variable widest(VariableKind knd) noexcept
{
    switch (knd)
    {
    case VariableKind::Binary:
        return Variable{.kind = knd, .lower = 0.0, .upper = 1.0};
    case VariableKind::Continuous:
    case VariableKind::Integer:
    case VariableKind::SemiContinuous:
    case VariableKind::SemiInteger:
        break;
    }
    return Variable{.kind = knd, .lower = -kInfinity, .upper = kInfinity};
}

Index Model::n_vars() const noexcept
{
    return static_cast<Index>(variables_.size());
}

const Variable &Model::variable(Index idx) const
{
    check_idx(kNoun, idx, n_vars());
    return variables_[static_cast<std::size_t>(idx)];
}

std::span<const Variable> Model::vars(Index beg, Index n) const
{
    check_span(kNoun, beg, n, n_vars());
    const auto start = static_cast<std::size_t>(beg);
    return std::span(variables_).subspan(start, static_cast<std::size_t>(n));
}

void Model::add_vars(std::span<const Variable> added)
{
    check_n(kNoun, variables_, added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(static_cast<Index>(variables_.size() + k), added[k]);
    }
    variables_.insert(variables_.end(), added.begin(), added.end());
}

void Model::set_vars(std::span<const Index> idx, std::span<const Variable> changed)
{
    if (idx.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and variables differ in number");
    }
    for (std::size_t k = 0; k < idx.size(); ++k)
    {
        check_idx(kNoun, idx[k], n_vars());
        check(idx[k], changed[k]);
    }
    for (std::size_t k = 0; k < idx.size(); ++k)
    {
        variables_[static_cast<std::size_t>(idx[k])] = changed[k];
    }
}

}
