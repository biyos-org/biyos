#pragma once

#include "core/numbers.hpp"
#include "model/compressed.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

inline constexpr Index kDeleted = -1;

std::vector<Index> renumbering(std::string_view noun, Index total, std::span<const Index> deleted);

template <typename T> void keep(std::vector<T> &items, std::span<const Index> map) noexcept
{
    std::size_t kept = 0;
    for (std::size_t k = 0; k < map.size(); ++k)
    {
        if (map[k] != kDeleted)
        {
            items[kept] = items[k];
            ++kept;
        }
    }
    items.resize(kept);
}

template <typename T>
void keep_parts(std::vector<Offset> &ends,
                std::vector<T> &items,
                std::span<const Index> map) noexcept
{
    std::size_t read = 0;
    std::size_t write = 0;
    std::size_t kept = 0;
    for (std::size_t k = 0; k < map.size(); ++k)
    {
        const auto end = static_cast<std::size_t>(ends[k]);
        if (map[k] != kDeleted)
        {
            write = keep_part(std::span(items), read, end, write);
            ends[kept] = static_cast<Offset>(write);
            ++kept;
        }
        read = end;
    }
    ends.resize(kept);
    items.resize(write);
}

}
