#pragma once

#include "model/overrides.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>

namespace biyos::test
{

inline void expect_part(double found, double wanted)
{
    EXPECT_EQ(std::bit_cast<std::uint64_t>(found), std::bit_cast<std::uint64_t>(wanted));
}

inline void expect_tolerance(const Override &found, double absolute, double relative)
{
    expect_part(found.absolute, absolute);
    expect_part(found.relative, relative);
}

}
