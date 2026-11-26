#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "core/options/options.hpp"
#include "core/timing/clock.hpp"
#include "core/version.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using biyos::FakeClock;
using biyos::FlagPart;
using biyos::FlagsRule;
using biyos::Option;
using biyos::OptionSpec;
using biyos::PartDefault;
using biyos::Scope;
using biyos::Version;
using biyos::test::last_message;

namespace
{

constexpr BIYOS_int kFlags = 1;
constexpr BIYOS_int kMasks = 2;
constexpr BIYOS_int kMask = 3;
constexpr BIYOS_mask kDual = BIYOS_mask{1} << 0;
constexpr BIYOS_mask kProbe = BIYOS_mask{1} << 1;
constexpr BIYOS_mask kScale = BIYOS_mask{1} << 2;

constexpr std::array kParts{
    FlagPart{
        .bit = kDual,
        .name = "dual",
        .default_value = PartDefault::On,
    },
    FlagPart{
        .bit = kProbe,
        .name = "probe",
        .default_value = PartDefault::Off,
    },
    FlagPart{
        .bit = kScale,
        .name = "scale",
        .default_value = PartDefault::Automatic,
    },
};

constexpr std::array kRows{
    OptionSpec{
        .id = Option{kFlags},
        .name = "parts",
        .scope = Scope::Environment,
        .rule = FlagsRule{.parts = kParts},
        .doc = "Parts.",
        .versions = {.since = Version{0, 0, 1}},
    },
    OptionSpec{
        .id = Option{kMasks},
        .name = "masks",
        .scope = Scope::Model,
        .rule = FlagsRule{.parts = kParts},
        .doc = "Masks.",
        .versions = {.since = Version{0, 0, 1}},
    },
    OptionSpec{
        .id = Option{kMask},
        .name = "mask",
        .scope = Scope::Model,
        .rule = FlagsRule{.parts = kParts},
        .doc = "Mask.",
        .versions = {.since = Version{0, 0, 1}, .deprecated = Version{0, 0, 2}},
        .replacement = Option{kMasks},
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
    std::string message() const { return last_message(pointer_); }

  private:
    BIYOS_env *pointer_ = nullptr;
};

class Flagged
{
  public:
    BIYOS_env *get() { return &env_; }
    std::string message() const { return last_message(&env_); }

  private:
    BIYOS_env env_{.core = biyos::Env(std::make_unique<FakeClock>(), kRows)};
};

class Model
{
  public:
    explicit Model(BIYOS_env *env) { EXPECT_EQ(BIYOS_newmdl(env, &pointer_), BIYOS_OK); }
    Model(BIYOS_env *env, const Model &source)
    {
        EXPECT_EQ(BIYOS_copymdl(source.get(), env, &pointer_), BIYOS_OK);
    }
    ~Model() { BIYOS_freemdl(pointer_); }
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;

    BIYOS_model *get() const { return pointer_; }

  private:
    BIYOS_model *pointer_ = nullptr;
};

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
};

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(last_message(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    BIYOS_int value = 0;
    note(attempt, BIYOS_setmdlboolopt(model, BIYOS_OPT_PRESOLVE, 0));
    note(attempt, BIYOS_setmdlintopt(model, BIYOS_OPT_SEED, 1));
    note(attempt, BIYOS_setmdldblopt(model, BIYOS_OPT_MAXTIME, 1.0));
    note(attempt, BIYOS_setmdlstropt(model, BIYOS_OPT_REPRO, "hook.txt"));
    note(attempt, BIYOS_setmdlflgopt(model, BIYOS_OPT_SEED, 0, 0));
    note(attempt, BIYOS_delmdlopt(model, BIYOS_OPT_SEED));
    note(attempt, BIYOS_getmdlintopt(model, BIYOS_OPT_SEED, &value));
    note(attempt, BIYOS_getmdloptsrc(model, BIYOS_OPT_SEED, &value));
}

struct Heard
{
    const BIYOS_model *model = nullptr;
    int count = 0;
};

void hear(const BIYOS_event *event, void *data)
{
    Heard &heard = *static_cast<Heard *>(data);
    heard.model = BIYOS_getevtmdl(event);
    ++heard.count;
}

BIYOS_int model_int(const Model &model, BIYOS_int option)
{
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_getmdlintopt(model.get(), option, &value), BIYOS_OK);
    return value;
}

BIYOS_int source_of(const Model &model, BIYOS_int option)
{
    BIYOS_int source = 0;
    EXPECT_EQ(BIYOS_getmdloptsrc(model.get(), option, &source), BIYOS_OK);
    return source;
}

}

