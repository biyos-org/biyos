#pragma once

#include "core/index.hpp"

#include <cstddef>
#include <span>

namespace biyos::capi
{

std::size_t length(int count);
std::span<const Index> listed(int count, const int *indices);

}
