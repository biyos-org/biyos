#include "expr/rounding.hpp"

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
constexpr std::int64_t kInfiniteOrder =
    std::bit_cast<std::int64_t>(std::numeric_limits<double>::infinity());
constexpr int kBoundUlps = kLibmUlps + 1;
constexpr int kHundredths = 100;
constexpr int kHundredthStepsPerUlp = 1608;

static_assert(kLibmSteps * kHundredths >= kHundredthStepsPerUlp * kBoundUlps);

bool is_zero(double value) noexcept
{
    return std::fpclassify(value) == FP_ZERO;
}

std::int64_t order_of(double value) noexcept
{
    const auto bits = std::bit_cast<std::int64_t>(value);
    return bits < 0 ? kSignOrder - bits : bits;
}

double value_of(std::int64_t order) noexcept
{
    return std::bit_cast<double>(order < 0 ? kSignOrder - order : order);
}

}

double shifted(double value, int steps) noexcept
{
    if (std::isnan(value))
    {
        return value;
    }
    const std::int64_t order = order_of(value) + steps;
    return value_of(std::clamp(order, -kInfiniteOrder, kInfiniteOrder));
}

double round_down(double value) noexcept
{
    if (is_zero(value) && !std::signbit(value))
    {
        return value;
    }
    return shifted(value, -kRoundedSteps);
}

double round_up(double value) noexcept
{
    if (is_zero(value) && std::signbit(value))
    {
        return value;
    }
    return shifted(value, kRoundedSteps);
}

double widen_down(double value) noexcept
{
    return shifted(value, -kLibmSteps);
}

double widen_up(double value) noexcept
{
    return shifted(value, kLibmSteps);
}

}
