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
