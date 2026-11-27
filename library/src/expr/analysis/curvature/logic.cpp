#include "expr/analysis/curvature/logic.hpp"

#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"

#include <algorithm>
#include <cstddef>
#include <span>

namespace biyos
{

namespace
{

constexpr Argument kHeld{.monotonicity = Monotonicity::Constant};
constexpr Argument kPassed{.monotonicity = Monotonicity::Nondecreasing};
constexpr std::size_t kThen = 1;
constexpr std::size_t kOtherwise = 2;

}

Curvature condition_curvature(Interval condition, std::span<Argument> arguments) noexcept
{
    const bool decided = is_point(condition);
    std::ranges::fill(arguments, decided ? kHeld : Argument{});
    return decided ? Curvature::Affine : Curvature::Unknown;
}

Curvature branch_curvature(Interval condition, std::span<Argument> arguments) noexcept
{
    const bool then = condition.lower() > 0.0;
    const bool decided = then || condition.upper() <= 0.0;
    std::ranges::fill(arguments, decided ? kHeld : Argument{});
    if (!decided)
    {
        return Curvature::Unknown;
    }
    arguments[then ? kThen : kOtherwise] = kPassed;
    return Curvature::Affine;
}

}
