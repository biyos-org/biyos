#pragma once

#include "core/numbers.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct PiecewiseLinear
{
    std::span<const Real> x;
    std::span<const Real> y;
};

class Points
{
  public:
    Points();

    Offset size() const noexcept;
    PiecewiseLinear piecewise_linear(Offset index) const noexcept;

    Offset add(std::span<const Real> x, std::span<const Real> y);
    void truncate(Offset size) noexcept;

  private:
    std::vector<Offset> starts_;
    std::vector<Real> x_;
    std::vector<Real> y_;
};

void check_points(std::span<const Real> x, std::span<const Real> y);
void check_piecewise_linear(Offset pwl, Offset size);
std::size_t segment_of(std::span<const Real> x, Real t) noexcept;
Real slope_of(const PiecewiseLinear &pwl, std::size_t segment) noexcept;
Real value_at(const PiecewiseLinear &pwl, Real t) noexcept;
Real slope_at(const PiecewiseLinear &pwl, Real t) noexcept;

}
