#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace biyos
{

template <typename T> std::size_t room(const std::vector<T> &items) noexcept
{
    const auto most = static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());
    return (std::min)(most / sizeof(T), items.max_size()) - items.size();
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
