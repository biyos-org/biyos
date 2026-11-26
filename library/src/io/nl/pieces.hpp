#pragma once

#include "core/numbers.hpp"
#include "io/lines.hpp"

#include <span>
#include <vector>

namespace biyos::nl
{

struct Curve
{
    std::vector<Real> x;
    std::vector<Real> y;
};

Curve curve_of(const Lines &lines, std::span<const Real> slopes, std::span<const Real> breaks);

}
