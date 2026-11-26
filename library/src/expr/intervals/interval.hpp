#pragma once

#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"

namespace biyos
{

class Interval
{
  public:
    constexpr Interval(Real lower, Real upper) noexcept
        : lower_(holds_reals(lower, upper) ? lower : kInfinity),
          upper_(holds_reals(lower, upper) ? upper : -kInfinity)
    {
    }

    static constexpr Interval empty() noexcept { return {kInfinity, -kInfinity}; }

    static constexpr Interval entire() noexcept { return {-kInfinity, kInfinity}; }

    constexpr bool is_empty() const noexcept { return lower_ > upper_; }

    constexpr Real lower() const noexcept { return lower_; }

    constexpr Real upper() const noexcept { return upper_; }

  private:
    static constexpr bool holds_reals(Real lower, Real upper) noexcept
    {
        return lower <= upper && lower < kInfinity && upper > -kInfinity;
    }

    Real lower_;
    Real upper_;
};

static_assert(sizeof(Interval) == 2 * sizeof(Real));

Interval operator+(Interval left, Interval right) noexcept;
Interval operator-(Interval value) noexcept;
Interval operator*(Interval left, Interval right) noexcept;
Interval operator/(Interval left, Interval right) noexcept;
Interval hull(Interval left, Interval right) noexcept;
Interval meet(Interval left, Interval right) noexcept;
Interval maximum(Interval left, Interval right) noexcept;
Interval minimum(Interval left, Interval right) noexcept;
Interval abs(Interval value) noexcept;
Interval signs(Interval value, Real root) noexcept;

}
