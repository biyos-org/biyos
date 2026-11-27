#pragma once

#include "core/numbers.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"

#include <span>

namespace biyos
{

Curvature sum_curvature(std::span<const Real> weights, std::span<Argument> arguments) noexcept;
Curvature product_curvature(Real coefficient,
                            const Operands &factors,
                            std::span<Argument> arguments) noexcept;
Curvature quotient_curvature(Interval numerator,
                             Interval denominator,
                             std::span<Argument> arguments) noexcept;
Curvature angle_curvature(Interval y, Interval x, std::span<Argument> arguments) noexcept;
Curvature largest_curvature(const Operands &operands, std::span<Argument> arguments) noexcept;
Curvature smallest_curvature(const Operands &operands, std::span<Argument> arguments) noexcept;

}
