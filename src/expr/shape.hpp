#pragma once

#include "expr/interval.hpp"

#include <cstdint>

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

bool is_convex(Curvature curvature) noexcept;
bool is_concave(Curvature curvature) noexcept;
Curvature curvature_of(bool convex, bool concave) noexcept;
Curvature common(Curvature left, Curvature right) noexcept;
bool rises(Monotonicity monotonicity) noexcept;
bool falls(Monotonicity monotonicity) noexcept;
Monotonicity monotonicity_of(bool rising, bool falling) noexcept;
Monotonicity common(Monotonicity left, Monotonicity right) noexcept;
bool is_point(Interval value) noexcept;

}
