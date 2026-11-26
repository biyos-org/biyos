#include "core/numeric/identical.hpp"

#include "core/numbers.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <limits>

using biyos::Bits;
using biyos::identical;
using biyos::Real;

TEST(IdenticalTest, Equal)
{
    EXPECT_TRUE(identical(1.5, 1.5));
    EXPECT_FALSE(identical(1.0, std::nextafter(1.0, 2.0)));
}

TEST(IdenticalTest, Zeros)
{
    EXPECT_TRUE(identical(0.0, 0.0));
    EXPECT_FALSE(identical(0.0, -0.0));
}

TEST(IdenticalTest, NaNs)
{
    const Real nan = std::numeric_limits<Real>::quiet_NaN();
    const Real other = std::bit_cast<Real>(std::bit_cast<Bits>(nan) | 1);
    EXPECT_TRUE(identical(nan, nan));
    EXPECT_FALSE(identical(nan, other));
}