TEST(CApiTest, OptionIds)
{
    const Env env;
    const char *names[] = {"seed",
                           "maxtime",
                           "checking",
                           "presolve",
                           "repro",
                           "feastol_abs",
                           "feastol_rel",
                           "inttol",
                           "zerotol",
                           "maxcputime"};
    const int ids[] = {BIYOS_OPT_SEED,
                       BIYOS_OPT_MAXTIME,
                       BIYOS_OPT_CHK,
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
                       BIYOS_OPT_CHK,
                       BIYOS_OPT_PRESOLVE,
                       BIYOS_OPT_REPRO,
                       BIYOS_OPT_FEASTOLABS,
                       BIYOS_OPT_FEASTOLREL,
                       BIYOS_OPT_INTTOL,
                       BIYOS_OPT_ZEROTOL,
                       BIYOS_OPT_MAXCPUTIME};
    const int kinds[] = {BIYOS_OPTKND_INT,
                         BIYOS_OPTKND_DBL,
                         BIYOS_OPTKND_ENUM,
                         BIYOS_OPTKND_BOOL,
                         BIYOS_OPTKND_STR,
                         BIYOS_OPTKND_DBL,
                         BIYOS_OPTKND_DBL,
                         BIYOS_OPTKND_DBL,
                         BIYOS_OPTKND_DBL,
                         BIYOS_OPTKND_DBL};
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
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_CHK, "full"), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_STREQ(path, "replay.txt");
}

TEST(CApiTest, Enum)
{
    const Env env;
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_CHK, &value), BIYOS_OK);
    EXPECT_EQ(value, BIYOS_CHK_CHEAP);
    for (const BIYOS_int chk : {BIYOS_CHK_FULL, BIYOS_CHK_NONE, BIYOS_CHK_CHEAP})
    {
        EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_CHK, chk), BIYOS_OK);
        EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_CHK, &value), BIYOS_OK);
        EXPECT_EQ(value, chk);
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
    const std::string option = "invalid argument: option 'checking': ";
    const std::string list = " is not one of 1 (none), 2 (cheap), 3 (full)";
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_CHK, BIYOS_CHK_FULL), BIYOS_OK);
    for (const BIYOS_int number : {4, 0, -1})
    {
        EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_CHK, number), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), option + std::to_string(number) + list);
    }
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_CHK, &value), BIYOS_OK);
    EXPECT_EQ(value, BIYOS_CHK_FULL);
}

TEST(CApiTest, EnumKind)
{
    const Env env;
    const char *text = nullptr;
    const std::string refused = "invalid argument: option 'checking': is an enum, not a string";
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_CHK, "full"), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_getenvstropt(env.get(), BIYOS_OPT_CHK, &text), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(text, nullptr);
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_CHK, &value), BIYOS_OK);
    EXPECT_EQ(value, BIYOS_CHK_CHEAP);
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
                       BIYOS_OPT_CHK,
                       BIYOS_OPT_PRESOLVE,
                       BIYOS_OPT_REPRO};
    const char *names[] = {"seed", "maxtime", "checking", "presolve", "repro"};
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

TEST(CApiTest, ModelBool)
{
    const Env env;
    const Model model(env.get());
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_getmdlboolopt(model.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 1);
    EXPECT_EQ(BIYOS_setmdlboolopt(model.get(), BIYOS_OPT_PRESOLVE, 0), BIYOS_OK);
    EXPECT_EQ(BIYOS_getmdlboolopt(model.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 0);
    EXPECT_EQ(BIYOS_setmdlboolopt(model.get(), BIYOS_OPT_PRESOLVE, 2), BIYOS_OK);
    EXPECT_EQ(BIYOS_getmdlboolopt(model.get(), BIYOS_OPT_PRESOLVE, &value), BIYOS_OK);
    EXPECT_EQ(value, 1);
}

TEST(CApiTest, ModelInt)
{
    const Env env;
    const Model model(env.get());
    BIYOS_int value = -1;
    EXPECT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_SEED, 42), BIYOS_OK);
    EXPECT_EQ(model_int(model, BIYOS_OPT_SEED), 42);
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_SEED, &value), BIYOS_OK);
    EXPECT_EQ(value, 0);
}

