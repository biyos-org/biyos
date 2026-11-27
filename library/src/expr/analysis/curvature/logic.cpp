#include "expr/analysis/curvature/logic.hpp"

#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"

#include <algorithm>
#include <span>

namespace biyos
{

namespace
{

constexpr Argument kHeld{.monotonicity = Monotonicity::Constant};

}

Curvature condition_curvature(Interval condition, std::span<Argument> arguments) noexcept
{
    const bool decided = is_point(condition);
    std::ranges::fill(arguments, decided ? kHeld : Argument{});
    return decided ? Curvature::Affine : Curvature::Unknown;
}

}
