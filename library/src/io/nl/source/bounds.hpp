#pragma once

#include "core/numbers.hpp"
#include "io/nl/source/source.hpp"

#include <optional>

namespace biyos::nl
{

struct Bounds
{
    Real lower;
    Real upper;
};

std::optional<Bounds> bounds_of(Source &source);

}
