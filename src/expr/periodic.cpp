#include "expr/periodic.hpp"

#include "expr/interval.hpp"
#include "expr/rounding.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

namespace biyos
{

namespace
{

constexpr double kPi = std::numbers::pi;
constexpr Interval kUnit{-1.0, 1.0};

struct Wave
{
    double (*value)(double);
    double (*slope)(double);
};

constexpr Wave kSine{
    .value = [](double x) { return std::sin(x); },
    .slope = [](double x) { return std::cos(x); },
};

constexpr Wave kCosine{
    .value = [](double x) { return std::cos(x); },
    .slope = [](double x) { return -std::sin(x); },
};

bool is_finite(Interval value) noexcept
{
    return std::isfinite(value.lower()) && std::isfinite(value.upper());
}

bool narrow(double lower, double upper) noexcept
{
    return upper - lower < kPi;
}

bool same_sign(double left, double right) noexcept
{
    return (left > 0.0 && right > 0.0) || (left < 0.0 && right < 0.0);
}

Interval arc(const Wave &wave, double lower, double upper) noexcept
{
    const double first = wave.value(lower);
    const double last = wave.value(upper);
    const double start = wave.slope(lower);
    const double end = wave.slope(upper);
    const bool point = !(lower < upper);
    const bool peak = !point && start >= 0.0 && end <= 0.0;
    const bool trough = !point && start <= 0.0 && end >= 0.0;
    const double low = trough ? -1.0 : widen_down((std::min)(first, last));
    const double high = peak ? 1.0 : widen_up((std::max)(first, last));
    return meet(Interval(low, high), kUnit);
}

Interval wave_of(const Wave &wave, Interval value) noexcept
{
    if (value.is_empty())
    {
        return value;
    }
    if (!is_finite(value))
    {
        return kUnit;
    }
    const double lower = value.lower();
    const double upper = value.upper();
    if (narrow(lower, upper))
    {
        return arc(wave, lower, upper);
    }
    const double middle = std::midpoint(lower, upper);
    if (narrow(lower, middle) && narrow(middle, upper))
    {
        return hull(arc(wave, lower, middle), arc(wave, middle, upper));
    }
    return kUnit;
}

}

Interval sin(Interval value) noexcept
{
    return wave_of(kSine, value);
}

Interval cos(Interval value) noexcept
{
    return wave_of(kCosine, value);
}

Interval tan(Interval value) noexcept
{
    if (value.is_empty())
    {
        return value;
    }
    const double lower = value.lower();
    const double upper = value.upper();
    if (!is_finite(value) || !narrow(lower, upper))
    {
        return Interval::entire();
    }
    if (!same_sign(std::cos(lower), std::cos(upper)))
    {
        return Interval::entire();
    }
    return {widen_down(std::tan(lower)), widen_up(std::tan(upper))};
}

}
