#pragma once

#include "core/numbers.hpp"
#include "io/nl/source/source.hpp"

#include <optional>

namespace biyos::nl
{

inline constexpr Offset kPairing = 5;
inline constexpr Offset kLowerFinite = 1;
inline constexpr Offset kUpperFinite = 2;
inline constexpr Offset kBothFinite = kLowerFinite + kUpperFinite;

struct Bounds
{
    Real lower;
    Real upper;
};

struct Pairing
{
    Index row;
    Offset finite;
    Index variable;
};

Offset finite_of(Real lower, Real upper) noexcept;
std::optional<Bounds> bounds_of(Source &source);
Pairing pairing_of(Source &source, Index row, Index variables);

}
