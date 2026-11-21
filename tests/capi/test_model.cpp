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
using biyos::test::expect_dbls;
using biyos::test::Model;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Columns
{
    std::vector<int> kinds;
    std::vector<double> lower;
    std::vector<double> upper;
};

Columns read_all(const Model &mdl)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnvars(mdl.get(), &n), BIYOS_OK);
    const auto size = static_cast<std::size_t>(n);
    Columns read{.kinds = std::vector<int>(size),
                 .lower = std::vector<double>(size),
                 .upper = std::vector<double>(size)};
    const BIYOS_status status =
        BIYOS_getvars(mdl.get(), 0, n, read.kinds.data(), read.lower.data(), read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void expect_columns(const Columns &found, const Columns &wanted)
{
    EXPECT_EQ(found.kinds, wanted.kinds);
    expect_dbls(found.lower, wanted.lower);
    expect_dbls(found.upper, wanted.upper);
}

const Columns kColumns{
    .kinds = {BIYOS_VAR_CONT, BIYOS_VAR_BIN, BIYOS_VAR_INT, BIYOS_VAR_SEMICONT, BIYOS_VAR_SEMIINT},
    .lower = {-kInfinity, 0.0, -3.0, 2.0, 1.0},
    .upper = {kInfinity, 1.0, 7.0, 10.0, 4.0},
};

void add_columns(const Model &mdl)
{
    const BIYOS_int *knds = kColumns.kinds.data();
    const double *lower = kColumns.lower.data();
    const double *upper = kColumns.upper.data();
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 5, knds, lower, upper, nullptr), BIYOS_OK);
}

Columns read_listed(const Model &mdl, const std::vector<BIYOS_index> &idx)
{
    const auto n = static_cast<BIYOS_index>(idx.size());
    Columns read{
        .kinds = std::vector<int>(idx.size(), -1),
        .lower = std::vector<double>(idx.size(), 9.0),
        .upper = std::vector<double>(idx.size(), 9.0),
    };
    const BIYOS_status status =
        BIYOS_getvarbnd(mdl.get(), n, idx.data(), read.lower.data(), read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarknd(mdl.get(), n, idx.data(), read.kinds.data()), BIYOS_OK);
    return read;
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

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *mdl = attempt.model;
    BIYOS_model *made = nullptr;
    const BIYOS_index idx = 0;
    const BIYOS_real bound = 0.0;
    const BIYOS_int knd = BIYOS_VAR_INT;
    note(attempt, BIYOS_newmdl(attempt.env, &made));
    note(attempt, BIYOS_freemdl(mdl));
    note(attempt, BIYOS_addvars(mdl, 1, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_setvarbnd(mdl, 1, &idx, &bound, &bound));
    note(attempt, BIYOS_setvarknd(mdl, 1, &idx, &knd));
    note(attempt, BIYOS_getvarbnd(mdl, 1, &idx, nullptr, nullptr));
    note(attempt, BIYOS_getvarknd(mdl, 1, &idx, nullptr));
    note(attempt, BIYOS_getnvars(mdl, &attempt.count));
    note(attempt, BIYOS_getvars(mdl, 0, 1, nullptr, nullptr, nullptr));
}

void keep(const BIYOS_event *evt, void *data)
{
    static_cast<std::vector<std::string> *>(data)->emplace_back(BIYOS_getevttext(evt));
}

void fail(const Env &env)
{
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
}

}

TEST(CApiTest, ModelCreate)
{
    const Env env;
    BIYOS_model *mdl = nullptr;
    ASSERT_EQ(BIYOS_newmdl(env.get(), &mdl), BIYOS_OK);
    EXPECT_NE(mdl, nullptr);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(BIYOS_freemdl(mdl), BIYOS_OK);
}

