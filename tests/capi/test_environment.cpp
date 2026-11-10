#include "biyos/biyos.h"

#include <gtest/gtest.h>

TEST(CApiTest, CreateAndFree)
{
    biyos_env *env = nullptr;
    ASSERT_EQ(biyos_env_create(&env), BIYOS_OK);
    EXPECT_NE(env, nullptr);
    EXPECT_STREQ(biyos_last_error_message(env), "");
    biyos_env_free(env);
}

TEST(CApiTest, CreateRejectsNull)
{
    EXPECT_EQ(biyos_env_create(nullptr), BIYOS_INVALID_ARGUMENT);
}

TEST(CApiTest, FreeAcceptsNull)
{
    biyos_env_free(nullptr);
}

TEST(CApiTest, MessageWithoutEnvironment)
{
    EXPECT_STREQ(biyos_last_error_message(nullptr), "");
}
