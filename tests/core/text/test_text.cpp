#include "core/text/text.hpp"

#include "core/numbers.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>

using biyos::Offset;
using biyos::Real;
using biyos::text_of;

TEST(TextTest, Widest)
{
    EXPECT_EQ(text_of((std::numeric_limits<Offset>::min)()), "-9223372036854775808");
    EXPECT_EQ(text_of((std::numeric_limits<std::size_t>::max)()), "18446744073709551615");
    EXPECT_EQ(text_of(-(std::numeric_limits<Real>::min)()), "-2.2250738585072014e-308");
}
