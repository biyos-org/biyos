#pragma once

#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

struct Tangents
{
    std::span<const double> slots;
    std::span<double> spare;
};

std::size_t tangent_rows(const Tape &tape) noexcept;
double scaled(double tangent, double factor) noexcept;
void partial_tangents(const Instruction &instruction,
                      const Frame &frame,
                      const Tangents &tangents,
                      std::span<const double> partials,
                      std::span<double> out) noexcept;

}
