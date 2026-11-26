#pragma once

#include "core/numbers.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

template <typename T>
std::span<T> slice(std::span<T> items, std::span<const Offset> starts, std::size_t k) noexcept
{
    const auto begin = to_size(starts[k]);
    const auto end = to_size(starts[k + 1]);
    return items.subspan(begin, end - begin);
}

}
