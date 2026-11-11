#pragma once

#include "core/index.hpp"
#include "model/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

void check_starts(std::span<const Offset> starts);
void check_shape(std::size_t functions, std::span<const Offset> starts, std::size_t terms);

template <typename T>
std::span<T> slice(std::span<T> terms, std::span<const Offset> starts, std::size_t k)
{
    const auto begin = static_cast<std::size_t>(starts[k]);
    const auto end = static_cast<std::size_t>(starts[k + 1]);
    return terms.subspan(begin, end - begin);
}

template <typename T> void reserve_more(std::vector<T> &items, std::size_t extra)
{
    const std::size_t wanted = items.size() + extra;
    if (wanted > items.capacity())
    {
        const std::size_t doubled = (std::max)(wanted, 2 * items.capacity());
        items.reserve((std::min)(doubled, items.size() + room(items)));
    }
}

}
