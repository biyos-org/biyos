#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <string>

namespace
{

class Env
{
  public:
    Env() { EXPECT_EQ(BIYOS_newenv(&ptr_), BIYOS_OK); }
    ~Env() { BIYOS_freeenv(ptr_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    BIYOS_env *get() const { return ptr_; }
    std::string message() const { return BIYOS_lasterror(ptr_); }

  private:
    BIYOS_env *ptr_ = nullptr;
};

}

TEST(CApiTest, OptionIds)
{
    const Env env;
    const char *names[] = {"seed",
                           "maxtime",
                           "check_lvl",
                           "presolve",
                           "repro",
                           "feastol_abs",
                           "feastol_rel",
                           "inttol",
                           "zerotol",
                           "maxcputime"};
    const int ids[] = {BIYOS_OPT_SEED,
                       BIYOS_OPT_MAXTIME,
                       BIYOS_OPT_CHECKLVL,
                       BIYOS_OPT_PRESOLVE,
                       BIYOS_OPT_REPRO,
                       BIYOS_OPT_FEASTOLABS,
                       BIYOS_OPT_FEASTOLREL,
                       BIYOS_OPT_INTTOL,
                       BIYOS_OPT_ZEROTOL,
                       BIYOS_OPT_MAXCPUTIME};
    for (int i = 0; i < 10; ++i)
    {
        BIYOS_int id = 0;
        EXPECT_EQ(BIYOS_getoptid(env.get(), names[i], &id), BIYOS_OK);
        EXPECT_EQ(id, ids[i]) << names[i];
    }
}

TEST(CApiTest, OptionKinds)
{
    const Env env;
    const int ids[] = {BIYOS_OPT_SEED,
                       BIYOS_OPT_MAXTIME,
                       BIYOS_OPT_CHECKLVL,
                       BIYOS_OPT_PRESOLVE,
                       BIYOS_OPT_REPRO,
                       BIYOS_OPT_FEASTOLABS,
                       BIYOS_OPT_FEASTOLREL,
                       BIYOS_OPT_INTTOL,
                       BIYOS_OPT_ZEROTOL,
                       BIYOS_OPT_MAXCPUTIME};
    const int kinds[] = {BIYOS_KND_INT,
                         BIYOS_KND_DBL,
                         BIYOS_KND_ENUM,
                         BIYOS_KND_BOOL,
                         BIYOS_KND_STR,
                         BIYOS_KND_DBL,
                         BIYOS_KND_DBL,
                         BIYOS_KND_DBL,
                         BIYOS_KND_DBL,
                         BIYOS_KND_DBL};
    for (int i = 0; i < 10; ++i)
    {
        BIYOS_int kind = 0;
        EXPECT_EQ(BIYOS_getoptknd(env.get(), ids[i], &kind), BIYOS_OK);
        EXPECT_EQ(kind, kinds[i]) << ids[i];
    }
}

TEST(CApiTest, OptionVersions)
{
    const Env env;
    for (int id = BIYOS_OPT_SEED; id <= BIYOS_OPT_MAXCPUTIME; ++id)
    {
        BIYOS_int since = -1;
        BIYOS_int deprecated = -1;
        BIYOS_int retired = -1;
        EXPECT_EQ(BIYOS_getoptver(env.get(), id, &since, &deprecated, &retired), BIYOS_OK);
        EXPECT_EQ(since, 100) << id;
        EXPECT_EQ(deprecated, 0) << id;
        EXPECT_EQ(retired, 0) << id;
    }
}

TEST(CApiTest, VersionArguments)
{
    const Env env;
    BIYOS_int since = -1;
    BIYOS_int deprecated = -1;
    BIYOS_int retired = -1;
    EXPECT_EQ(BIYOS_getoptver(env.get(), 11, &since, &deprecated, &retired), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 11: unknown");
    EXPECT_EQ(BIYOS_getoptver(env.get(), BIYOS_OPT_SEED, nullptr, &deprecated, &retired),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'since': is null");
    EXPECT_EQ(BIYOS_getoptver(env.get(), BIYOS_OPT_SEED, &since, nullptr, &retired),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'deprecated': is null");
    EXPECT_EQ(BIYOS_getoptver(env.get(), BIYOS_OPT_SEED, &since, &deprecated, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'retired': is null");
    EXPECT_EQ(BIYOS_getoptver(nullptr, BIYOS_OPT_SEED, &since, &deprecated, &retired),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(since, -1);
    EXPECT_EQ(deprecated, -1);
    EXPECT_EQ(retired, -1);
}

TEST(CApiTest, UnknownName)
{
    const Env env;
    BIYOS_int id = 0;
    EXPECT_EQ(BIYOS_getoptid(env.get(), "seeds", &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'seeds': unknown");
}

TEST(CApiTest, UnknownId)
{
    const Env env;
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_setintopt(env.get(), 99, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 99: unknown");
    EXPECT_EQ(BIYOS_getintopt(env.get(), 0, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 0: unknown");
    EXPECT_EQ(BIYOS_getoptknd(env.get(), -1, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option -1: unknown");
}

TEST(CApiTest, Bool)
{
    const Env env;
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_getboolopt(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 1);
    EXPECT_EQ(BIYOS_setboolopt(env.get(), BIYOS_OPT_PRESOLVE, 0), BIYOS_OK);
    EXPECT_EQ(BIYOS_getboolopt(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 0);
}

TEST(CApiTest, NonzeroBool)
{
    const Env env;
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_setboolopt(env.get(), BIYOS_OPT_PRESOLVE, 0), BIYOS_OK);
    EXPECT_EQ(BIYOS_setboolopt(env.get(), BIYOS_OPT_PRESOLVE, 2), BIYOS_OK);
    EXPECT_EQ(BIYOS_getboolopt(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 1);
}

TEST(CApiTest, Int)
{
    const Env env;
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, 42), BIYOS_OK);
    EXPECT_EQ(BIYOS_getintopt(env.get(), BIYOS_OPT_SEED, &value), BIYOS_OK);
    EXPECT_EQ(value, 42);
}

TEST(CApiTest, Double)
{
    const Env env;
    BIYOS_real value = -1.0;
    EXPECT_EQ(BIYOS_setdblopt(env.get(), BIYOS_OPT_MAXTIME, 3.5), BIYOS_OK);
    EXPECT_EQ(BIYOS_getdblopt(env.get(), BIYOS_OPT_MAXTIME, &value), BIYOS_OK);
    EXPECT_DOUBLE_EQ(value, 3.5);
}

TEST(CApiTest, String)
{
    const Env env;
    const char *value = nullptr;
    EXPECT_EQ(BIYOS_setstropt(env.get(), BIYOS_OPT_REPRO, "replay.txt"), BIYOS_OK);
    EXPECT_EQ(BIYOS_getstropt(env.get(), BIYOS_OPT_REPRO, &value), BIYOS_OK);
    EXPECT_STREQ(value, "replay.txt");
}

TEST(CApiTest, StringLifetime)
{
    const Env env;
    const char *path = nullptr;
    const char *level = nullptr;
    ASSERT_EQ(BIYOS_setstropt(env.get(), BIYOS_OPT_REPRO, "replay.txt"), BIYOS_OK);
    ASSERT_EQ(BIYOS_getstropt(env.get(), BIYOS_OPT_REPRO, &path), BIYOS_OK);
    ASSERT_EQ(BIYOS_getstropt(env.get(), BIYOS_OPT_CHECKLVL, &level), BIYOS_OK);
    EXPECT_EQ(BIYOS_setstropt(env.get(), BIYOS_OPT_REPRO, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setdblopt(env.get(), BIYOS_OPT_REPRO, 1.0), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setstropt(env.get(), BIYOS_OPT_CHECKLVL, "most"), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_STREQ(path, "replay.txt");
    EXPECT_STREQ(level, "cheap");
}

TEST(CApiTest, Enum)
{
    const Env env;
    const char *value = nullptr;
    EXPECT_EQ(BIYOS_getstropt(env.get(), BIYOS_OPT_CHECKLVL, &value), BIYOS_OK);
    EXPECT_STREQ(value, "cheap");
    EXPECT_EQ(BIYOS_setstropt(env.get(), BIYOS_OPT_CHECKLVL, "full"), BIYOS_OK);
    EXPECT_EQ(BIYOS_getstropt(env.get(), BIYOS_OPT_CHECKLVL, &value), BIYOS_OK);
    EXPECT_STREQ(value, "full");
}

TEST(CApiTest, BadSet)
{
    const Env env;
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(BIYOS_getintopt(env.get(), BIYOS_OPT_SEED, &value), BIYOS_OK);
    EXPECT_EQ(value, 0);
}

TEST(CApiTest, BadEnum)
{
    const Env env;
    EXPECT_EQ(BIYOS_setstropt(env.get(), BIYOS_OPT_CHECKLVL, "most"), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: option 'check_lvl': 'most' is not one of none, cheap, full");
}

TEST(CApiTest, WrongKind)
{
    const Env env;
    EXPECT_EQ(BIYOS_setdblopt(env.get(), BIYOS_OPT_SEED, 1.0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': is an int, not a double");
}

TEST(CApiTest, KeptMessage)
{
    const Env env;
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, 1), BIYOS_OK);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
}

TEST(CApiTest, NullArguments)
{
    const Env env;
    BIYOS_int id = 0;
    EXPECT_EQ(BIYOS_getoptid(env.get(), nullptr, &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
    EXPECT_EQ(BIYOS_getoptid(env.get(), "seed", nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'opt': is null");
    EXPECT_EQ(BIYOS_getoptknd(env.get(), BIYOS_OPT_SEED, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'knd': is null");
    EXPECT_EQ(BIYOS_setstropt(env.get(), BIYOS_OPT_REPRO, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'val': is null");
    EXPECT_EQ(BIYOS_getintopt(env.get(), BIYOS_OPT_SEED, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'val': is null");
}

TEST(CApiTest, NullEnv)
{
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_getoptid(nullptr, "seed", &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getoptknd(nullptr, BIYOS_OPT_SEED, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setintopt(nullptr, BIYOS_OPT_SEED, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getintopt(nullptr, BIYOS_OPT_SEED, &value), BIYOS_ERR_BADARG);
}
