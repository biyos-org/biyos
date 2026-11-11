#pragma once

#include "core/index.hpp"
#include "expr/frame.hpp"
#include "expr/tape.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace biyos
{

std::uint32_t partial_count(const Instruction &instruction) noexcept;
void partials_of(const Instruction &instruction,
                 const Frame &frame,
                 double result,
                 std::span<double> partials) noexcept;
std::size_t chosen(std::span<const double> partials) noexcept;

template <typename T> std::span<T> region(const Block &block, std::span<T> partials) noexcept
{
    return partials.subspan(static_cast<std::size_t>(block.origin), block.places.back());
}

}
