#pragma once

#include "core/index.hpp"
#include "core/memory/room.hpp"
#include "model/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

void check_starts(std::span<const Offset> starts, std::string_view argument = "off");
void check_shape(std::size_t functions,
                 std::span<const Offset> starts,
                 std::size_t terms,
                 std::string_view argument = "off");

template <typename T>
std::span<T> slice(std::span<T> terms, std::span<const Offset> starts, std::size_t k)
{
    const auto begin = static_cast<std::size_t>(starts[k]);
    const auto end = static_cast<std::size_t>(starts[k + 1]);
    return terms.subspan(begin, end - begin);
}

template <typename T>
std::size_t keep_part(std::span<T> items,
                      std::size_t start,
                      std::size_t end,
                      std::size_t to) noexcept
{
    if (to != start)
    {
        std::ranges::copy(items.subspan(start, end - start), items.subspan(to).begin());
    }
    return to + (end - start);
}

template <typename T>
void clear_parts(std::vector<Offset> &ends,
                 std::vector<T> &items,
                 std::span<const Index> functions) noexcept
{
    if (functions.empty())
    {
        return;
    }
    const auto start = static_cast<std::size_t>(functions.front());
    std::size_t read = start == 0 ? 0 : static_cast<std::size_t>(ends[start - 1]);
    std::size_t write = read;
    std::size_t next = 0;
    for (std::size_t function = start; function < ends.size(); ++function)
    {
        const auto end = static_cast<std::size_t>(ends[function]);
        if (next < functions.size() && static_cast<std::size_t>(functions[next]) == function)
        {
            ++next;
        }
        else
        {
            write = keep_part(std::span(items), read, end, write);
        }
        read = end;
        ends[function] = static_cast<Offset>(write);
    }
    items.resize(write);
}

}
