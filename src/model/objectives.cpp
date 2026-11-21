#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "model/checks.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "objective";

bool zero(double value) noexcept
{
    return std::fpclassify(value) == FP_ZERO;
}

void check_sense(Index idx, Sense sense)
{
    if (std::ranges::find(kSenses, sense) == kSenses.end())
    {
        refuse(kNoun, idx, "sense " + text_of(static_cast<int>(sense)) + " is unknown");
    }
}

void check_weight(Index idx, double weight)
{
    const std::string_view fault = value_fault(weight);
    if (!fault.empty())
    {
        refuse(kNoun, idx, "weight " + std::string(fault));
    }
    if (std::signbit(weight) || zero(weight))
    {
        refuse(kNoun, idx, "weight " + text_of(weight) + " is not positive");
    }
}

void check_tolerance(Index idx, std::string_view name, double tolerance)
{
    const bool unlimited = std::isinf(tolerance) && !std::signbit(tolerance);
    const std::string_view fault = unlimited ? std::string_view() : value_fault(tolerance);
    if (!fault.empty())
    {
        refuse(kNoun, idx, std::string(name) + " tolerance " + std::string(fault));
    }
    if (std::signbit(tolerance) && !zero(tolerance))
    {
        const std::string value = text_of(tolerance);
        refuse(kNoun, idx, std::string(name) + " tolerance " + value + " is negative");
    }
}

void check(Index idx, const Objective &obj, Index funs)
{
    check_fun(kNoun, idx, obj.function, funs);
    check_sense(idx, obj.sense);
    check_weight(idx, obj.weight);
    check_tolerance(idx, "absolute", obj.absolute);
    check_tolerance(idx, "relative", obj.relative);
}

}

Objective default_obj(Index fun, Index idx) noexcept
{
    return Objective{
        .function = fun,
        .sense = Sense::Minimize,
        .priority = idx,
        .weight = 1.0,
        .absolute = 0.0,
        .relative = 0.0,
    };
}

Index Model::n_objs() const noexcept
{
    return static_cast<Index>(objectives_.size());
}

const Objective &Model::objective(Index idx) const
{
    check_idx(kNoun, idx, n_objs());
    return objectives_[static_cast<std::size_t>(idx)];
}

std::span<const Objective> Model::objs(Index beg, Index n) const
{
    check_span(kNoun, beg, n, n_objs());
    const auto start = static_cast<std::size_t>(beg);
    return std::span(objectives_).subspan(start, static_cast<std::size_t>(n));
}

void Model::check_obj_room(std::size_t added) const
{
    check_n(kNoun, objectives_, added);
}

void Model::add_objs(std::span<const Objective> added)
{
    check_obj_room(added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(static_cast<Index>(objectives_.size() + k), added[k], n_funs());
    }
    objectives_.insert(objectives_.end(), added.begin(), added.end());
}

void Model::set_objs(std::span<const Index> idx, std::span<const Objective> changed)
{
    if (idx.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and objectives differ in number");
    }
    for (std::size_t k = 0; k < idx.size(); ++k)
    {
        check_idx(kNoun, idx[k], n_objs());
        check(idx[k], changed[k], n_funs());
    }
    for (std::size_t k = 0; k < idx.size(); ++k)
    {
        objectives_[static_cast<std::size_t>(idx[k])] = changed[k];
    }
}

}
