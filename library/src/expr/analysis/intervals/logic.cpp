#include "expr/analysis/intervals/logic.hpp"

#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/analysis/intervals/interval.hpp"

namespace biyos
{

namespace
{

constexpr Interval kTrue{1.0, 1.0};
constexpr Interval kFalse{0.0, 0.0};
constexpr Interval kUnknown{0.0, 1.0};

bool narrow(const Tolerances &tolerances) noexcept
{
    return !tolerances.at_most(1.0, 0.0);
}

bool settled(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return !(b < 0.0 && a > 0.0) || narrow(tolerances);
}

bool holds(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return settled(tolerances, a, b) && tolerances.at_most(a, b);
}

bool fails(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return settled(tolerances, a, b) && !tolerances.at_most(a, b);
}

}

Interval less_equal(const Tolerances &tolerances, Interval a, Interval b) noexcept
{
    if (a.is_empty() || b.is_empty())
    {
        return Interval::empty();
    }
    if (holds(tolerances, a.upper(), b.lower()))
    {
        return kTrue;
    }
    if (fails(tolerances, a.lower(), b.upper()))
    {
        return kFalse;
    }
    return kUnknown;
}

Interval greater_equal(const Tolerances &tolerances, Interval a, Interval b) noexcept
{
    return less_equal(tolerances, b, a);
}

Interval equal(const Tolerances &tolerances, Interval a, Interval b) noexcept
{
    return minimum(less_equal(tolerances, a, b), greater_equal(tolerances, a, b));
}

Interval less(const Tolerances &tolerances, Interval a, Interval b) noexcept
{
    return complement(greater_equal(tolerances, a, b));
}

Interval greater(const Tolerances &tolerances, Interval a, Interval b) noexcept
{
    return complement(less_equal(tolerances, a, b));
}

Interval not_equal(const Tolerances &tolerances, Interval a, Interval b) noexcept
{
    return complement(equal(tolerances, a, b));
}

Interval complement(Interval condition) noexcept
{
    return {1.0 - condition.upper(), 1.0 - condition.lower()};
}

Interval taken(Interval condition, Interval then, Interval otherwise) noexcept
{
    if (condition.is_empty())
    {
        return Interval::empty();
    }
    if (condition.lower() > 0.0)
    {
        return then;
    }
    if (condition.upper() <= 0.0)
    {
        return otherwise;
    }
    return hull(then, otherwise);
}

}
