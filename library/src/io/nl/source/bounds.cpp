#include "io/nl/source/bounds.hpp"

#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "io/nl/source/source.hpp"
#include "io/utils/records.hpp"

#include <cmath>
#include <optional>

namespace biyos::nl
{

namespace
{

Bounds interval(Source &source)
{
    const Real lower = source.real();
    return {.lower = lower, .upper = source.real()};
}

Bounds point(Source &source)
{
    const Real value = source.real();
    return {.lower = value, .upper = value};
}

Bounds coded_bounds(Source &source, Offset code)
{
    switch (code)
    {
    case 0:
        source.expect(3);
        return interval(source);
    case 1:
        source.expect(2);
        return {.lower = -kInfinity, .upper = source.real()};
    case 2:
        source.expect(2);
        return {.lower = source.real(), .upper = kInfinity};
    case 3:
        source.expect(1);
        return {.lower = -kInfinity, .upper = kInfinity};
    case 4:
        source.expect(2);
        return point(source);
    default:
        break;
    }
    source.bad("bound code " + quote(source.last_field()) + " is unknown");
}

}

Offset finite_of(Real lower, Real upper) noexcept
{
    return (std::isinf(lower) ? 0 : kLowerFinite) + (std::isinf(upper) ? 0 : kUpperFinite);
}

std::optional<Bounds> bounds_of(Source &source)
{
    const Offset code = source.code();
    if (code == kPairing)
    {
        return std::nullopt;
    }
    return coded_bounds(source, code);
}

Pairing pairing_of(Source &source, Index row, Index variables)
{
    source.expect(3);
    const Offset finite = source.integer();
    if (finite < 0 || finite > kBothFinite)
    {
        source.bad("complementarity code " + quote(source.last_field()) + " is unknown");
    }
    const Offset variable = source.integer();
    if (variable < 1 || variable > variables)
    {
        source.bad("complemented variable " + quote(source.last_field()) + " is out of range");
    }
    return {.row = row, .finite = finite, .variable = static_cast<Index>(variable - 1)};
}

}
