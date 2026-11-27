#include "core/env/version.hpp"

#include <gtest/gtest.h>

#include <string>

using biyos::number_of;
using biyos::numbered;
using biyos::Version;
using biyos::version;

TEST(VersionTest, MatchesProjectVersion)
{
    const Version v = version();
    const std::string text =
        std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
    EXPECT_EQ(text, BIYOS_EXPECTED_VERSION);
}

TEST(VersionTest, Order)
{
    const Version first{0, 1, 0};
    EXPECT_EQ(first, (Version{0, 1, 0}));
    EXPECT_LT((Version{0, 0, 9}), first);
    EXPECT_LT(first, (Version{0, 1, 1}));
    EXPECT_LT((Version{0, 9, 9}), (Version{1, 0, 0}));
}

TEST(VersionTest, Number)
{
    EXPECT_EQ(number_of({0, 1, 0}), 100);
    EXPECT_EQ(number_of({1, 2, 3}), 10203);
    EXPECT_EQ(number_of({12, 99, 99}), 129999);
    EXPECT_TRUE(numbered({0, 99, 99}));
    EXPECT_FALSE(numbered({0, 100, 0}));
    EXPECT_FALSE(numbered({0, 0, 100}));
    EXPECT_FALSE(numbered({-1, 0, 0}));
    EXPECT_FALSE(numbered({0, -1, 0}));
    EXPECT_FALSE(numbered({0, 0, -1}));
}
