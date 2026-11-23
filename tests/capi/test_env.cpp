#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <latch>
#include <string>
#include <thread>

namespace
{

void fail_read(BIYOS_env *env, BIYOS_int id, std::latch &failed, std::string &seen)
{
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_getenvintopt(env, id, &value), BIYOS_ERR_BADARG);
    failed.arrive_and_wait();
    seen = biyos::test::last_message(env);
}

}

TEST(CApiTest, Create)
{
    BIYOS_env *env = nullptr;
    ASSERT_EQ(BIYOS_newenv(&env), BIYOS_OK);
    EXPECT_NE(env, nullptr);
    EXPECT_STREQ(biyos::test::last_message(env), "");
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_OK);
}

TEST(CApiTest, NullCreate)
{
    EXPECT_EQ(BIYOS_newenv(nullptr), BIYOS_ERR_BADARG);
}

TEST(CApiTest, NullFree)
{
    EXPECT_EQ(BIYOS_freeenv(nullptr), BIYOS_OK);
}

TEST(CApiTest, NoError)
{
    const biyos::test::Env env;
    const BIYOS_error error = biyos::test::last_error(env.get());
    EXPECT_EQ(error.status, BIYOS_OK);
    EXPECT_EQ(error.mdl, nullptr);
    EXPECT_STREQ(error.msg, "");
}

TEST(CApiTest, ErrorModel)
{
    const biyos::test::Env env;
    const biyos::test::Model model(env);
    EXPECT_EQ(BIYOS_getnvar(model.get(), nullptr), BIYOS_ERR_BADARG);
    const BIYOS_error error = biyos::test::last_error(env.get());
    EXPECT_EQ(error.status, BIYOS_ERR_BADARG);
    EXPECT_EQ(error.mdl, model.get());
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(biyos::test::last_error(env.get()).mdl, nullptr);
}

TEST(CApiTest, ErrorThreads)
{
    const biyos::test::Env env;
    std::latch failed(2);
    std::string first;
    std::string second;
    std::thread one(fail_read, env.get(), 998, std::ref(failed), std::ref(first));
    std::thread two(fail_read, env.get(), 999, std::ref(failed), std::ref(second));
    one.join();
    two.join();
    EXPECT_NE(first.find("998"), std::string::npos);
    EXPECT_NE(second.find("999"), std::string::npos);
    EXPECT_STREQ(env.message().c_str(), "");
}

TEST(CApiTest, ShortError)
{
    const biyos::test::Env env;
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::string held = env.message();
    BIYOS_error error{};
    error.size = sizeof error;
    EXPECT_EQ(BIYOS_lasterror(nullptr, &error), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_lasterror(env.get(), nullptr), BIYOS_ERR_BADARG);
    error.size = sizeof error - 1;
    EXPECT_EQ(BIYOS_lasterror(env.get(), &error), BIYOS_ERR_BADARG);
    EXPECT_EQ(error.msg, nullptr);
    EXPECT_EQ(env.message(), held);
    EXPECT_EQ(biyos::test::last_error(env.get()).status, BIYOS_ERR_BADARG);
}
