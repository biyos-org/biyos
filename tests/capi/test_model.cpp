#include "biyos/biyos.h"
#include "doubles.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::expect_doubles;
using biyos::test::Model;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Columns
{
    std::vector<int> kinds;
    std::vector<double> lower;
    std::vector<double> upper;
};

Columns read_all(const Model &model)
{
    int count = -1;
    EXPECT_EQ(BIYOS_getnvars(model.get(), &count), BIYOS_OK);
    const auto size = static_cast<std::size_t>(count);
    Columns read{.kinds = std::vector<int>(size),
                 .lower = std::vector<double>(size),
                 .upper = std::vector<double>(size)};
    const BIYOS_status status = BIYOS_getvars(model.get(),
                                              0,
                                              count,
                                              read.kinds.data(),
                                              read.lower.data(),
                                              read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void expect_columns(const Columns &found, const Columns &wanted)
{
    EXPECT_EQ(found.kinds, wanted.kinds);
    expect_doubles(found.lower, wanted.lower);
    expect_doubles(found.upper, wanted.upper);
}

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
    int count = -1;
};

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(BIYOS_lasterror(attempt.env));
}

void change(const BIYOS_evt *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    BIYOS_model *made = nullptr;
    const int index = 0;
    const double bound = 0.0;
    const int kind = BIYOS_VAR_INTEGER;
    note(attempt, BIYOS_newmodel(attempt.env, &made));
    note(attempt, BIYOS_freemodel(model));
    note(attempt, BIYOS_addvars(model, 1, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_setvarbounds(model, 1, &index, &bound, &bound));
    note(attempt, BIYOS_setvarkinds(model, 1, &index, &kind));
    note(attempt, BIYOS_getnvars(model, &attempt.count));
    note(attempt, BIYOS_getvars(model, 0, 1, nullptr, nullptr, nullptr));
}

void keep(const BIYOS_evt *event, void *data)
{
    static_cast<std::vector<std::string> *>(data)->emplace_back(BIYOS_getevttext(event));
}

void fail(const Env &env)
{
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
}

}

TEST(CApiTest, ModelCreate)
{
    const Env env;
    BIYOS_model *model = nullptr;
    ASSERT_EQ(BIYOS_newmodel(env.get(), &model), BIYOS_OK);
    EXPECT_NE(model, nullptr);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(BIYOS_freemodel(model), BIYOS_OK);
}

