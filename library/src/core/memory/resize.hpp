#pragma once

#include <cstddef>
#include <vector>

namespace biyos
{

template <typename T, typename Make>
void resize(std::vector<T> &items, std::size_t count, Make make)
{
    if (count < items.size())
    {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(count), items.end());
    }
    while (items.size() < count)
    {
        items.push_back(make());
    }
}

}
