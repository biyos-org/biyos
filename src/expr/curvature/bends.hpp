#pragma once

#include "expr/curvature/shape.hpp"
#include "expr/intervals/interval.hpp"

#include <limits>

namespace biyos
{

struct Domain
{
    Interval closure;
    bool open_lower = false;
    bool open_upper = false;
};

template <typename T> struct Split
{
    Interval at;
    T below;
    T above;
    T across;
};

struct Bend
{
    Domain domain;
    Split<Curvature> curvature;
    Split<Monotonicity> monotonicity;
};

template <typename T> constexpr Split<T> everywhere(T value) noexcept
{
    return {.at = {0.0, 0.0}, .below = value, .above = value, .across = value};
}

template <typename T> constexpr Split<T> at_zero(T below, T above, T across) noexcept
{
    return {.at = {0.0, 0.0}, .below = below, .above = above, .across = across};
}

inline constexpr Domain kReals{.closure = Interval::entire()};
inline constexpr Domain kNonnegatives{.closure = {0.0, std::numeric_limits<double>::infinity()}};
inline constexpr Domain kPositives{
    .closure = {0.0, std::numeric_limits<double>::infinity()},
    .open_lower = true,
};
inline constexpr Domain kNegatives{
    .closure = {-std::numeric_limits<double>::infinity(), 0.0},
    .open_upper = true,
};

extern const Bend kNegation;
extern const Bend kExp;
extern const Bend kLog;
extern const Bend kSqrt;
extern const Bend kAbs;
extern const Bend kAsin;
extern const Bend kAcos;
extern const Bend kAtan;
extern const Bend kSinh;
extern const Bend kCosh;
extern const Bend kTanh;
extern const Bend kAsinh;
extern const Bend kAcosh;
extern const Bend kAtanh;
extern const Bend kEntropy;
extern const Bend kErf;

Profile bend(const Bend &shape, Interval child) noexcept;

}
