#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <string>

TEST(CApiTest, VersionMatchesProjectVersion)
{
    int major = -1;
    int minor = -1;
    int patch = -1;
    biyos_version(&major, &minor, &patch);
    const std::string text =
        std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    EXPECT_EQ(text, BIYOS_EXPECTED_VERSION);
}

TEST(CApiTest, VersionAcceptsNullArguments)
{
    int minor = -1;
    biyos_version(nullptr, &minor, nullptr);
    EXPECT_GE(minor, 0);
}
