#pragma once

#include "expr/tape.hpp"

#include <cstdint>

namespace biyos
{

std::uint32_t partial_count(const Instruction &instruction) noexcept;

}
