#include "expr/analysis/intervals/rounding.hpp"

#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "core/numeric/zero.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace biyos
{

namespace
{

constexpr std::int64_t kSignOrder = (std::numeric_limits<std::int64_t>::min)();
constexpr std::int64_t kInfiniteOrder = std::bit_cast<std::int64_t>(kInfinity);
constexpr int kBoundUlps = kLibmUlps + 1;
constexpr int kHundredths = 100;
constexpr int kHundredthStepsPerUlp = 1608;

static_assert(kLibmSteps * kHundredths >= kHundredthStepsPerUlp * kBoundUlps);

std::int64_t order_of(Real value) noexcept
{
    const auto bits = std::bit_cast<std::int64_t>(value);
    return bits < 0 ? kSignOrder - bits : bits;
}

Real value_of(std::int64_t order) noexcept
{
    return std::bit_cast<Real>(order < 0 ? kSignOrder - order : order);
}

}

Real shifted(Real value, int steps) noexcept
{
    if (std::isnan(value))
    {
        return value;
    }
    const std::int64_t order = order_of(value) + steps;
    return value_of(std::clamp(order, -kInfiniteOrder, kInfiniteOrder));
}

Real round_down(Real value) noexcept
{
    if (is_zero(value) && !std::signbit(value))
    {
        return value;
    }
    return shifted(value, -kRoundedSteps);
}

Real round_up(Real value) noexcept
{
    if (is_zero(value) && std::signbit(value))
    {
        return value;
    }
    return shifted(value, kRoundedSteps);
}

Real widen_down(Real value) noexcept
{
    return shifted(value, -kLibmSteps);
}

Real widen_up(Real value) noexcept
{
    return shifted(value, kLibmSteps);
}

}
