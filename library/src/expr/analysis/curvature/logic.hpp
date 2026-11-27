#pragma once

#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"

#include <span>

namespace biyos
{

Curvature condition_curvature(Interval condition, std::span<Argument> arguments) noexcept;

}
