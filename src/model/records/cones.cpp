#include "model/model.hpp"

#include "core/error.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
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

constexpr std::string_view kNoun = "cone";

struct GivenCone
{
    ConeKind kind;
    std::span<const Index> functions;
    std::span<const Real> parameters;
};

bool parametric(ConeKind kind) noexcept
{
    switch (kind)
    {
    case ConeKind::Nonnegative:
    case ConeKind::SecondOrder:
    case ConeKind::Rotated:
    case ConeKind::Exponential:
    case ConeKind::DualExponential:
    case ConeKind::Semidefinite:
        return false;
    case ConeKind::Power:
    case ConeKind::DualPower:
        return true;
    }
    return false;
}

bool triangular(std::size_t dimension) noexcept
{
    std::size_t total = 0;
    for (std::size_t order = 1; total < dimension; ++order)
    {
        total += order;
    }
    return total == dimension;
}

std::string dimension_of(std::size_t dimension)
{
    return "dimension " + text_of(dimension);
}

std::string below(std::size_t dimension, const std::string &least)
{
    return dimension_of(dimension) + " is below " + least;
}

std::string dimension_fault(ConeKind kind, std::size_t dimension, std::size_t parameters)
{
    switch (kind)
    {
    case ConeKind::Nonnegative:
    case ConeKind::SecondOrder:
        return "";
    case ConeKind::Rotated:
        return dimension >= 2 ? "" : below(dimension, "2");
    case ConeKind::Exponential:
    case ConeKind::DualExponential:
        return dimension == 3 ? "" : dimension_of(dimension) + " is not 3";
    case ConeKind::Power:
    case ConeKind::DualPower:
        return dimension >= parameters
                   ? ""
                   : below(dimension, "its " + text_of(parameters) + " parameters");
    case ConeKind::Semidefinite:
        return triangular(dimension) ? "" : dimension_of(dimension) + " is not n(n+1)/2";
    }
    return "";
}

void check_kind(Index cone, ConeKind kind)
{
    if (std::ranges::find(kConeKinds, kind) == kConeKinds.end())
    {
        refuse(kNoun, cone, "kind " + text_of(static_cast<int>(kind)) + " is unknown");
    }
}

void check_members(Index cone,
                   std::span<const Index> members,
                   Index functions,
                   std::vector<Index> &sorted)
{
    if (members.empty())
    {
        refuse(kNoun, cone, "has no member");
    }
    for (const Index function : members)
    {
        check_function(kNoun, cone, function, functions);
    }
    sorted.assign(members.begin(), members.end());
    std::ranges::sort(sorted);
    const auto repeat = std::ranges::adjacent_find(sorted);
    if (repeat != sorted.end())
    {
        refuse(kNoun, cone, item("function", *repeat) + " is repeated");
    }
}

void check_parameter(Index cone, std::size_t k, Real value)
{
    const std::string name = "parameter " + text_of(k);
    const std::string_view fault = value_fault(value);
    if (!fault.empty())
    {
        refuse(kNoun, cone, name + " " + std::string(fault));
    }
    if (std::signbit(value) || std::fpclassify(value) == FP_ZERO)
    {
        refuse(kNoun, cone, name + " is " + text_of(value) + ", not positive");
    }
}

void check_parameters(Index cone, ConeKind kind, std::span<const Real> parameters)
{
    if (!parametric(kind) && !parameters.empty())
    {
        refuse(kNoun, cone, "takes no parameter, given " + text_of(parameters.size()));
    }
    if (parametric(kind) && parameters.empty())
    {
        refuse(kNoun, cone, "takes at least one parameter");
    }
    for (std::size_t k = 0; k < parameters.size(); ++k)
    {
        check_parameter(cone, k, parameters[k]);
    }
}

void check(Index index, const GivenCone &cone, Index functions, std::vector<Index> &sorted)
{
    check_kind(index, cone.kind);
    check_members(index, cone.functions, functions, sorted);
    check_parameters(index, cone.kind, cone.parameters);
    const std::string fault =
        dimension_fault(cone.kind, cone.functions.size(), cone.parameters.size());
    if (!fault.empty())
    {
        refuse(kNoun, index, fault);
    }
}

