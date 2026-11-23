#pragma once

#include "core/error.hpp"
#include "core/index.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

std::string item(std::string_view noun, Index index);
[[noreturn]] void refuse(std::string_view noun, Index index, const std::string &reason);
std::string_view bound_fault(double lower, double upper) noexcept;
std::string_view value_fault(double value) noexcept;
void check_index(std::string_view noun, Index index, Index total);
void check_span(std::string_view noun, Index start, Index size, Index total);
void check_function(std::string_view noun, Index index, Index function, Index functions);
void check_functions(std::span<const Index> functions, Index held, std::string_view argument);

inline constexpr auto kRoom = static_cast<std::size_t>((std::numeric_limits<Index>::max)());

template <typename T> std::size_t room(const std::vector<T> &items) noexcept
{
    const auto most = static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());
    return (std::min)(most / sizeof(T), items.max_size()) - items.size();
}

template <typename T>
void check_n(std::string_view noun, const std::vector<T> &items, std::size_t added)
{
    if (added > (std::min)(kRoom - items.size(), room(items)))
    {
        const std::string reason = "no " + std::string(noun) + " index left";
        throw Error(ErrorKind::NoMemory, "model", reason);
    }
}

}