TEST(CApiTest, ModelEnum)
{
    const Env env;
    const Model model(env.get());
    EXPECT_EQ(model_int(model, BIYOS_OPT_CHK), BIYOS_CHK_CHEAP);
    EXPECT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_CHK, BIYOS_CHK_FULL), BIYOS_OK);
    EXPECT_EQ(model_int(model, BIYOS_OPT_CHK), BIYOS_CHK_FULL);
    EXPECT_EQ(BIYOS_setmdlstropt(model.get(), BIYOS_OPT_CHK, "full"), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'checking': is an enum, not a string");
}

TEST(CApiTest, ModelDouble)
{
    const Env env;
    const Model model(env.get());
    BIYOS_real value = -1.0;
    EXPECT_EQ(BIYOS_setmdldblopt(model.get(), BIYOS_OPT_MAXTIME, 3.5), BIYOS_OK);
    EXPECT_EQ(BIYOS_getmdldblopt(model.get(), BIYOS_OPT_MAXTIME, &value), BIYOS_OK);
    EXPECT_DOUBLE_EQ(value, 3.5);
}

TEST(CApiTest, ModelString)
{
    const Env env;
    const Model model(env.get());
    const char *value = nullptr;
    EXPECT_EQ(BIYOS_setmdlstropt(model.get(), BIYOS_OPT_REPRO, "model.txt"), BIYOS_OK);
    EXPECT_EQ(BIYOS_getmdlstropt(model.get(), BIYOS_OPT_REPRO, &value), BIYOS_OK);
    EXPECT_STREQ(value, "model.txt");
    EXPECT_EQ(BIYOS_getenvstropt(env.get(), BIYOS_OPT_REPRO, &value), BIYOS_OK);
    EXPECT_STREQ(value, "");
}

TEST(CApiTest, ModelFlags)
{
    Flagged env;
    const Model model(env.get());
    BIYOS_mask on = kDual;
    BIYOS_mask off = kDual;
    ASSERT_EQ(BIYOS_setenvflgopt(env.get(), kMasks, kDual, 0), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlflgopt(model.get(), kMasks, kScale, kProbe), BIYOS_OK);
    EXPECT_EQ(BIYOS_getmdlflgopt(model.get(), kMasks, &on, &off), BIYOS_OK);
    EXPECT_EQ(on, kScale);
    EXPECT_EQ(off, kProbe);
    EXPECT_EQ(BIYOS_getenvflgopt(env.get(), kMasks, &on, &off), BIYOS_OK);
    EXPECT_EQ(on, kDual);
    EXPECT_EQ(off, 0U);
    EXPECT_EQ(BIYOS_setmdlflgopt(model.get(), kMasks, kScale, kScale), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'masks': sets bit 2 both on and off");
    EXPECT_EQ(BIYOS_getmdlflgopt(model.get(), kMasks, nullptr, &off), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'on': is null");
    EXPECT_EQ(BIYOS_getmdlflgopt(model.get(), kMasks, &on, &off), BIYOS_OK);
    EXPECT_EQ(on, kScale);
    EXPECT_EQ(off, kProbe);
}

TEST(CApiTest, OptionSources)
{
    const Env env;
    const Model model(env.get());
    EXPECT_EQ(source_of(model, BIYOS_OPT_SEED), BIYOS_OPTSRC_DEFAULT);
    ASSERT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 0), BIYOS_OK);
    EXPECT_EQ(source_of(model, BIYOS_OPT_SEED), BIYOS_OPTSRC_ENV);
    ASSERT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_SEED, 0), BIYOS_OK);
    EXPECT_EQ(source_of(model, BIYOS_OPT_SEED), BIYOS_OPTSRC_MDL);
    EXPECT_EQ(source_of(model, BIYOS_OPT_MAXTIME), BIYOS_OPTSRC_DEFAULT);
}

