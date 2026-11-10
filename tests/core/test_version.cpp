#include "core/version.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(VersionTest, MatchesProjectVersion)
{
    const biyos::Version v = biyos::version();
    const std::string text =
        std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
    EXPECT_EQ(text, BIYOS_EXPECTED_VERSION);
}

TEST(VersionTest, Order)
{
    const biyos::Version first{0, 1, 0};
    EXPECT_EQ(first, (biyos::Version{0, 1, 0}));
    EXPECT_LT((biyos::Version{0, 0, 9}), first);
    EXPECT_LT(first, (biyos::Version{0, 1, 1}));
    EXPECT_LT((biyos::Version{0, 9, 9}), (biyos::Version{1, 0, 0}));
}
