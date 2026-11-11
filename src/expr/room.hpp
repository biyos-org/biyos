#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace biyos
{

template <typename T> void make_room(std::vector<T> &values, std::size_t needed)
{
    if (needed > values.capacity())
    {
        values.reserve((std::max)(needed, 2 * values.capacity()));
    }
}

}
