#include "core/tolerances.hpp"

#include <algorithm>
#include <cmath>

namespace biyos
{

namespace
{

bool same_infinity(double a, double b)
{
    return std::isinf(a) && std::isinf(b) && std::signbit(a) == std::signbit(b);
}

}

Tolerances::Tolerances(const Options &options)
    : feasibility_abs_(options.get_double(Option::FeasibilityAbs)),
      feasibility_rel_(options.get_double(Option::FeasibilityRel)),
      integrality_tol_(options.get_double(Option::IntegralityTol)),
      zero_tol_(options.get_double(Option::ZeroTol))
{
}

bool Tolerances::is_zero(double x) const noexcept
{
    return std::abs(x) <= zero_tol_;
}

bool Tolerances::approx_equal(double a, double b) const noexcept
{
    if (std::isinf(a) || std::isinf(b))
    {
        return same_infinity(a, b);
    }
    const double scale = (std::max)({1.0, std::abs(a), std::abs(b)});
    return within(std::abs(a - b), std::abs(a / scale - b / scale));
}

bool Tolerances::satisfies_lower(double x, double lower) const noexcept
{
    if (std::isinf(lower) || std::isinf(x))
    {
        return x >= lower;
    }
    const double scale = (std::max)(1.0, std::abs(lower));
    return within(lower - x, lower / scale - x / scale);
}

bool Tolerances::satisfies_upper(double x, double upper) const noexcept
{
    if (std::isinf(upper) || std::isinf(x))
    {
        return x <= upper;
    }
    const double scale = (std::max)(1.0, std::abs(upper));
    return within(x - upper, x / scale - upper / scale);
}

bool Tolerances::is_integral(double x) const noexcept
{
    return std::abs(x - std::round(x)) <= integrality_tol_;
}

bool Tolerances::within(double gap, double scaled_gap) const noexcept
{
    return gap <= feasibility_abs_ || scaled_gap <= feasibility_rel_;
}

}
