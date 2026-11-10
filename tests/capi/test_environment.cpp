#include "biyos/biyos.h"

#include <gtest/gtest.h>

TEST(CApiTest, Create)
{
    biyos_env *env = nullptr;
    ASSERT_EQ(biyos_env_create(&env), BIYOS_OK);
    EXPECT_NE(env, nullptr);
    EXPECT_STREQ(biyos_last_error_message(env), "");
    EXPECT_EQ(biyos_env_free(env), BIYOS_OK);
}

TEST(CApiTest, NullCreate)
{
    EXPECT_EQ(biyos_env_create(nullptr), BIYOS_INVALID_ARGUMENT);
}

TEST(CApiTest, NullFree)
{
    EXPECT_EQ(biyos_env_free(nullptr), BIYOS_OK);
}

TEST(CApiTest, NullMessage)
{
    EXPECT_STREQ(biyos_last_error_message(nullptr), "");
}