template <typename T>
void check_more(const std::vector<T> &items, Offset added, std::string_view part)
{
    if (added > to_offset(room(items)))
    {
        throw Error(ErrorKind::NoMemory, "model", "no cone " + std::string(part) + " offset left");
    }
}

Offset begin_at(const std::vector<Offset> &ends, Index cone) noexcept
{
    return cone == 0 ? 0 : ends[to_size(cone - 1)];
}

template <typename T>
std::span<const T> part(const std::vector<T> &items,
                        const std::vector<Offset> &ends,
                        Index start,
                        Index stop)
{
    const auto begin = to_size(begin_at(ends, start));
    const auto end = to_size(begin_at(ends, stop));
    return std::span(items).subspan(begin, end - begin);
}

template <typename T>
void extend(std::vector<Offset> &ends,
            std::vector<T> &items,
            std::span<const Offset> starts,
            std::span<const T> added)
{
    const auto base = to_offset(items.size());
    for (const Offset end : starts.subspan(1))
    {
        ends.push_back(base + end);
    }
    items.insert(items.end(), added.begin(), added.end());
}

}

Index Model::n_cones() const noexcept
{
    return to_index(cone_kinds_.size());
}

std::span<const ConeKind> Model::cone_kinds(Index start, Index stop) const
{
    check_span(kNoun, start, stop, n_cones());
    const auto offset = to_size(start);
    return std::span(cone_kinds_).subspan(offset, to_size(stop - start));
}

std::span<const Index> Model::cone_functions(Index start, Index stop) const
{
    check_span(kNoun, start, stop, n_cones());
    return part(cone_functions_, cone_ends_, start, stop);
}

std::span<const Real> Model::cone_parameters(Index start, Index stop) const
{
    check_span(kNoun, start, stop, n_cones());
    return part(cone_parameters_, cone_parameter_ends_, start, stop);
}

void Model::check_cone_room(std::size_t cones, Offset members, Offset parameters) const
{
    check_n(kNoun, cone_kinds_, cones);
    check_more(cone_functions_, members, "member");
    check_more(cone_parameters_, parameters, "parameter");
}

void Model::add_cones(std::span<const ConeKind> kinds,
                      std::span<const Offset> starts,
                      std::span<const Index> functions,
                      std::span<const Offset> parameter_starts,
                      std::span<const Real> parameters)
{
    check_cone_room(kinds.size(), to_offset(functions.size()), to_offset(parameters.size()));
    check_shape(kinds.size(), starts, functions.size());
    check_shape(kinds.size(), parameter_starts, parameters.size(), "prmoff");
    std::vector<Index> sorted;
    for (std::size_t k = 0; k < kinds.size(); ++k)
    {
        const GivenCone cone{
            .kind = kinds[k],
            .functions = slice(functions, starts, k),
            .parameters = slice(parameters, parameter_starts, k),
        };
        check(to_index(cone_kinds_.size() + k), cone, n_functions(), sorted);
    }
    reserve_more(cone_kinds_, kinds.size());
    reserve_more(cone_ends_, kinds.size());
    reserve_more(cone_functions_, functions.size());
    reserve_more(cone_parameter_ends_, kinds.size());
    reserve_more(cone_parameters_, parameters.size());
    cone_kinds_.insert(cone_kinds_.end(), kinds.begin(), kinds.end());
    extend(cone_ends_, cone_functions_, starts, functions);
    extend(cone_parameter_ends_, cone_parameters_, parameter_starts, parameters);
}

std::vector<Index> Model::delete_cones(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, n_cones(), indices);
    keep(cone_kinds_, map);
    keep_parts(cone_ends_, cone_functions_, map);
    keep_parts(cone_parameter_ends_, cone_parameters_, map);
    renumber_tolerances(Family::Cone, map);
    renumber_names(Family::Cone, map);
    return map;
}

}
