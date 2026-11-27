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
    std::span<Real> slots;
    std::span<Real> running;
    std::span<Real> prefix;
    std::size_t lanes;
};

std::size_t widest_product(const Tape &tape) noexcept;
Real scaled(Real tangent, Real factor) noexcept;
bool curved(Real exponent) noexcept;
void partial_tangents(const Instruction &instruction,
                      const Frame &frame,
                      const Tangents &tangents,
                      std::span<const Real> partials,
                      std::span<Real> out) noexcept;

}