TEST(CApiTest, NullModel)
{
    const Env env;
    const Model kept(env);
    BIYOS_model *mdl = kept.get();
    EXPECT_EQ(BIYOS_newmdl(nullptr, &mdl), BIYOS_ERR_BADARG);
    EXPECT_EQ(mdl, nullptr);
    EXPECT_EQ(BIYOS_newmdl(env.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'mdl': is null");
    EXPECT_EQ(BIYOS_freemdl(nullptr), BIYOS_OK);
    const BIYOS_index idx = 0;
    const BIYOS_real bound = 0.0;
    BIYOS_index n = 0;
    EXPECT_EQ(BIYOS_addvars(nullptr, 1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setvarbnd(nullptr, 1, &idx, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getvarbnd(nullptr, 1, &idx, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setvarknd(nullptr, 1, &idx, &idx), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getvarknd(nullptr, 1, &idx, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnvars(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getvars(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    BIYOS_offset needed = 0;
    EXPECT_EQ(BIYOS_addfuns(nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnfuns(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getfuns(nullptr, 0, 0, 0, nullptr, nullptr, &needed, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addquadterms(nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getquadterms(nullptr, 0, 0, 0, nullptr, &needed, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addcons(nullptr, 1, &idx, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setconbnd(nullptr, 1, &idx, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getconbnd(nullptr, 1, &idx, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getncons(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getcons(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addobjs(nullptr, 1, &idx, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjs(nullptr, 1, &idx, nullptr, nullptr, &bound, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjsense(nullptr, 1, &idx, &idx), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjsense(nullptr, 1, &idx, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjprio(nullptr, 1, &idx, &idx), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjprio(nullptr, 1, &idx, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjcoef(nullptr, 1, &idx, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjcoef(nullptr, 1, &idx, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjtol(nullptr, 1, &idx, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjtol(nullptr, 1, &idx, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnobjs(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjs(nullptr, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
}

TEST(CApiTest, HeldFree)
{
    BIYOS_env *env = nullptr;
    BIYOS_model *first = nullptr;
    BIYOS_model *second = nullptr;
    ASSERT_EQ(BIYOS_newenv(&env), BIYOS_OK);
    ASSERT_EQ(BIYOS_newmdl(env, &first), BIYOS_OK);
    ASSERT_EQ(BIYOS_newmdl(env, &second), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_ERR_BADARG);
    EXPECT_STREQ(BIYOS_lasterror(env),
                 "invalid argument: environment: has a model that is not freed");
    EXPECT_EQ(BIYOS_freemdl(first), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setintopt(env, BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_EQ(BIYOS_freemdl(second), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_OK);
}

TEST(CApiTest, LockedModel)
{
    const Env env;
    const Model mdl(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = mdl.get();
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    fail(env);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status> statuses{
        refused,
        refused,
        refused,
        refused,
        refused,
        BIYOS_OK,
        BIYOS_OK,
        BIYOS_OK,
        BIYOS_OK,
    };
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    const std::string failure = "invalid argument: option 'seed': -1 is outside [0, 2147483647]";
    const std::vector<std::string> msgs{
        "invalid argument: environment: is read-only while delivering an event",
        locked,
        locked,
        locked,
        locked,
        locked,
        locked,
        locked,
        locked,
    };
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, msgs);
    EXPECT_EQ(attempt.count, 1);
    EXPECT_EQ(env.message(), failure);
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
}

TEST(CApiTest, BulkAdd)
{
    const Env env;
    const Model mdl(env);
    const Columns added{
        .kinds =
            {BIYOS_VAR_CONT, BIYOS_VAR_BIN, BIYOS_VAR_INT, BIYOS_VAR_SEMICONT, BIYOS_VAR_SEMIINT},
        .lower = {-kInfinity, 0.0, -3.0, 2.0, 1.0},
        .upper = {kInfinity, 1.0, 7.0, 10.0, 4.0},
    };
    const int *knds = added.kinds.data();
    const double *low = added.lower.data();
    const double *high = added.upper.data();
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 5, knds, low, high, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    expect_columns(read_all(mdl), added);
    BIYOS_int knd = 0;
    BIYOS_real lower = 0.0;
    BIYOS_real upper = 0.0;
    EXPECT_EQ(BIYOS_getvars(mdl.get(), 2, 1, &knd, &lower, &upper), BIYOS_OK);
    EXPECT_EQ(knd, BIYOS_VAR_INT);
    EXPECT_DOUBLE_EQ(lower, -3.0);
    EXPECT_DOUBLE_EQ(upper, 7.0);
    EXPECT_EQ(BIYOS_getvars(mdl.get(), 5, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstIndex)
{
    const Env env;
    const Model mdl(env);
    BIYOS_index beg = -1;
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 0, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 1, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 3, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 1);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 0, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 6);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, RefusedAdd)
{
    const Env env;
    const Model mdl(env);
    BIYOS_index beg = -1;
    const int knds[] = {BIYOS_VAR_CONT, 9};
    EXPECT_EQ(BIYOS_addvars(mdl.get(), 2, knds, nullptr, nullptr, &beg), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 1: kind 9 is unknown");
    const double lower[] = {0.0, kInfinity};
    EXPECT_EQ(BIYOS_addvars(mdl.get(), 2, nullptr, lower, nullptr, &beg), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 1: lower bound is infinity");
    EXPECT_EQ(BIYOS_addvars(mdl.get(), -1, nullptr, nullptr, nullptr, &beg), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(beg, -1);
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnvars(mdl.get(), &n), BIYOS_OK);
    EXPECT_EQ(n, 0);
}

TEST(CApiTest, BinaryKind)
{
    const Env env;
    const Model mdl(env);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_index idx = 0;
    const BIYOS_int binary = BIYOS_VAR_BIN;
    EXPECT_EQ(BIYOS_setvarknd(mdl.get(), 1, &idx, &binary), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: variable 0: bounds [-inf, inf] are outside "
              "[0, 1], the widest of its kind");
    const BIYOS_real lower = 0.0;
    const BIYOS_real upper = 1.0;
    const BIYOS_real wide = 2.0;
    ASSERT_EQ(BIYOS_setvarbnd(mdl.get(), 1, &idx, &lower, &upper), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarknd(mdl.get(), 1, &idx, &binary), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarbnd(mdl.get(), 1, &idx, nullptr, &wide), BIYOS_ERR_BADARG);
    expect_columns(read_all(mdl), Columns{.kinds = {binary}, .lower = {0.0}, .upper = {1.0}});
}

TEST(CApiTest, BadIndex)
{
    const Env env;
    const Model mdl(env);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Columns before = read_all(mdl);
    const int beyond[] = {0, 3};
    const double bnds[] = {0.0, 1.0};
    const int knds[] = {BIYOS_VAR_INT, BIYOS_VAR_INT};
    EXPECT_EQ(BIYOS_setvarbnd(mdl.get(), 2, beyond, bnds, bnds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    const int negative[] = {0, -1};
    EXPECT_EQ(BIYOS_setvarknd(mdl.get(), 2, negative, knds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    expect_columns(read_all(mdl), before);
    int found[] = {0, 0};
    EXPECT_EQ(BIYOS_getvars(mdl.get(), 2, 2, found, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    EXPECT_EQ(BIYOS_getvars(mdl.get(), -1, 1, found, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_getvars(mdl.get(), 0, -1, found, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(found[0], 0);
    EXPECT_EQ(found[1], 0);
}

TEST(CApiTest, NullAdd)
{
    const Env env;
    const Model mdl(env);
    const int knds[] = {BIYOS_VAR_BIN, BIYOS_VAR_INT};
    const double bnds[] = {0.5, 0.75};
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 2, knds, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 2, knds, bnds, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 2, knds, nullptr, bnds, nullptr), BIYOS_OK);
    const int continuous = BIYOS_VAR_CONT;
    const Columns wanted{
        .kinds = {continuous, continuous, knds[0], knds[1], knds[0], knds[1], knds[0], knds[1]},
        .lower = {-kInfinity, -kInfinity, 0.0, -kInfinity, 0.5, 0.75, 0.0, -kInfinity},
        .upper = {kInfinity, kInfinity, 1.0, kInfinity, 1.0, kInfinity, 0.5, 0.75},
    };
    expect_columns(read_all(mdl), wanted);
}

TEST(CApiTest, NullSet)
{
    const Env env;
    const Model mdl(env);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const int idx[] = {1, 0};
    const double lower[] = {1.0, 2.0};
    const double upper[] = {3.0, 4.0};
    const int knds[] = {BIYOS_VAR_INT, BIYOS_VAR_SEMIINT};
    ASSERT_EQ(BIYOS_setvarbnd(mdl.get(), 2, idx, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarbnd(mdl.get(), 1, idx, nullptr, upper), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarbnd(mdl.get(), 2, idx, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarknd(mdl.get(), 2, idx, nullptr), BIYOS_OK);
    const int continuous = BIYOS_VAR_CONT;
    const Columns bounded{.kinds = {continuous, continuous},
                          .lower = {2.0, 1.0},
                          .upper = {kInfinity, 3.0}};
    expect_columns(read_all(mdl), bounded);
    ASSERT_EQ(BIYOS_setvarknd(mdl.get(), 2, idx, knds), BIYOS_OK);
    expect_columns(
        read_all(mdl),
        Columns{.kinds = {knds[1], knds[0]}, .lower = bounded.lower, .upper = bounded.upper});
    EXPECT_EQ(BIYOS_setvarbnd(mdl.get(), 2, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_setvarknd(mdl.get(), 2, nullptr, knds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_setvarbnd(mdl.get(), 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarknd(mdl.get(), 0, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullGet)
{
    const Env env;
    const Model mdl(env);
    const int knds[] = {BIYOS_VAR_INT, BIYOS_VAR_BIN};
    const double lower[] = {-1.0, 0.0};
    const double upper[] = {1.0, 0.5};
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 2, knds, lower, upper, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvars(mdl.get(), 0, 2, nullptr, nullptr, nullptr), BIYOS_OK);
    int found[] = {0, 0};
    double low[] = {9.0, 9.0};
    double high[] = {9.0, 9.0};
    EXPECT_EQ(BIYOS_getvars(mdl.get(), 0, 2, found, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvars(mdl.get(), 0, 2, nullptr, low, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvars(mdl.get(), 1, 1, nullptr, nullptr, high), BIYOS_OK);
    EXPECT_EQ(found[1], BIYOS_VAR_BIN);
    EXPECT_DOUBLE_EQ(low[0], -1.0);
    EXPECT_DOUBLE_EQ(high[0], 0.5);
    EXPECT_DOUBLE_EQ(high[1], 9.0);
    EXPECT_EQ(BIYOS_getnvars(mdl.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
}

TEST(CApiTest, ListedVariables)
{
    const Env env;
    const Model mdl(env);
    add_columns(mdl);
    const Columns wanted{
        .kinds = {BIYOS_VAR_SEMIINT, BIYOS_VAR_CONT, BIYOS_VAR_SEMIINT, BIYOS_VAR_INT},
        .lower = {1.0, -kInfinity, 1.0, -3.0},
        .upper = {4.0, kInfinity, 4.0, 7.0},
    };
    expect_columns(read_listed(mdl, {4, 0, 4, 2}), wanted);
    expect_columns(read_listed(mdl, {}), Columns{});
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, VariableSetters)
{
    const Env env;
    const Model mdl(env);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_index idx[] = {2, 0, 2};
    const double lower[] = {0.0, -1.0, 0.5};
    const double upper[] = {1.0, 2.0, 1.0};
    const BIYOS_int knd[] = {BIYOS_VAR_INT, BIYOS_VAR_SEMICONT, BIYOS_VAR_BIN};
    ASSERT_EQ(BIYOS_setvarbnd(mdl.get(), 3, idx, lower, upper), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarknd(mdl.get(), 3, idx, knd), BIYOS_OK);
    std::vector<double> lb(3, 9.0);
    std::vector<double> ub(3, 9.0);
    std::vector<BIYOS_int> found(3, -1);
    ASSERT_EQ(BIYOS_getvarbnd(mdl.get(), 3, idx, lb.data(), ub.data()), BIYOS_OK);
    ASSERT_EQ(BIYOS_getvarknd(mdl.get(), 3, idx, found.data()), BIYOS_OK);
    const Columns wanted{
        .kinds = {BIYOS_VAR_BIN, BIYOS_VAR_SEMICONT, BIYOS_VAR_BIN},
        .lower = {0.5, -1.0, 0.5},
        .upper = {1.0, 2.0, 1.0},
    };
    expect_columns(Columns{.kinds = found, .lower = lb, .upper = ub}, wanted);
    const Columns untouched{.kinds = {BIYOS_VAR_CONT}, .lower = {-kInfinity}, .upper = {kInfinity}};
    expect_columns(read_listed(mdl, {1}), untouched);
}

TEST(CApiTest, VariableNulls)
{
    const Env env;
    const Model mdl(env);
    add_columns(mdl);
    const BIYOS_index idx[] = {3, 1};
    double lower[] = {9.0, 9.0};
    double upper[] = {9.0, 9.0};
    BIYOS_int knd[] = {-1, -1};
    const BIYOS_model *held = mdl.get();
    EXPECT_EQ(BIYOS_getvarbnd(held, 2, idx, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarknd(held, 2, idx, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarbnd(held, 2, idx, lower, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarbnd(held, 1, idx, nullptr, upper), BIYOS_OK);
    expect_dbls({lower[0], lower[1]}, {2.0, 0.0});
    expect_dbls({upper[0], upper[1]}, {10.0, 9.0});
    EXPECT_EQ(BIYOS_getvarbnd(held, 0, nullptr, lower, upper), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarknd(held, 0, nullptr, knd), BIYOS_OK);
    EXPECT_EQ(std::vector<BIYOS_int>({knd[0], knd[1]}), std::vector<BIYOS_int>({-1, -1}));
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, VariableRefusals)
{
    const Env env;
    const Model mdl(env);
    add_columns(mdl);
    const BIYOS_model *held = mdl.get();
    const BIYOS_index beyond[] = {0, 5};
    const BIYOS_index negative[] = {1, -1};
    double lower[] = {9.0, 9.0};
    double upper[] = {9.0, 9.0};
    BIYOS_int knd[] = {-1, -1};
    EXPECT_EQ(BIYOS_getvarbnd(held, 2, beyond, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 5: unknown");
    EXPECT_EQ(BIYOS_getvarknd(held, 2, beyond, knd), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 5: unknown");
    EXPECT_EQ(BIYOS_getvarbnd(held, 2, negative, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_getvarknd(held, 2, negative, knd), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_getvarbnd(held, -1, beyond, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_getvarknd(held, -1, beyond, knd), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_getvarbnd(held, 1, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_getvarknd(held, 1, nullptr, knd), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    expect_dbls({lower[0], lower[1]}, {9.0, 9.0});
    expect_dbls({upper[0], upper[1]}, {9.0, 9.0});
    EXPECT_EQ(std::vector<BIYOS_int>({knd[0], knd[1]}), std::vector<BIYOS_int>({-1, -1}));
}

TEST(CApiTest, ModelEvent)
{
    const Env env;
    const Model mdl(env);
    std::vector<std::string> texts;
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), keep, &texts, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    const BIYOS_index idx = 0;
    const BIYOS_real bound = 0.0;
    EXPECT_EQ(BIYOS_setvarbnd(mdl.get(), 1, &idx, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(texts, std::vector<std::string>{"invalid argument: variable 0: unknown"});
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
}
