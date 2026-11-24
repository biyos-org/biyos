#pragma once

#include "core/index.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Pwl
{
    std::span<const double> x;
    std::span<const double> y;
};

class Points
{
  public:
    Points();

    Offset size() const noexcept;
    Pwl pwl(Offset index) const noexcept;

    Offset add(std::span<const double> x, std::span<const double> y);
    void truncate(Offset size) noexcept;

  private:
    std::vector<Offset> starts_;
    std::vector<double> x_;
    std::vector<double> y_;
};

void check_points(std::span<const double> x, std::span<const double> y);
void check_pwl(Offset pwl, Offset size);
std::size_t segment_of(std::span<const double> x, double t) noexcept;
double slope_of(const Pwl &pwl, std::size_t segment) noexcept;
double value_at(const Pwl &pwl, double t) noexcept;
double slope_at(const Pwl &pwl, double t) noexcept;

}
