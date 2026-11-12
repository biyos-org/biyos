#pragma once

#include "expr/tape.hpp"

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
};

inline std::size_t place(std::uint64_t payload) noexcept
{
    return static_cast<std::size_t>(payload);
}

inline double constant_of(const Instruction &instruction) noexcept
{
    return std::bit_cast<double>(instruction.payload);
}

inline double left(const Instruction &instruction, const Frame &frame) noexcept
{
    return frame.slots[instruction.first];
}

inline double right(const Instruction &instruction, const Frame &frame) noexcept
{
    return frame.slots[instruction.second];
}

inline std::span<const std::uint32_t> listed(const Instruction &instruction,
                                             std::span<const std::uint32_t> operands) noexcept
{
    return operands.subspan(instruction.first, instruction.second - instruction.first);
}

}