TEST(CApiTest, DeletedOption)
{
    const Env env;
    const Model model(env.get());
    ASSERT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 3), BIYOS_OK);
    ASSERT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_SEED, 7), BIYOS_OK);
    EXPECT_EQ(model_int(model, BIYOS_OPT_SEED), 7);
    EXPECT_EQ(BIYOS_delmdlopt(model.get(), BIYOS_OPT_SEED), BIYOS_OK);
    EXPECT_EQ(model_int(model, BIYOS_OPT_SEED), 3);
    EXPECT_EQ(source_of(model, BIYOS_OPT_SEED), BIYOS_OPTSRC_ENV);
    EXPECT_EQ(BIYOS_delmdlopt(model.get(), BIYOS_OPT_SEED), BIYOS_OK);
    EXPECT_EQ(BIYOS_delmdlopt(model.get(), BIYOS_OPT_MAXTIME), BIYOS_OK);
    EXPECT_EQ(model_int(model, BIYOS_OPT_SEED), 3);
    EXPECT_EQ(source_of(model, BIYOS_OPT_MAXTIME), BIYOS_OPTSRC_DEFAULT);
    EXPECT_EQ(BIYOS_delmdlopt(model.get(), 0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 0: unknown");
}

TEST(CApiTest, DroppedOption)
{
    const Env env;
    const Model shared(env.get());
    const Model own(env.get());
    ASSERT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 3), BIYOS_OK);
    ASSERT_EQ(BIYOS_setmdlintopt(own.get(), BIYOS_OPT_SEED, 7), BIYOS_OK);
    EXPECT_EQ(BIYOS_delenvopt(env.get(), BIYOS_OPT_SEED), BIYOS_OK);
    EXPECT_EQ(model_int(shared, BIYOS_OPT_SEED), 0);
    EXPECT_EQ(source_of(shared, BIYOS_OPT_SEED), BIYOS_OPTSRC_DEFAULT);
    EXPECT_EQ(model_int(own, BIYOS_OPT_SEED), 7);
    EXPECT_EQ(source_of(own, BIYOS_OPT_SEED), BIYOS_OPTSRC_MDL);
    EXPECT_EQ(BIYOS_delenvopt(env.get(), BIYOS_OPT_SEED), BIYOS_OK);
    EXPECT_EQ(BIYOS_delenvopt(env.get(), BIYOS_OPT_MAXTIME), BIYOS_OK);
    EXPECT_EQ(source_of(shared, BIYOS_OPT_MAXTIME), BIYOS_OPTSRC_DEFAULT);
    EXPECT_EQ(BIYOS_delenvopt(env.get(), 0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 0: unknown");
    EXPECT_EQ(BIYOS_delenvopt(nullptr, BIYOS_OPT_SEED), BIYOS_ERR_BADARG);
}

TEST(CApiTest, SharedOption)
{
    const Env env;
    const Model shared(env.get());
    const Model own(env.get());
    ASSERT_EQ(BIYOS_setmdlintopt(own.get(), BIYOS_OPT_SEED, 7), BIYOS_OK);
    ASSERT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 9), BIYOS_OK);
    EXPECT_EQ(model_int(shared, BIYOS_OPT_SEED), 9);
    EXPECT_EQ(model_int(own, BIYOS_OPT_SEED), 7);
}

