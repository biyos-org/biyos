#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "core/clock.hpp"
#include "core/env.hpp"
#include "core/options.hpp"
#include "core/version.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace
{

constexpr BIYOS_int kFlags = 1;
constexpr BIYOS_mask kDual = BIYOS_mask{1} << 0;
constexpr BIYOS_mask kProbe = BIYOS_mask{1} << 1;
constexpr BIYOS_mask kScale = BIYOS_mask{1} << 2;

constexpr std::array kParts{
    biyos::FlagPart{
        .bit = kDual,
        .name = "dual",
        .default_value = biyos::PartDefault::On,
    },
    biyos::FlagPart{
        .bit = kProbe,
        .name = "probe",
        .default_value = biyos::PartDefault::Off,
    },
    biyos::FlagPart{
        .bit = kScale,
        .name = "scale",
        .default_value = biyos::PartDefault::Automatic,
    },
};

constexpr std::array kRows{
    biyos::OptionSpec{
        .id = biyos::Option{kFlags},
        .name = "parts",
        .scope = biyos::Scope::Environment,
        .rule = biyos::FlagsRule{.parts = kParts},
        .doc = "Parts.",
        .versions = {.since = biyos::Version{0, 0, 1}},
    },
};

class Env
{
  public:
    Env() { EXPECT_EQ(BIYOS_newenv(&pointer_), BIYOS_OK); }
    ~Env() { BIYOS_freeenv(pointer_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    BIYOS_env *get() const { return pointer_; }
    std::string message() const { return biyos::test::last_message(pointer_); }

  private:
    BIYOS_env *pointer_ = nullptr;
};

class Flagged
{
  public:
    BIYOS_env *get() { return &env_; }
    std::string message() const { return biyos::test::last_message(&env_); }

  private:
    BIYOS_env env_{.core = biyos::Env(std::make_unique<biyos::FakeClock>(), kRows)};
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
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), 99, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 99: unknown");
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), 0, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 0: unknown");
    EXPECT_EQ(BIYOS_getoptknd(env.get(), -1, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option -1: unknown");
}

TEST(CApiTest, Bool)
{
    const Env env;
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_getenvboolopt(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 1);
    EXPECT_EQ(BIYOS_setenvboolopt(env.get(), BIYOS_OPT_PRESOLVE, 0), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvboolopt(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 0);
}

TEST(CApiTest, NonzeroBool)
{
    const Env env;
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_setenvboolopt(env.get(), BIYOS_OPT_PRESOLVE, 0), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvboolopt(env.get(), BIYOS_OPT_PRESOLVE, 2), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvboolopt(env.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 1);
}

TEST(CApiTest, Int)
{
    const Env env;
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 42), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_SEED, &value), BIYOS_OK);
    EXPECT_EQ(value, 42);
}

TEST(CApiTest, Double)
{
    const Env env;
    BIYOS_real value = -1.0;
    EXPECT_EQ(BIYOS_setenvdblopt(env.get(), BIYOS_OPT_MAXTIME, 3.5), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvdblopt(env.get(), BIYOS_OPT_MAXTIME, &value), BIYOS_OK);
    EXPECT_DOUBLE_EQ(value, 3.5);
}

TEST(CApiTest, String)
{
    const Env env;
    const char *value = nullptr;
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_REPRO, "replay.txt"), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvstropt(env.get(), BIYOS_OPT_REPRO, &value), BIYOS_OK);
    EXPECT_STREQ(value, "replay.txt");
}

TEST(CApiTest, StringLifetime)
{
    const Env env;
    const char *path = nullptr;
    ASSERT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_REPRO, "replay.txt"), BIYOS_OK);
    ASSERT_EQ(BIYOS_getenvstropt(env.get(), BIYOS_OPT_REPRO, &path), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_REPRO, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvdblopt(env.get(), BIYOS_OPT_REPRO, 1.0), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_CHECKLVL, "full"), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_STREQ(path, "replay.txt");
}

TEST(CApiTest, Enum)
{
    const Env env;
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_CHECKLVL, &value), BIYOS_OK);
    EXPECT_EQ(value, BIYOS_CHECKLVL_CHEAP);
    for (const BIYOS_int level : {BIYOS_CHECKLVL_FULL, BIYOS_CHECKLVL_NONE, BIYOS_CHECKLVL_CHEAP})
    {
        EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_CHECKLVL, level), BIYOS_OK);
        EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_CHECKLVL, &value), BIYOS_OK);
        EXPECT_EQ(value, level);
    }
}

TEST(CApiTest, BadSet)
{
    const Env env;
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_SEED, &value), BIYOS_OK);
    EXPECT_EQ(value, 0);
}

TEST(CApiTest, BadEnum)
{
    const Env env;
    const std::string option = "invalid argument: option 'check_lvl': ";
    const std::string list = " is not one of 1 (none), 2 (cheap), 3 (full)";
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_CHECKLVL, BIYOS_CHECKLVL_FULL), BIYOS_OK);
    for (const BIYOS_int number : {4, 0, -1})
    {
        EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_CHECKLVL, number), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), option + std::to_string(number) + list);
    }
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_CHECKLVL, &value), BIYOS_OK);
    EXPECT_EQ(value, BIYOS_CHECKLVL_FULL);
}

TEST(CApiTest, EnumKind)
{
    const Env env;
    const char *text = nullptr;
    const std::string refused = "invalid argument: option 'check_lvl': is an enum, not a string";
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_CHECKLVL, "full"), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_getenvstropt(env.get(), BIYOS_OPT_CHECKLVL, &text), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(text, nullptr);
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_CHECKLVL, &value), BIYOS_OK);
    EXPECT_EQ(value, BIYOS_CHECKLVL_CHEAP);
}

