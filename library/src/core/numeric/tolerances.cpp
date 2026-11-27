#include "core/numeric/tolerances.hpp"

#include "core/numbers.hpp"

#include <algorithm>
#include <cmath>

namespace biyos
{

namespace
{

bool same_infinity(Real a, Real b)
{
    return std::isinf(a) && std::isinf(b) && std::signbit(a) == std::signbit(b);
}

bool close(Real a, Real b, Real absolute, Real relative)
{
    if (std::isinf(a) || std::isinf(b))
    {
        return same_infinity(a, b);
    }
    const Real scale = (std::max)({1.0, std::abs(a), std::abs(b)});
    return std::abs(a - b) <= absolute || std::abs(a / scale - b / scale) <= relative;
}

}

Tolerances::Tolerances(const Lookup &lookup)
    : feasabs_(lookup.get_double(Option::FeasAbs)), feasrel_(lookup.get_double(Option::FeasRel)),
      cmpabs_(lookup.get_double(Option::CmpAbs)), cmprel_(lookup.get_double(Option::CmpRel)),
      inttol_(lookup.get_double(Option::IntTol)), zerotol_(lookup.get_double(Option::ZeroTol))
{
}

bool Tolerances::is_zero(Real x) const noexcept
{
    return std::abs(x) <= zerotol_;
}

bool Tolerances::approx_equal(Real a, Real b) const noexcept
{
    return close(a, b, feasabs_, feasrel_);
}

bool Tolerances::at_most(Real a, Real b) const noexcept
{
    return a <= b || approx_equal(a, b);
}

bool Tolerances::compares_equal(Real a, Real b) const noexcept
{
    return close(a, b, cmpabs_, cmprel_);
}

bool Tolerances::compares_at_most(Real a, Real b) const noexcept
{
    return a <= b || compares_equal(a, b);
}

bool Tolerances::satisfies_lower(Real x, Real lower) const noexcept
{
    if (std::isinf(lower) || std::isinf(x))
    {
        return x >= lower;
    }
    const Real scale = (std::max)(1.0, std::abs(lower));
    return within(lower - x, lower / scale - x / scale);
}

bool Tolerances::satisfies_upper(Real x, Real upper) const noexcept
{
    if (std::isinf(upper) || std::isinf(x))
    {
        return x <= upper;
    }
    const Real scale = (std::max)(1.0, std::abs(upper));
    return within(x - upper, x / scale - upper / scale);
}

bool Tolerances::is_integral(Real x) const noexcept
{
    return std::abs(x - std::round(x)) <= inttol_;
}

bool Tolerances::within(Real gap, Real scaled_gap) const noexcept
{
    return gap <= feasabs_ || scaled_gap <= feasrel_;
}

}
