#include "model/attributes/overrides.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/checks.hpp"
#include "model/renumber.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace biyos
{

namespace
{

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr Override kGlobal{.absolute = kNan, .relative = kNan};

void check_size(std::span<const Index> indices, std::span<const double> values)
{
    if (!values.empty() && values.size() != indices.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and tolerances differ in number");
    }
}

void check_value(std::string_view noun, Index index, std::string_view part, double value)
{
    const std::string field = std::string(part) + " tolerance ";
    const std::string_view fault = value_fault(value);
    if (!fault.empty())
    {
        refuse(noun, index, field + std::string(fault));
    }
    if (std::signbit(value) && std::fpclassify(value) != FP_ZERO)
    {
        refuse(noun, index, field + text_of(value) + " is negative");
    }
}

void check_entry(std::string_view noun,
                 Index index,
                 std::span<const double> values,
                 std::size_t k,
                 std::string_view part)
{
    if (!values.empty())
    {
        check_value(noun, index, part, values[k]);
    }
}

void add_missing(std::map<Index, Override> &held, std::span<const Index> indices)
{
    std::map<Index, Override> added;
    for (const Index index : indices)
    {
        if (!held.contains(index))
        {
            added.try_emplace(index, kGlobal);
        }
    }
    held.merge(added);
}

void assign(double &held, std::span<const double> values, std::size_t k) noexcept
{
    if (!values.empty())
    {
        held = values[k];
    }
}

}

Override Overrides::at(Index index) const noexcept
{
    const auto found = held_.find(index);
    return found == held_.end() ? kGlobal : found->second;
}

void Overrides::set(std::string_view noun,
                    Index size,
                    std::span<const Index> indices,
                    std::span<const double> absolute,
                    std::span<const double> relative)
{
    check_size(indices, absolute);
    check_size(indices, relative);
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(noun, indices[k], size);
        check_entry(noun, indices[k], absolute, k, "absolute");
        check_entry(noun, indices[k], relative, k, "relative");
    }
    if (absolute.empty() && relative.empty())
    {
        return;
    }
    add_missing(held_, indices);
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        Override &held = held_.find(indices[k])->second;
        assign(held.absolute, absolute, k);
        assign(held.relative, relative, k);
    }
}

void Overrides::remove(std::string_view noun, Index size, std::span<const Index> indices)
{
    for (const Index index : indices)
    {
        check_index(noun, index, size);
    }
    for (const Index index : indices)
    {
        held_.erase(index);
    }
}

void Overrides::renumber(std::span<const Index> map) noexcept
{
    auto at = held_.begin();
    while (at != held_.end())
    {
        const Index to = map[to_size(at->first)];
        auto node = held_.extract(at++);
        if (to != kDeleted)
        {
            node.key() = to;
            held_.insert(std::move(node));
        }
    }
}

}
