#pragma once

#include "core/error.hpp"
#include "core/index.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

std::string item(std::string_view noun, Index index);
[[noreturn]] void refuse(std::string_view noun, Index index, const std::string &reason);
std::string_view bound_fault(double lower, double upper) noexcept;
void check_index(std::string_view noun, Index index, Index size);
void check_span(std::string_view noun, Index first, Index count, Index size);

inline constexpr auto kRoom = static_cast<std::size_t>((std::numeric_limits<Index>::max)());

template <typename T> std::size_t room(const std::vector<T> &items) noexcept
{
    const auto most = static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());
    return (std::min)(most / sizeof(T), items.max_size()) - items.size();
}

template <typename T>
void check_count(std::string_view noun, const std::vector<T> &items, std::size_t added)
{
    if (added > (std::min)(kRoom - items.size(), room(items)))
    {
        const std::string reason = "no " + std::string(noun) + " index left";
        throw Error(ErrorKind::NoMemory, "model", reason);
    }
}

}