TEST(CApiTest, RefusedOption)
{
    const Env env;
    const Model model(env.get());
    EXPECT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(source_of(model, BIYOS_OPT_SEED), BIYOS_OPTSRC_DEFAULT);
    ASSERT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdldblopt(model.get(), BIYOS_OPT_SEED, 1.0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': is an int, not a double");
    EXPECT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(model_int(model, BIYOS_OPT_SEED), 5);
    EXPECT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_CHK, 7), BIYOS_ERR_BADARG);
    EXPECT_EQ(source_of(model, BIYOS_OPT_CHK), BIYOS_OPTSRC_DEFAULT);
    EXPECT_EQ(BIYOS_setmdlintopt(model.get(), 99, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: option 99: unknown");
}

TEST(CApiTest, CopiedOptions)
{
    const Env env;
    auto source = std::make_unique<Model>(env.get());
    ASSERT_EQ(BIYOS_setmdlintopt(source->get(), BIYOS_OPT_SEED, 4), BIYOS_OK);
    ASSERT_EQ(BIYOS_setmdlstropt(source->get(), BIYOS_OPT_REPRO, "copy.txt"), BIYOS_OK);
    const Model copy(env.get(), *source);
    ASSERT_EQ(BIYOS_setmdlintopt(source->get(), BIYOS_OPT_SEED, 6), BIYOS_OK);
    source.reset();
    const char *path = nullptr;
    EXPECT_EQ(model_int(copy, BIYOS_OPT_SEED), 4);
    EXPECT_EQ(source_of(copy, BIYOS_OPT_SEED), BIYOS_OPTSRC_MDL);
    EXPECT_EQ(BIYOS_getmdlstropt(copy.get(), BIYOS_OPT_REPRO, &path), BIYOS_OK);
    EXPECT_STREQ(path, "copy.txt");
    EXPECT_EQ(source_of(copy, BIYOS_OPT_MAXTIME), BIYOS_OPTSRC_DEFAULT);
}

TEST(CApiTest, OtherOptions)
{
    const Env first;
    const Env second;
    const Model source(first.get());
    BIYOS_real limit = 0.0;
    ASSERT_EQ(BIYOS_setenvdblopt(first.get(), BIYOS_OPT_MAXTIME, 2.0), BIYOS_OK);
    ASSERT_EQ(BIYOS_setenvdblopt(second.get(), BIYOS_OPT_MAXTIME, 8.0), BIYOS_OK);
    ASSERT_EQ(BIYOS_setmdlintopt(source.get(), BIYOS_OPT_SEED, 4), BIYOS_OK);
    const Model copy(second.get(), source);
    EXPECT_EQ(model_int(copy, BIYOS_OPT_SEED), 4);
    EXPECT_EQ(BIYOS_getmdldblopt(copy.get(), BIYOS_OPT_MAXTIME, &limit), BIYOS_OK);
    EXPECT_DOUBLE_EQ(limit, 8.0);
    EXPECT_EQ(source_of(copy, BIYOS_OPT_MAXTIME), BIYOS_OPTSRC_ENV);
    ASSERT_EQ(BIYOS_setenvdblopt(second.get(), BIYOS_OPT_MAXTIME, 6.0), BIYOS_OK);
    EXPECT_EQ(BIYOS_getmdldblopt(copy.get(), BIYOS_OPT_MAXTIME, &limit), BIYOS_OK);
    EXPECT_DOUBLE_EQ(limit, 6.0);
}

TEST(CApiTest, LockedOptions)
{
    const Env env;
    const Model model(env.get());
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status>
        statuses{refused, refused, refused, refused, refused, refused, BIYOS_OK, BIYOS_OK};
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    for (std::size_t k = 0; k < 6; ++k)
    {
        EXPECT_EQ(attempt.messages[k], locked) << k;
    }
    EXPECT_EQ(source_of(model, BIYOS_OPT_SEED), BIYOS_OPTSRC_DEFAULT);
    EXPECT_EQ(source_of(model, BIYOS_OPT_REPRO), BIYOS_OPTSRC_DEFAULT);
}

TEST(CApiTest, EnvironmentScope)
{
    Flagged env;
    const Model model(env.get());
    BIYOS_model *held = model.get();
    const std::string refused = "invalid argument: option 'parts': has environment scope";
    BIYOS_int value = 0;
    BIYOS_real real = 0.0;
    const char *text = nullptr;
    BIYOS_mask on = 0;
    BIYOS_mask off = 0;
    EXPECT_EQ(BIYOS_setmdlboolopt(held, kFlags, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_setmdlintopt(held, kFlags, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_setmdldblopt(held, kFlags, 1.0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_setmdlstropt(held, kFlags, "dual"), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_setmdlflgopt(held, kFlags, kDual, 0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_getmdlboolopt(held, kFlags, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_getmdlintopt(held, kFlags, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_getmdldblopt(held, kFlags, &real), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_getmdlstropt(held, kFlags, &text), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_getmdlflgopt(held, kFlags, &on, &off), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_delmdlopt(held, kFlags), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_getmdloptsrc(held, kFlags, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused);
    EXPECT_EQ(BIYOS_setenvflgopt(env.get(), kFlags, kDual, 0), BIYOS_OK);
    EXPECT_EQ(BIYOS_delenvopt(env.get(), kFlags), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvflgopt(env.get(), kFlags, &on, &off), BIYOS_OK);
    EXPECT_EQ(on, 0U);
    EXPECT_EQ(off, 0U);
}

TEST(CApiTest, DeprecatedModel)
{
    Heard heard;
    Flagged env;
    const Model model(env.get());
    BIYOS_index id = 0;
    const BIYOS_status added =
        BIYOS_addenvhook(env.get(), hear, &heard, BIYOS_EVTKND_MSG, BIYOS_EVTLVL_WARN, &id);
    ASSERT_EQ(added, BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlflgopt(model.get(), kMask, kDual, 0), BIYOS_OK);
    EXPECT_EQ(heard.count, 1);
    EXPECT_EQ(heard.model, model.get());
}

TEST(CApiTest, KeptStrings)
{
    const Env env;
    const Model model(env.get());
    const char *shared = nullptr;
    const char *own = nullptr;
    const char *global = nullptr;
    ASSERT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_REPRO, "shared.txt"), BIYOS_OK);
    ASSERT_EQ(BIYOS_getmdlstropt(model.get(), BIYOS_OPT_REPRO, &shared), BIYOS_OK);
    ASSERT_EQ(BIYOS_getenvstropt(env.get(), BIYOS_OPT_REPRO, &global), BIYOS_OK);
    ASSERT_EQ(BIYOS_setmdlstropt(model.get(), BIYOS_OPT_REPRO, "own.txt"), BIYOS_OK);
    ASSERT_EQ(BIYOS_getmdlstropt(model.get(), BIYOS_OPT_REPRO, &own), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_REPRO, "next.txt"), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlstropt(model.get(), BIYOS_OPT_REPRO, "again.txt"), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 3), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_SEED, 4), BIYOS_OK);
    EXPECT_EQ(BIYOS_delmdlopt(model.get(), BIYOS_OPT_REPRO), BIYOS_OK);
    EXPECT_STREQ(shared, "shared.txt");
    EXPECT_STREQ(global, "shared.txt");
    EXPECT_STREQ(own, "own.txt");
}

TEST(CApiTest, EqualString)
{
    const Env env;
    const Model model(env.get());
    const std::string text = "same.txt";
    const char *first = nullptr;
    const char *second = nullptr;
    ASSERT_EQ(BIYOS_setmdlstropt(model.get(), BIYOS_OPT_REPRO, "same.txt"), BIYOS_OK);
    ASSERT_EQ(BIYOS_getmdlstropt(model.get(), BIYOS_OPT_REPRO, &first), BIYOS_OK);
    ASSERT_EQ(BIYOS_setmdlstropt(model.get(), BIYOS_OPT_REPRO, text.c_str()), BIYOS_OK);
    ASSERT_EQ(BIYOS_getmdlstropt(model.get(), BIYOS_OPT_REPRO, &second), BIYOS_OK);
    EXPECT_EQ(first, second);
    EXPECT_NE(first, text.c_str());
    ASSERT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_REPRO, "same.txt"), BIYOS_OK);
    ASSERT_EQ(BIYOS_getenvstropt(env.get(), BIYOS_OPT_REPRO, &first), BIYOS_OK);
    ASSERT_EQ(BIYOS_setenvstropt(env.get(), BIYOS_OPT_REPRO, text.c_str()), BIYOS_OK);
    ASSERT_EQ(BIYOS_getenvstropt(env.get(), BIYOS_OPT_REPRO, &second), BIYOS_OK);
    EXPECT_EQ(first, second);
    EXPECT_NE(first, text.c_str());
}

TEST(CApiTest, ModelNulls)
{
    const Env env;
    const Model model(env.get());
    BIYOS_int value = 0;
    EXPECT_EQ(BIYOS_setmdlintopt(nullptr, BIYOS_OPT_SEED, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getmdlintopt(nullptr, BIYOS_OPT_SEED, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delmdlopt(nullptr, BIYOS_OPT_SEED), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getmdloptsrc(nullptr, BIYOS_OPT_SEED, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setmdlstropt(model.get(), BIYOS_OPT_REPRO, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'val': is null");
    EXPECT_EQ(source_of(model, BIYOS_OPT_REPRO), BIYOS_OPTSRC_DEFAULT);
    EXPECT_EQ(BIYOS_getmdlintopt(model.get(), BIYOS_OPT_SEED, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'val': is null");
    EXPECT_EQ(BIYOS_getmdloptsrc(model.get(), BIYOS_OPT_SEED, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'src': is null");
}
