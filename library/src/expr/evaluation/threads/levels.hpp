#pragma once

#include "core/numbers.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Levels
{
    std::vector<std::size_t> blocks;
    std::vector<Offset> starts;
};

Levels levels_of(const Tape &tape);
std::span<const std::size_t> level(const Levels &levels, std::size_t k) noexcept;

}
