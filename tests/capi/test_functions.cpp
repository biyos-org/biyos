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
constexpr double kNoConstant = -9.5;
constexpr int kNoVariable = -7;
constexpr double kNoValue = -7.5;

struct Compressed
{
    std::vector<double> constants;
    std::vector<std::int64_t> starts;
    std::vector<int> variables;
    std::vector<double> values;
};

const Compressed kSample{
    .constants = {1.5, 0.0, -2.0},
    .starts = {0, 3, 3, 5},
    .variables = {2, 0, 1, 1, 0},
    .values = {3.0, 1.0, 0.0, 5.0, 4.0},
};

const Compressed kSorted{
    .constants = {1.5, 0.0, -2.0},
    .starts = {0, 3, 3, 5},
    .variables = {0, 1, 2, 0, 1},
    .values = {1.0, 0.0, 3.0, 4.0, 5.0},
};

void add_sample(const Model &mdl)
{
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_status status = BIYOS_addfuns(mdl.get(),
                                              3,
                                              kSample.constants.data(),
                                              kSample.starts.data(),
                                              kSample.variables.data(),
                                              kSample.values.data(),
                                              nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

Compressed blank(std::size_t funs, std::size_t terms)
{
    return Compressed{
        .constants = std::vector<double>(funs, kNoConstant),
        .starts = std::vector<std::int64_t>(funs + 1, -1),
        .variables = std::vector<int>(terms, kNoVariable),
        .values = std::vector<double>(terms, kNoValue),
    };
}

BIYOS_status fill(const Model &mdl,
                  int beg,
                  int n,
                  Compressed &read,
                  std::int64_t size,
                  BIYOS_offset *needed)
{
    return BIYOS_getfuns(mdl.get(),
                         beg,
                         n,
                         size,
                         read.constants.data(),
                         read.starts.data(),
                         needed,
                         read.variables.data(),
                         read.values.data());
}

std::int64_t read_with(const Model &mdl, int beg, int n, Compressed &read, std::int64_t size)
{
    BIYOS_offset needed = -1;
    EXPECT_EQ(fill(mdl, beg, n, read, size, &needed), BIYOS_OK);
    return needed;
}

std::int64_t read_funs(const Model &mdl, int beg, int n, Compressed &read)
{
    return read_with(mdl, beg, n, read, static_cast<std::int64_t>(read.variables.size()));
}

void expect_compressed(const Compressed &found, const Compressed &wanted)
{
    expect_dbls(found.constants, wanted.constants);
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.variables, wanted.variables);
    expect_dbls(found.values, wanted.values);
}

int n_funs(const Model &mdl)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnfuns(mdl.get(), &n), BIYOS_OK);
    return n;
}

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
    attempt.messages.emplace_back(BIYOS_lasterror(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *mdl = attempt.model;
    BIYOS_index n = 0;
    BIYOS_offset needed = 0;
    note(attempt, BIYOS_addfuns(mdl, 1, nullptr, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getnfuns(mdl, &n));
    note(attempt, BIYOS_getfuns(mdl, 0, 1, 0, nullptr, nullptr, &needed, nullptr, nullptr));
}

}

TEST(CApiTest, CompressedFunctions)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    EXPECT_EQ(n_funs(mdl), 3);
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_funs(mdl, 0, 3, read), 5);
    expect_compressed(read, kSorted);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstFunction)
{
    const Env env;
    const Model mdl(env);
    BIYOS_index beg = -1;
    ASSERT_EQ(BIYOS_addfuns(mdl.get(), 0, nullptr, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    ASSERT_EQ(BIYOS_addfuns(mdl.get(), 2, nullptr, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    ASSERT_EQ(BIYOS_addfuns(mdl.get(), 3, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfuns(mdl.get(), 1, nullptr, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 5);
    const BIYOS_real infinite = kInfinity;
    EXPECT_EQ(BIYOS_addfuns(mdl.get(), 1, &infinite, nullptr, nullptr, nullptr, &beg),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(beg, 5);
    EXPECT_EQ(n_funs(mdl), 6);
}

TEST(CApiTest, SizeZero)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_offset needed = -1;
    EXPECT_EQ(BIYOS_getfuns(mdl.get(), 0, 3, 0, nullptr, nullptr, &needed, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(needed, 5);
    Compressed read = blank(3, 2);
    EXPECT_EQ(read_with(mdl, 0, 3, read, 0), 5);
    const Compressed wanted{
        .constants = kSorted.constants,
        .starts = kSorted.starts,
        .variables = {kNoVariable, kNoVariable},
        .values = {kNoValue, kNoValue},
    };
    expect_compressed(read, wanted);
}

TEST(CApiTest, SmallSize)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Compressed read = blank(3, 4);
    EXPECT_EQ(read_with(mdl, 0, 3, read, 2), 5);
    const Compressed wanted{
        .constants = kSorted.constants,
        .starts = kSorted.starts,
        .variables = {0, 1, kNoVariable, kNoVariable},
        .values = {1.0, 0.0, kNoValue, kNoValue},
    };
    expect_compressed(read, wanted);
}

TEST(CApiTest, LargeSize)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Compressed read = blank(3, 7);
    EXPECT_EQ(read_funs(mdl, 0, 3, read), 5);
    const Compressed wanted{
        .constants = kSorted.constants,
        .starts = kSorted.starts,
        .variables = {0, 1, 2, 0, 1, kNoVariable, kNoVariable},
        .values = {1.0, 0.0, 3.0, 4.0, 5.0, kNoValue, kNoValue},
    };
    expect_compressed(read, wanted);
}

TEST(CApiTest, FunctionRange)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Compressed read = blank(2, 2);
    EXPECT_EQ(read_funs(mdl, 1, 2, read), 2);
    const Compressed wanted{
        .constants = {0.0, -2.0},
        .starts = {0, 0, 2},
        .variables = {0, 1},
        .values = {4.0, 5.0},
    };
    expect_compressed(read, wanted);
    Compressed none = blank(0, 0);
    EXPECT_EQ(read_funs(mdl, 3, 0, none), 0);
    EXPECT_EQ(none.starts, std::vector<std::int64_t>{0});
}

TEST(CApiTest, NullFunctions)
{
    const Env env;
    const Model mdl(env);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const double constants[] = {0.5, 0.75};
    const std::int64_t empty[] = {0, 0, 0};
    BIYOS_model *held = mdl.get();
    ASSERT_EQ(BIYOS_addfuns(held, 2, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfuns(held, 2, constants, empty, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfuns(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    Compressed read = blank(4, 0);
    EXPECT_EQ(read_funs(mdl, 0, 4, read), 0);
    expect_dbls(read.constants, {0.0, 0.0, 0.5, 0.75});
    EXPECT_EQ(read.starts, (std::vector<std::int64_t>{0, 0, 0, 0, 0}));
    const std::int64_t one[] = {0, 1};
    const BIYOS_index var = 0;
    const BIYOS_real value = 1.0;
    EXPECT_EQ(BIYOS_addfuns(held, 1, nullptr, one, nullptr, &value, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'varidx': is null");
    EXPECT_EQ(BIYOS_addfuns(held, 1, nullptr, one, &var, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'coef': is null");
    EXPECT_EQ(n_funs(mdl), 4);
}

TEST(CApiTest, NullTerms)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Compressed read = blank(3, 5);
    BIYOS_offset needed = -1;
    BIYOS_model *held = mdl.get();
    double *constants = read.constants.data();
    std::int64_t *starts = read.starts.data();
    int *vars = read.variables.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getfuns(held, 0, 3, 5, nullptr, nullptr, &needed, vars, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getfuns(held, 0, 3, 5, nullptr, starts, &needed, nullptr, values), BIYOS_OK);
    EXPECT_EQ(BIYOS_getfuns(held, 0, 3, 0, constants, nullptr, &needed, nullptr, nullptr),
              BIYOS_OK);
    expect_compressed(read, kSorted);
    Compressed untouched = blank(3, 5);
    EXPECT_EQ(fill(mdl, 0, 3, untouched, 5, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
    expect_compressed(untouched, blank(3, 5));
    EXPECT_EQ(BIYOS_getnfuns(held, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
}

TEST(CApiTest, RefusedFunctions)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const std::int64_t late[] = {1, 1};
    const std::int64_t falling[] = {0, 1, 0};
    const std::int64_t negative[] = {0, -1};
    const int vars[] = {0, 3};
    const double values[] = {1.0, 2.0};
    const BIYOS_real nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(BIYOS_addfuns(held, 1, nullptr, late, vars, values, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': begins at 1, not 0");
    EXPECT_EQ(BIYOS_addfuns(held, 2, nullptr, falling, vars, values, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': decreases from 1 to 0 at entry 2");
    EXPECT_EQ(BIYOS_addfuns(held, 1, nullptr, negative, vars, values, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': decreases from 0 to -1 at entry 1");
    const std::int64_t both[] = {0, 2};
    EXPECT_EQ(BIYOS_addfuns(held, 1, nullptr, both, vars, values, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 3: variable 3 is unknown");
    EXPECT_EQ(BIYOS_addfuns(held, 1, &nan, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 3: constant is NaN");
    EXPECT_EQ(BIYOS_addfuns(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    BIYOS_offset needed = -1;
    Compressed untouched = blank(3, 5);
    EXPECT_EQ(fill(mdl, 2, 2, untouched, 5, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 3: unknown");
    EXPECT_EQ(fill(mdl, -1, 1, untouched, 5, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function -1: unknown");
    EXPECT_EQ(fill(mdl, 0, 1, untouched, -1, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'size': -1 is negative");
    EXPECT_EQ(needed, -1);
    expect_compressed(untouched, blank(3, 5));
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_funs(mdl, 0, 3, read), 5);
    expect_compressed(read, kSorted);
}

TEST(CApiTest, LockedFunctions)
{
    const Env env;
    const Model mdl(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = mdl.get();
    ASSERT_EQ(BIYOS_addfuns(mdl.get(), 1, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG, BIYOS_OK, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(n_funs(mdl), 1);
}

TEST(CApiTest, HugeFunctions)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const std::int64_t most[] = {0, (std::numeric_limits<std::int64_t>::max)()};
    const std::int64_t large[] = {0, std::int64_t{1} << 59};
    const int vars[] = {0, 1};
    const double values[] = {1.0, 2.0};
    EXPECT_EQ(BIYOS_addfuns(held, 1, nullptr, most, vars, values, nullptr), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), "out of memory: model: no term offset left");
    EXPECT_EQ(BIYOS_addfuns(held, 1, nullptr, large, vars, values, nullptr), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), "out of memory: model: no term offset left");
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_funs(mdl, 0, 3, read), 5);
    expect_compressed(read, kSorted);
}
