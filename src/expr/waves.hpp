#pragma once

#include "expr/interval.hpp"
#include "expr/shape.hpp"

#include <array>

namespace biyos
{

struct Wave
{
    std::array<Curvature, 4> curvatures;
    std::array<Monotonicity, 4> monotonicities;
    bool poles;
};

extern const Wave kSine;
extern const Wave kCosine;
extern const Wave kTangent;

Profile wave(const Wave &shape, Interval child) noexcept;

}
