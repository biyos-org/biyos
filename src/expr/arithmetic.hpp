#pragma once

#include "expr/intervals/interval.hpp"
#include "expr/shape.hpp"

#include <span>

namespace biyos
{

Curvature sum_curvature(std::span<const double> weights, std::span<Argument> arguments) noexcept;
Curvature product_curvature(double coefficient,
                            const Operands &factors,
                            std::span<Argument> arguments) noexcept;
Curvature quotient_curvature(Interval numerator,
                             Interval denominator,
                             std::span<Argument> arguments) noexcept;
Curvature largest_curvature(const Operands &operands, std::span<Argument> arguments) noexcept;
Curvature smallest_curvature(const Operands &operands, std::span<Argument> arguments) noexcept;

}
