#pragma once

#include "core/options.hpp"

namespace biyos
{

class Tolerances
{
  public:
    explicit Tolerances(const Options &options);

    bool is_zero(double x) const noexcept;
    bool approx_equal(double a, double b) const noexcept;
    bool satisfies_lower(double x, double lower) const noexcept;
    bool satisfies_upper(double x, double upper) const noexcept;
    bool is_integral(double x) const noexcept;

  private:
    bool within(double gap, double scaled_gap) const noexcept;

    double feasabs_;
    double feasrel_;
    double inttol_;
    double zerotol_;
};

}
