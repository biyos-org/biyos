#include "io/directory.hpp"

#include "core/numbers.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace biyos
{

std::size_t Directory::Hash::operator()(std::string_view text) const noexcept
{
    return std::hash<std::string_view>{}(text);
}

std::optional<Index> Directory::find(std::string_view name) const
{
    const auto found = indices_.find(name);
    if (found == indices_.end())
    {
        return std::nullopt;
    }
    return found->second;
}

bool Directory::add(std::string_view name)
{
    const auto [entry, added] = indices_.try_emplace(std::string(name), size());
    if (added)
    {
        names_.push_back(entry->first);
    }
    return added;
}

Index Directory::size() const noexcept
{
    return static_cast<Index>(names_.size());
}

std::span<const std::string_view> Directory::names() const noexcept
{
    return names_;
}

}
