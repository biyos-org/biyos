#include "expr/curvature/shape.hpp"

#include "expr/intervals/interval.hpp"

#include <cstddef>

namespace biyos
{

bool is_convex(Curvature curvature) noexcept
{
    switch (curvature)
    {
    case Curvature::Convex:
    case Curvature::Affine:
        return true;
    case Curvature::Unknown:
    case Curvature::Concave:
        return false;
    }
    return false;
}

bool is_concave(Curvature curvature) noexcept
{
    switch (curvature)
    {
    case Curvature::Concave:
    case Curvature::Affine:
        return true;
    case Curvature::Unknown:
    case Curvature::Convex:
        return false;
    }
    return false;
}

Curvature curvature_of(bool convex, bool concave) noexcept
{
    if (convex)
    {
        return concave ? Curvature::Affine : Curvature::Convex;
    }
    return concave ? Curvature::Concave : Curvature::Unknown;
}

Curvature common(Curvature left, Curvature right) noexcept
{
    const bool convex = is_convex(left) && is_convex(right);
    return curvature_of(convex, is_concave(left) && is_concave(right));
}

bool rises(Monotonicity monotonicity) noexcept
{
    switch (monotonicity)
    {
    case Monotonicity::Nondecreasing:
    case Monotonicity::Constant:
        return true;
    case Monotonicity::Unknown:
    case Monotonicity::Nonincreasing:
        return false;
    }
    return false;
}

bool falls(Monotonicity monotonicity) noexcept
{
    switch (monotonicity)
    {
    case Monotonicity::Nonincreasing:
    case Monotonicity::Constant:
        return true;
    case Monotonicity::Unknown:
    case Monotonicity::Nondecreasing:
        return false;
    }
    return false;
}

Monotonicity monotonicity_of(bool rising, bool falling) noexcept
{
    if (rising)
    {
        return falling ? Monotonicity::Constant : Monotonicity::Nondecreasing;
    }
    return falling ? Monotonicity::Nonincreasing : Monotonicity::Unknown;
}

Monotonicity common(Monotonicity left, Monotonicity right) noexcept
{
    return monotonicity_of(rises(left) && rises(right), falls(left) && falls(right));
}

Monotonicity monotonicity_of(Interval slope) noexcept
{
    if (slope.is_empty())
    {
        return Monotonicity::Unknown;
    }
    return monotonicity_of(slope.lower() >= 0.0, slope.upper() <= 0.0);
}

bool is_point(Interval value) noexcept
{
    return !value.is_empty() && !(value.lower() < value.upper());
}

bool any_empty(const Operands &operands) noexcept
{
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        if (operands[k].is_empty())
        {
            return true;
        }
    }
    return false;
}

}
