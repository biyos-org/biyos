#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <string>

namespace
{

class Env
{
  public:
    Env() { EXPECT_EQ(biyos_env_create(&ptr_), BIYOS_OK); }
    ~Env() { biyos_env_free(ptr_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    biyos_env *get() const { return ptr_; }
    std::string message() const { return biyos_last_error_message(ptr_); }

  private:
    biyos_env *ptr_ = nullptr;
};

}

TEST(CApiTest, OptionIds)
{
    const Env env;
    const char *names[] = {"seed",       "time_limit",      "check_level",     "presolve",
                           "reproducer", "feasibility_abs", "feasibility_rel", "integrality_tol",
                           "zero_tol",   "cpu_time_limit"};
    const int ids[] = {BIYOS_OPT_SEED,
                       BIYOS_OPT_TIME_LIMIT,
                       BIYOS_OPT_CHECK_LEVEL,
                       BIYOS_OPT_PRESOLVE,
                       BIYOS_OPT_REPRODUCER,
                       BIYOS_OPT_FEASIBILITY_ABS,
                       BIYOS_OPT_FEASIBILITY_REL,
                       BIYOS_OPT_INTEGRALITY_TOL,
                       BIYOS_OPT_ZERO_TOL,
                       BIYOS_OPT_CPU_TIME_LIMIT};
    for (int i = 0; i < 10; ++i)
    {
        int id = 0;
        EXPECT_EQ(biyos_option_id(env.get(), names[i], &id), BIYOS_OK);
        EXPECT_EQ(id, ids[i]) << names[i];
    }
}

TEST(CApiTest, OptionKinds)
{
    const Env env;
    const int ids[] = {BIYOS_OPT_SEED,
                       BIYOS_OPT_TIME_LIMIT,
                       BIYOS_OPT_CHECK_LEVEL,
                       BIYOS_OPT_PRESOLVE,
                       BIYOS_OPT_REPRODUCER,
                       BIYOS_OPT_FEASIBILITY_ABS,
                       BIYOS_OPT_FEASIBILITY_REL,
                       BIYOS_OPT_INTEGRALITY_TOL,
                       BIYOS_OPT_ZERO_TOL,
                       BIYOS_OPT_CPU_TIME_LIMIT};
    const int kinds[] = {BIYOS_KIND_INT,    BIYOS_KIND_DOUBLE, BIYOS_KIND_ENUM,   BIYOS_KIND_BOOL,
                         BIYOS_KIND_STRING, BIYOS_KIND_DOUBLE, BIYOS_KIND_DOUBLE, BIYOS_KIND_DOUBLE,
                         BIYOS_KIND_DOUBLE, BIYOS_KIND_DOUBLE};
    for (int i = 0; i < 10; ++i)
    {
        int kind = 0;
        EXPECT_EQ(biyos_option_kind(env.get(), ids[i], &kind), BIYOS_OK);
        EXPECT_EQ(kind, kinds[i]) << ids[i];
    }
}

TEST(CApiTest, UnknownName)
{
    const Env env;
    int id = 0;
    EXPECT_EQ(biyos_option_id(env.get(), "seeds", &id), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: option 'seeds': unknown");
}

TEST(CApiTest, UnknownId)
{
    const Env env;
    int value = 0;
    EXPECT_EQ(biyos_set_int_option(env.get(), 99, 1), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: option 99: unknown");
    EXPECT_EQ(biyos_get_int_option(env.get(), 0, &value), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: option 0: unknown");
    EXPECT_EQ(biyos_option_kind(env.get(), -1, &value), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: option -1: unknown");
}

TEST(CApiTest, Bool)
{
    const Env env;
    int value = -1;
    EXPECT_EQ(biyos_get_bool_option(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 1);
    EXPECT_EQ(biyos_set_bool_option(env.get(), BIYOS_OPT_PRESOLVE, 0), BIYOS_OK);
    EXPECT_EQ(biyos_get_bool_option(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 0);
}

TEST(CApiTest, NonzeroBool)
{
    const Env env;
    int value = -1;
    EXPECT_EQ(biyos_set_bool_option(env.get(), BIYOS_OPT_PRESOLVE, 0), BIYOS_OK);
    EXPECT_EQ(biyos_set_bool_option(env.get(), BIYOS_OPT_PRESOLVE, 2), BIYOS_OK);
    EXPECT_EQ(biyos_get_bool_option(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 1);
}

TEST(CApiTest, Int)
{
    const Env env;
    int value = -1;
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, 42), BIYOS_OK);
    EXPECT_EQ(biyos_get_int_option(env.get(), BIYOS_OPT_SEED, &value), BIYOS_OK);
    EXPECT_EQ(value, 42);
}

TEST(CApiTest, Double)
{
    const Env env;
    double value = -1.0;
    EXPECT_EQ(biyos_set_double_option(env.get(), BIYOS_OPT_TIME_LIMIT, 3.5), BIYOS_OK);
    EXPECT_EQ(biyos_get_double_option(env.get(), BIYOS_OPT_TIME_LIMIT, &value), BIYOS_OK);
    EXPECT_DOUBLE_EQ(value, 3.5);
}

TEST(CApiTest, String)
{
    const Env env;
    const char *value = nullptr;
    EXPECT_EQ(biyos_set_string_option(env.get(), BIYOS_OPT_REPRODUCER, "replay.txt"), BIYOS_OK);
    EXPECT_EQ(biyos_get_string_option(env.get(), BIYOS_OPT_REPRODUCER, &value), BIYOS_OK);
    EXPECT_STREQ(value, "replay.txt");
}

TEST(CApiTest, StringLifetime)
{
    const Env env;
    const char *path = nullptr;
    const char *level = nullptr;
    ASSERT_EQ(biyos_set_string_option(env.get(), BIYOS_OPT_REPRODUCER, "replay.txt"), BIYOS_OK);
    ASSERT_EQ(biyos_get_string_option(env.get(), BIYOS_OPT_REPRODUCER, &path), BIYOS_OK);
    ASSERT_EQ(biyos_get_string_option(env.get(), BIYOS_OPT_CHECK_LEVEL, &level), BIYOS_OK);
    EXPECT_EQ(biyos_set_string_option(env.get(), BIYOS_OPT_REPRODUCER, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_set_double_option(env.get(), BIYOS_OPT_REPRODUCER, 1.0),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_set_string_option(env.get(), BIYOS_OPT_CHECK_LEVEL, "most"),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_STREQ(path, "replay.txt");
    EXPECT_STREQ(level, "cheap");
}

TEST(CApiTest, Enum)
{
    const Env env;
    const char *value = nullptr;
    EXPECT_EQ(biyos_get_string_option(env.get(), BIYOS_OPT_CHECK_LEVEL, &value), BIYOS_OK);
    EXPECT_STREQ(value, "cheap");
    EXPECT_EQ(biyos_set_string_option(env.get(), BIYOS_OPT_CHECK_LEVEL, "full"), BIYOS_OK);
    EXPECT_EQ(biyos_get_string_option(env.get(), BIYOS_OPT_CHECK_LEVEL, &value), BIYOS_OK);
    EXPECT_STREQ(value, "full");
}

TEST(CApiTest, BadSet)
{
    const Env env;
    int value = -1;
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(biyos_get_int_option(env.get(), BIYOS_OPT_SEED, &value), BIYOS_OK);
    EXPECT_EQ(value, 0);
}

TEST(CApiTest, BadEnum)
{
    const Env env;
    EXPECT_EQ(biyos_set_string_option(env.get(), BIYOS_OPT_CHECK_LEVEL, "most"),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(),
              "invalid argument: option 'check_level': 'most' is not one of none, cheap, full");
}

TEST(CApiTest, WrongKind)
{
    const Env env;
    EXPECT_EQ(biyos_set_double_option(env.get(), BIYOS_OPT_SEED, 1.0), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': is an int, not a double");
}

TEST(CApiTest, KeptMessage)
{
    const Env env;
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, 1), BIYOS_OK);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
}

TEST(CApiTest, NullArguments)
{
    const Env env;
    int id = 0;
    EXPECT_EQ(biyos_option_id(env.get(), nullptr, &id), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
    EXPECT_EQ(biyos_option_id(env.get(), "seed", nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'option': is null");
    EXPECT_EQ(biyos_option_kind(env.get(), BIYOS_OPT_SEED, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'kind': is null");
    EXPECT_EQ(biyos_set_string_option(env.get(), BIYOS_OPT_REPRODUCER, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'value': is null");
    EXPECT_EQ(biyos_get_int_option(env.get(), BIYOS_OPT_SEED, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'value': is null");
}

TEST(CApiTest, NullEnvironment)
{
    int value = 0;
    EXPECT_EQ(biyos_option_id(nullptr, "seed", &value), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_option_kind(nullptr, BIYOS_OPT_SEED, &value), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_set_int_option(nullptr, BIYOS_OPT_SEED, 1), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_get_int_option(nullptr, BIYOS_OPT_SEED, &value), BIYOS_INVALID_ARGUMENT);
}
