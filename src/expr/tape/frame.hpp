#pragma once

#include "expr/tape/tape.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace biyos
{

struct Frame
{
    std::span<const double> point;
    std::span<const std::uint32_t> operands;
    std::span<const double> weights;
    std::span<double> slots;
    std::span<double> result;
    std::span<double> spare;
    std::size_t lanes;
};

inline std::size_t place(std::uint64_t payload) noexcept
{
    return static_cast<std::size_t>(payload);
}

inline double constant_of(const Instruction &instruction) noexcept
{
    return std::bit_cast<double>(instruction.payload);
}

template <typename T>
std::span<T> row(std::span<T> items, std::size_t index, std::size_t lanes) noexcept
{
    return items.subspan(index * lanes, lanes);
}

inline std::span<const double> row(const Frame &frame, std::uint32_t slot) noexcept
{
    return row(std::span<const double>(frame.slots), slot, frame.lanes);
}

inline std::span<const std::uint32_t> listed(const Instruction &instruction,
                                             std::span<const std::uint32_t> operands) noexcept
{
    return operands.subspan(instruction.first, instruction.second - instruction.first);
}

}
