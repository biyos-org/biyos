#pragma once

#include "core/numbers.hpp"
#include "expr/intervals/interval.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace biyos
{

enum class Curvature : std::uint8_t
{
    Unknown,
    Convex,
    Concave,
    Affine,
};

enum class Monotonicity : std::uint8_t
{
    Unknown,
    Nondecreasing,
    Nonincreasing,
    Constant,
};

struct Argument
{
    Monotonicity monotonicity = Monotonicity::Unknown;
    bool cut_below = false;
    bool cut_above = false;
};

struct Profile
{
    Curvature curvature = Curvature::Unknown;
    Argument argument;
};

struct Operands
{
    std::span<const Offset> children;
    std::span<const Interval> nodes;

    std::size_t size() const noexcept { return children.size(); }

    Interval operator[](std::size_t position) const noexcept
    {
        return nodes[static_cast<std::size_t>(children[position])];
    }
};

bool is_convex(Curvature curvature) noexcept;
bool is_concave(Curvature curvature) noexcept;
Curvature curvature_of(bool convex, bool concave) noexcept;
Curvature common(Curvature left, Curvature right) noexcept;
bool rises(Monotonicity monotonicity) noexcept;
bool falls(Monotonicity monotonicity) noexcept;
Monotonicity monotonicity_of(bool rising, bool falling) noexcept;
Monotonicity common(Monotonicity left, Monotonicity right) noexcept;
Monotonicity monotonicity_of(Interval slope) noexcept;
bool is_point(Interval value) noexcept;
bool any_empty(const Operands &operands) noexcept;

}