TEST(CApiTest, NullModel)
{
    const Env env;
    const Model kept(env);
    BIYOS_model *model = kept.get();
    EXPECT_EQ(BIYOS_newmodel(nullptr, &model), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(model, nullptr);
    EXPECT_EQ(BIYOS_newmodel(env.get(), nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'model': is null");
    EXPECT_EQ(BIYOS_freemodel(nullptr), BIYOS_OK);
    const int index = 0;
    const double bound = 0.0;
    int count = 0;
    EXPECT_EQ(BIYOS_addvars(nullptr, 1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_setvarbounds(nullptr, 1, &index, &bound, &bound), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_setvarkinds(nullptr, 1, &index, &index), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getnvars(nullptr, &count), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getvars(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_INVALID_ARGUMENT);
    std::int64_t needed = 0;
    EXPECT_EQ(BIYOS_addfuns(nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getnfuns(nullptr, &count), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getfuns(nullptr, 0, 0, nullptr, nullptr, 0, &needed, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_addquadterms(nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getquadterms(nullptr, 0, 0, nullptr, 0, &needed, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_addcons(nullptr, 1, &index, nullptr, nullptr, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_setconbounds(nullptr, 1, &index, &bound, &bound), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getncons(nullptr, &count), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getcons(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(
        BIYOS_addobjs(nullptr, 1, &index, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
        BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_setobjs(nullptr, 1, &index, nullptr, nullptr, &bound, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getnobjs(nullptr, &count), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_getobjs(nullptr, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
}

TEST(CApiTest, HeldFree)
{
    BIYOS_env *env = nullptr;
    BIYOS_model *first = nullptr;
    BIYOS_model *second = nullptr;
    ASSERT_EQ(BIYOS_newenv(&env), BIYOS_OK);
    ASSERT_EQ(BIYOS_newmodel(env, &first), BIYOS_OK);
    ASSERT_EQ(BIYOS_newmodel(env, &second), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_INVALID_ARGUMENT);
    EXPECT_STREQ(BIYOS_lasterror(env),
                 "invalid argument: environment: has a model that is not freed");
    EXPECT_EQ(BIYOS_freemodel(first), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(BIYOS_setintopt(env, BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_EQ(BIYOS_freemodel(second), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_OK);
}

TEST(CApiTest, LockedModel)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    ASSERT_EQ(BIYOS_addvars(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), change, &attempt, message, BIYOS_LEVEL_ERROR, &id),
              BIYOS_OK);
    fail(env);
    const BIYOS_status refused = BIYOS_INVALID_ARGUMENT;
    const std::vector<BIYOS_status>
        statuses{refused, refused, refused, refused, refused, BIYOS_OK, BIYOS_OK};
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    const std::string failure = "invalid argument: option 'seed': -1 is outside [0, 2147483647]";
    const std::vector<std::string> messages{
        "invalid argument: environment: is read-only while delivering an event",
        locked,
        locked,
        locked,
        locked,
        locked,
        locked,
    };
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, messages);
    EXPECT_EQ(attempt.count, 1);
    EXPECT_EQ(env.message(), failure);
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
}

TEST(CApiTest, BulkAdd)
{
    const Env env;
    const Model model(env);
    const Columns added{
        .kinds = {BIYOS_VAR_CONTINUOUS,
                  BIYOS_VAR_BINARY,
                  BIYOS_VAR_INTEGER,
                  BIYOS_VAR_SEMICONTINUOUS,
                  BIYOS_VAR_SEMIINTEGER},
        .lower = {-kInfinity, 0.0, -3.0, 2.0, 1.0},
        .upper = {kInfinity, 1.0, 7.0, 10.0, 4.0},
    };
    const int *kinds = added.kinds.data();
    const double *low = added.lower.data();
    const double *high = added.upper.data();
    ASSERT_EQ(BIYOS_addvars(model.get(), 5, kinds, low, high, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(model.get(), 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    expect_columns(read_all(model), added);
    int kind = 0;
    double lower = 0.0;
    double upper = 0.0;
    EXPECT_EQ(BIYOS_getvars(model.get(), 2, 1, &kind, &lower, &upper), BIYOS_OK);
    EXPECT_EQ(kind, BIYOS_VAR_INTEGER);
    EXPECT_DOUBLE_EQ(lower, -3.0);
    EXPECT_DOUBLE_EQ(upper, 7.0);
    EXPECT_EQ(BIYOS_getvars(model.get(), 5, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstIndex)
{
    const Env env;
    const Model model(env);
    int first = -1;
    ASSERT_EQ(BIYOS_addvars(model.get(), 0, nullptr, nullptr, nullptr, &first), BIYOS_OK);
    EXPECT_EQ(first, 0);
    ASSERT_EQ(BIYOS_addvars(model.get(), 1, nullptr, nullptr, nullptr, &first), BIYOS_OK);
    EXPECT_EQ(first, 0);
    ASSERT_EQ(BIYOS_addvars(model.get(), 3, nullptr, nullptr, nullptr, &first), BIYOS_OK);
    EXPECT_EQ(first, 1);
    ASSERT_EQ(BIYOS_addvars(model.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(model.get(), 0, nullptr, nullptr, nullptr, &first), BIYOS_OK);
    EXPECT_EQ(first, 6);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, RefusedAdd)
{
    const Env env;
    const Model model(env);
    int first = -1;
    const int kinds[] = {BIYOS_VAR_CONTINUOUS, 9};
    EXPECT_EQ(BIYOS_addvars(model.get(), 2, kinds, nullptr, nullptr, &first),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: variable 1: kind 9 is unknown");
    const double lower[] = {0.0, kInfinity};
    EXPECT_EQ(BIYOS_addvars(model.get(), 2, nullptr, lower, nullptr, &first),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: variable 1: lower bound is infinity");
    EXPECT_EQ(BIYOS_addvars(model.get(), -1, nullptr, nullptr, nullptr, &first),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': -1 is negative");
    EXPECT_EQ(first, -1);
    int count = -1;
    EXPECT_EQ(BIYOS_getnvars(model.get(), &count), BIYOS_OK);
    EXPECT_EQ(count, 0);
}

TEST(CApiTest, BinaryKind)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvars(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const int index = 0;
    const int binary = BIYOS_VAR_BINARY;
    EXPECT_EQ(BIYOS_setvarkinds(model.get(), 1, &index, &binary), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(),
              "invalid argument: variable 0: bounds [-inf, inf] are outside "
              "[0, 1], the widest of its kind");
    const double lower = 0.0;
    const double upper = 1.0;
    const double wide = 2.0;
    ASSERT_EQ(BIYOS_setvarbounds(model.get(), 1, &index, &lower, &upper), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarkinds(model.get(), 1, &index, &binary), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarbounds(model.get(), 1, &index, nullptr, &wide), BIYOS_INVALID_ARGUMENT);
    expect_columns(read_all(model), Columns{.kinds = {binary}, .lower = {0.0}, .upper = {1.0}});
}

TEST(CApiTest, BadIndex)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvars(model.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Columns before = read_all(model);
    const int beyond[] = {0, 3};
    const double bounds[] = {0.0, 1.0};
    const int kinds[] = {BIYOS_VAR_INTEGER, BIYOS_VAR_INTEGER};
    EXPECT_EQ(BIYOS_setvarbounds(model.get(), 2, beyond, bounds, bounds), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    const int negative[] = {0, -1};
    EXPECT_EQ(BIYOS_setvarkinds(model.get(), 2, negative, kinds), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    expect_columns(read_all(model), before);
    int found[] = {0, 0};
    EXPECT_EQ(BIYOS_getvars(model.get(), 2, 2, found, nullptr, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    EXPECT_EQ(BIYOS_getvars(model.get(), -1, 1, found, nullptr, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_getvars(model.get(), 0, -1, found, nullptr, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': -1 is negative");
    EXPECT_EQ(found[0], 0);
    EXPECT_EQ(found[1], 0);
}

TEST(CApiTest, NullAdd)
{
    const Env env;
    const Model model(env);
    const int kinds[] = {BIYOS_VAR_BINARY, BIYOS_VAR_INTEGER};
    const double bounds[] = {0.5, 0.75};
    ASSERT_EQ(BIYOS_addvars(model.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(model.get(), 2, kinds, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(model.get(), 2, kinds, bounds, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(model.get(), 2, kinds, nullptr, bounds, nullptr), BIYOS_OK);
    const int continuous = BIYOS_VAR_CONTINUOUS;
    const Columns wanted{
        .kinds =
            {continuous, continuous, kinds[0], kinds[1], kinds[0], kinds[1], kinds[0], kinds[1]},
        .lower = {-kInfinity, -kInfinity, 0.0, -kInfinity, 0.5, 0.75, 0.0, -kInfinity},
        .upper = {kInfinity, kInfinity, 1.0, kInfinity, 1.0, kInfinity, 0.5, 0.75},
    };
    expect_columns(read_all(model), wanted);
}

TEST(CApiTest, NullSet)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvars(model.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const int indices[] = {1, 0};
    const double lower[] = {1.0, 2.0};
    const double upper[] = {3.0, 4.0};
    const int kinds[] = {BIYOS_VAR_INTEGER, BIYOS_VAR_SEMIINTEGER};
    ASSERT_EQ(BIYOS_setvarbounds(model.get(), 2, indices, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarbounds(model.get(), 1, indices, nullptr, upper), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarbounds(model.get(), 2, indices, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarkinds(model.get(), 2, indices, nullptr), BIYOS_OK);
    const int continuous = BIYOS_VAR_CONTINUOUS;
    const Columns bounded{.kinds = {continuous, continuous},
                          .lower = {2.0, 1.0},
                          .upper = {kInfinity, 3.0}};
    expect_columns(read_all(model), bounded);
    ASSERT_EQ(BIYOS_setvarkinds(model.get(), 2, indices, kinds), BIYOS_OK);
    expect_columns(
        read_all(model),
        Columns{.kinds = {kinds[1], kinds[0]}, .lower = bounded.lower, .upper = bounded.upper});
    EXPECT_EQ(BIYOS_setvarbounds(model.get(), 2, nullptr, lower, upper), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'indices': is null");
    EXPECT_EQ(BIYOS_setvarkinds(model.get(), 2, nullptr, kinds), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'indices': is null");
    EXPECT_EQ(BIYOS_setvarbounds(model.get(), 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarkinds(model.get(), 0, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullGet)
{
    const Env env;
    const Model model(env);
    const int kinds[] = {BIYOS_VAR_INTEGER, BIYOS_VAR_BINARY};
    const double lower[] = {-1.0, 0.0};
    const double upper[] = {1.0, 0.5};
    ASSERT_EQ(BIYOS_addvars(model.get(), 2, kinds, lower, upper, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvars(model.get(), 0, 2, nullptr, nullptr, nullptr), BIYOS_OK);
    int found[] = {0, 0};
    double low[] = {9.0, 9.0};
    double high[] = {9.0, 9.0};
    EXPECT_EQ(BIYOS_getvars(model.get(), 0, 2, found, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvars(model.get(), 0, 2, nullptr, low, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvars(model.get(), 1, 1, nullptr, nullptr, high), BIYOS_OK);
    EXPECT_EQ(found[1], BIYOS_VAR_BINARY);
    EXPECT_DOUBLE_EQ(low[0], -1.0);
    EXPECT_DOUBLE_EQ(high[0], 0.5);
    EXPECT_DOUBLE_EQ(high[1], 9.0);
    EXPECT_EQ(BIYOS_getnvars(model.get(), nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': is null");
}

TEST(CApiTest, ModelEvent)
{
    const Env env;
    const Model model(env);
    std::vector<std::string> texts;
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), keep, &texts, message, BIYOS_LEVEL_ERROR, &id), BIYOS_OK);
    const int index = 0;
    const double bound = 0.0;
    EXPECT_EQ(BIYOS_setvarbounds(model.get(), 1, &index, &bound, &bound), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(texts, std::vector<std::string>{"invalid argument: variable 0: unknown"});
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
}
