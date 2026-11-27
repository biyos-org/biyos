#pragma once

#include "core/memory/room.hpp"
#include "core/numbers.hpp"
#include "model/utils/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

void check_shape(std::size_t functions,
                 std::span<const Offset> starts,
                 std::size_t terms,
                 std::string_view argument = "off");

template <typename T>
std::size_t keep_part(std::span<T> items,
                      std::size_t start,
                      std::size_t stop,
                      std::size_t to) noexcept
{
    if (to != start)
    {
        std::ranges::copy(items.subspan(start, stop - start), items.subspan(to).begin());
    }
    return to + (stop - start);
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
    const auto start = to_size(functions.front());
    std::size_t read = start == 0 ? 0 : to_size(ends[start - 1]);
    std::size_t write = read;
    std::size_t next = 0;
    for (std::size_t function = start; function < ends.size(); ++function)
    {
        const auto end = to_size(ends[function]);
        if (next < functions.size() && to_size(functions[next]) == function)
        {
            ++next;
        }
        else
        {
            write = keep_part(std::span(items), read, end, write);
        }
        read = end;
        ends[function] = to_offset(write);
    }
    items.resize(write);
}

}
