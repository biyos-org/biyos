#pragma once

#include "core/numbers.hpp"
#include "core/options/lookup.hpp"

namespace biyos
{

class Tolerances
{
  public:
    explicit Tolerances(const Lookup &lookup);

    bool is_zero(Real x) const noexcept;
    bool approx_equal(Real a, Real b) const noexcept;
    bool at_most(Real a, Real b) const noexcept;
    bool compares_equal(Real a, Real b) const noexcept;
    bool compares_at_most(Real a, Real b) const noexcept;
    bool satisfies_lower(Real x, Real lower) const noexcept;
    bool satisfies_upper(Real x, Real upper) const noexcept;
    bool is_integral(Real x) const noexcept;

  private:
    bool within(Real gap, Real scaled_gap) const noexcept;

    Real feasabs_;
    Real feasrel_;
    Real cmpabs_;
    Real cmprel_;
    Real inttol_;
    Real zerotol_;
};

}
