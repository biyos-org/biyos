#include "biyos/biyos.h"

#include <gtest/gtest.h>

TEST(CApiTest, Create)
{
    BIYOS_env *env = nullptr;
    ASSERT_EQ(BIYOS_newenv(&env), BIYOS_OK);
    EXPECT_NE(env, nullptr);
    EXPECT_STREQ(BIYOS_lasterror(env), "");
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_OK);
}

TEST(CApiTest, NullCreate)
{
    EXPECT_EQ(BIYOS_newenv(nullptr), BIYOS_INVALID_ARGUMENT);
}

TEST(CApiTest, NullFree)
{
    EXPECT_EQ(BIYOS_freeenv(nullptr), BIYOS_OK);
}

TEST(CApiTest, NullMessage)
{
    EXPECT_STREQ(BIYOS_lasterror(nullptr), "");
}