TEST(CApiTest, WrongKind)
{
    const Env env;
    EXPECT_EQ(BIYOS_setenvdblopt(env.get(), BIYOS_OPT_SEED, 1.0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': is an int, not a double");
}

TEST(CApiTest, KeptMessage)
{
    const Env env;
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 1), BIYOS_OK);
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
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_REPRO, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'val': is null");
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_SEED, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'val': is null");
}

TEST(CApiTest, NullEnv)
{
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_getoptid(nullptr, "seed", &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getoptknd(nullptr, BIYOS_OPT_SEED, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvintopt(nullptr, BIYOS_OPT_SEED, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getenvintopt(nullptr, BIYOS_OPT_SEED, &value), BIYOS_ERR_BADARG);
}

TEST(CApiTest, Flags)
{
    Flagged env;
    BIYOS_mask on = kDual;
    BIYOS_mask off = kDual;
    EXPECT_EQ(BIYOS_getenvflgopt(env.get(), kFlags, &on, &off), BIYOS_OK);
    EXPECT_EQ(on, 0U);
    EXPECT_EQ(off, 0U);
    EXPECT_EQ(BIYOS_setenvflgopt(env.get(), kFlags, kDual | kScale, kProbe), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvflgopt(env.get(), kFlags, &on, &off), BIYOS_OK);
    EXPECT_EQ(on, kDual | kScale);
    EXPECT_EQ(off, kProbe);
    EXPECT_EQ(BIYOS_setenvflgopt(env.get(), kFlags, 0, kDual), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvflgopt(env.get(), kFlags, &on, &off), BIYOS_OK);
    EXPECT_EQ(on, 0U);
    EXPECT_EQ(off, kDual);
}

TEST(CApiTest, BadFlags)
{
    Flagged env;
    const std::string option = "invalid argument: option 'parts': ";
    const std::string parts = " outside its parts 0 (dual), 1 (probe), 2 (scale)";
    const BIYOS_mask last = BIYOS_mask{1} << 63;
    EXPECT_EQ(BIYOS_setenvflgopt(env.get(), kFlags, kDual, kProbe), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvflgopt(env.get(), kFlags, BIYOS_mask{1} << 3, 0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), option + "sets bit 3" + parts);
    EXPECT_EQ(BIYOS_setenvflgopt(env.get(), kFlags, 0, last), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), option + "sets bit 63" + parts);
    EXPECT_EQ(BIYOS_setenvflgopt(env.get(), kFlags, kScale, kScale), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), option + "sets bit 2 both on and off");
    BIYOS_mask on = 0;
    BIYOS_mask off = 0;
    EXPECT_EQ(BIYOS_getenvflgopt(env.get(), kFlags, &on, &off), BIYOS_OK);
    EXPECT_EQ(on, kDual);
    EXPECT_EQ(off, kProbe);
}

TEST(CApiTest, FlagsKind)
{
    Flagged env;
    const std::string refused = "invalid argument: option 'parts': is flags, not ";
    BIYOS_int value = 0;
    BIYOS_real real = 0.0;
    const char *text = nullptr;
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), kFlags, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused + "an int");
    EXPECT_EQ(BIYOS_setenvboolopt(env.get(), kFlags, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused + "a bool");
    EXPECT_EQ(BIYOS_setenvdblopt(env.get(), kFlags, 1.0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused + "a double");
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), kFlags, "dual"), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused + "a string");
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), kFlags, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused + "an int");
    EXPECT_EQ(BIYOS_getenvboolopt(env.get(), kFlags, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused + "a bool");
    EXPECT_EQ(BIYOS_getenvdblopt(env.get(), kFlags, &real), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused + "a double");
    EXPECT_EQ(BIYOS_getenvstropt(env.get(), kFlags, &text), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused + "a string");
}

TEST(CApiTest, OtherKinds)
{
    const Env env;
    const int ids[] = {BIYOS_OPT_SEED,
                       BIYOS_OPT_MAXTIME,
                       BIYOS_OPT_CHECKLVL,
                       BIYOS_OPT_PRESOLVE,
                       BIYOS_OPT_REPRO};
    const char *names[] = {"seed", "maxtime", "check_lvl", "presolve", "repro"};
    const char *nouns[] = {"an int", "a double", "an enum", "a bool", "a string"};
    for (int i = 0; i < 5; ++i)
    {
        const std::string option = "invalid argument: option '" + std::string(names[i]) + "': ";
        const std::string refused = option + "is " + nouns[i] + ", not flags";
        BIYOS_mask on = kDual;
        BIYOS_mask off = kProbe;
        EXPECT_EQ(BIYOS_setenvflgopt(env.get(), ids[i], 0, 0), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused);
        EXPECT_EQ(BIYOS_getenvflgopt(env.get(), ids[i], &on, &off), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused);
        EXPECT_EQ(on, kDual);
        EXPECT_EQ(off, kProbe);
    }
}

TEST(CApiTest, NullFlags)
{
    Flagged env;
    BIYOS_mask mask = kScale;
    EXPECT_EQ(BIYOS_getenvflgopt(env.get(), kFlags, nullptr, &mask), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'on': is null");
    EXPECT_EQ(BIYOS_getenvflgopt(env.get(), kFlags, &mask, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': is null");
    EXPECT_EQ(mask, kScale);
    EXPECT_EQ(BIYOS_setenvflgopt(nullptr, kFlags, 0, 0), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getenvflgopt(nullptr, kFlags, &mask, &mask), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvflgopt(env.get(), 99, 0, 0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 99: unknown");
}
