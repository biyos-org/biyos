#include "model/model.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/utils/checks.hpp"
#include "model/utils/renumber.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "indicator";

void check_variable(Index index, Index variable, const Model &model)
{
    if (variable < 0 || variable >= model.n_variables())
    {
        refuse(kNoun, index, item("variable", variable) + " is unknown");
    }
    if (model.variable(variable).kind != VariableKind::Binary)
    {
        refuse(kNoun, index, item("variable", variable) + " is not binary");
    }
}

void check_value(Index index, int value)
{
    if (value != 0 && value != 1)
    {
        refuse(kNoun, index, "value " + text_of(value) + " is neither 0 nor 1");
    }
}

void check_bounds(Index index, const Indicator &indicator)
{
    const std::string_view fault = bound_fault(indicator.lower, indicator.upper);
    if (!fault.empty())
    {
        refuse(kNoun, index, std::string(fault));
    }
}

void check(Index index, const Indicator &indicator, const Model &model)
{
    check_variable(index, indicator.variable, model);
    check_value(index, indicator.value);
    check_function(kNoun, index, indicator.function, model.n_functions());
    check_bounds(index, indicator);
}

std::vector<Index> switches(std::span<const Indicator> indicators)
{
    std::vector<Index> found;
    found.reserve(indicators.size());
    for (const Indicator &indicator : indicators)
    {
        found.push_back(indicator.variable);
    }
    std::ranges::sort(found);
    return found;
}

bool demotes(const Model &model, std::span<const Index> indices, std::span<const Variable> changed)
{
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        const bool was = model.variable(indices[k]).kind == VariableKind::Binary;
        if (was && changed[k].kind != VariableKind::Binary)
        {
            return true;
        }
    }
    return false;
}

[[noreturn]] void refuse_kind(std::span<const Indicator> indicators, Index variable)
{
    const auto user = std::ranges::find(indicators, variable, &Indicator::variable);
    const auto index = static_cast<Index>(user - indicators.begin());
    const std::string reason = item(kNoun, index) + " needs it binary";
    throw Error(ErrorKind::InUse, item("variable", variable), reason);
}

}

Index Model::n_indicators() const noexcept
{
    return to_index(indicators_.size());
}

const Indicator &Model::indicator(Index index) const
{
    check_index(kNoun, index, n_indicators());
    return indicators_[to_size(index)];
}

std::span<const Indicator> Model::indicators(Index start, Index stop) const
{
    check_span(kNoun, start, stop, n_indicators());
    const auto offset = to_size(start);
    return std::span(indicators_).subspan(offset, to_size(stop - start));
}

void Model::add_indicators(std::span<const Indicator> added)
{
    check_n(kNoun, indicators_, added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(to_index(indicators_.size() + k), added[k], *this);
    }
    indicators_.insert(indicators_.end(), added.begin(), added.end());
}

void Model::set_indicators(std::span<const Index> indices, std::span<const Indicator> changed)
{
    if (indices.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and indicators differ in number");
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(kNoun, indices[k], n_indicators());
        check(indices[k], changed[k], *this);
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        indicators_[to_size(indices[k])] = changed[k];
    }
}

void Model::check_kinds(std::span<const Index> indices, std::span<const Variable> changed) const
{
    if (!demotes(*this, indices, changed))
    {
        return;
    }
    const std::vector<Index> used = switches(indicators_);
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        const bool binary = changed[k].kind == VariableKind::Binary;
        if (!binary && std::ranges::binary_search(used, indices[k]))
        {
            refuse_kind(indicators_, indices[k]);
        }
    }
}

std::vector<Index> Model::delete_indicators(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, n_indicators(), indices);
    keep(indicators_, map);
    renumber_names(Family::Indicator, map);
    return map;
}

}
