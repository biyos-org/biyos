#pragma once

#include <limits>

namespace biyos
{

class Interval
{
  public:
    constexpr Interval(double lower, double upper) noexcept
        : lower_(holds_reals(lower, upper) ? lower : kInfinity),
          upper_(holds_reals(lower, upper) ? upper : -kInfinity)
    {
    }

    static constexpr Interval empty() noexcept { return {kInfinity, -kInfinity}; }

    static constexpr Interval entire() noexcept { return {-kInfinity, kInfinity}; }

    constexpr bool is_empty() const noexcept { return lower_ > upper_; }

    constexpr double lower() const noexcept { return lower_; }

    constexpr double upper() const noexcept { return upper_; }

  private:
    static constexpr double kInfinity = std::numeric_limits<double>::infinity();

    static constexpr bool holds_reals(double lower, double upper) noexcept
    {
        return lower <= upper && lower < kInfinity && upper > -kInfinity;
    }

    double lower_;
    double upper_;
};

static_assert(sizeof(Interval) == 2 * sizeof(double));

Interval operator+(Interval left, Interval right) noexcept;
Interval operator-(Interval value) noexcept;
Interval operator*(Interval left, Interval right) noexcept;
Interval operator/(Interval left, Interval right) noexcept;
Interval hull(Interval left, Interval right) noexcept;
Interval meet(Interval left, Interval right) noexcept;
Interval maximum(Interval left, Interval right) noexcept;
Interval minimum(Interval left, Interval right) noexcept;
Interval abs(Interval value) noexcept;
Interval signs(Interval value, double root) noexcept;

}
