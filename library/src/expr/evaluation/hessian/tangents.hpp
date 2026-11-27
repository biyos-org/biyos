#pragma once

#include "core/numbers.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

struct Tangents
{
    std::span<const Real> slots;
    std::span<Real> spare;
};

std::size_t tangent_rows(const Tape &tape) noexcept;
Real scaled(Real tangent, Real factor) noexcept;
void partial_tangents(const Instruction &instruction,
                      const Frame &frame,
                      const Tangents &tangents,
                      std::span<const Real> partials,
                      std::span<Real> out) noexcept;

}
