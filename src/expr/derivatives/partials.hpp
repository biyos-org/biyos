#pragma once

#include "core/numbers.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace biyos
{

std::uint32_t n_partials(const Instruction &instruction) noexcept;
void partials_of(const Instruction &instruction,
                 const Frame &frame,
                 std::span<double> partials) noexcept;
std::size_t chosen(double partial) noexcept;
void mark_undefined(const Frame &frame, std::span<double> partials) noexcept;

template <typename T>
std::span<T> region(const Block &block, std::span<T> partials, std::size_t lanes) noexcept
{
    const auto origin = to_size(block.origin);
    return partials.subspan(origin * lanes, block.places.back() * lanes);
}

}
