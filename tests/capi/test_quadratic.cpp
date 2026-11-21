#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

constexpr int kNoVariable = -7;
constexpr double kNoValue = -7.5;

struct Compressed
{
    std::vector<std::int64_t> starts;
    std::vector<int> firsts;
    std::vector<int> seconds;
    std::vector<double> values;
};

const std::vector<int> kFunctions{2, 0};

const Compressed kSample{
    .starts = {0, 2, 5},
    .firsts = {1, 2, 0, 2, 1},
    .seconds = {0, 2, 0, 1, 1},
    .values = {3.0, 1.0, 2.0, -1.0, -0.0},
};

const Compressed kSorted{
    .starts = {0, 3, 3, 5},
    .firsts = {0, 1, 1, 0, 2},
    .seconds = {0, 1, 2, 1, 2},
    .values = {2.0, -0.0, -1.0, 3.0, 1.0},
};

BIYOS_status add_terms(const Model &mdl, const std::vector<int> &funs, const Compressed &terms)
{
    return BIYOS_addquadterms(mdl.get(),
                              static_cast<int>(funs.size()),
                              funs.data(),
                              terms.starts.data(),
                              terms.firsts.data(),
                              terms.seconds.data(),
                              terms.values.data());
}

void add_sample(const Model &mdl)
{
    BIYOS_model *held = mdl.get();
    ASSERT_EQ(BIYOS_addvars(held, 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfuns(held, 3, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(add_terms(mdl, kFunctions, kSample), BIYOS_OK);
}

Compressed blank(std::size_t funs, std::size_t terms)
{
    return Compressed{
        .starts = std::vector<std::int64_t>(funs + 1, -1),
        .firsts = std::vector<int>(terms, kNoVariable),
        .seconds = std::vector<int>(terms, kNoVariable),
        .values = std::vector<double>(terms, kNoValue),
    };
}

std::int64_t read_with(const Model &mdl, int beg, int n, Compressed &read, std::int64_t size)
{
    BIYOS_offset needed = -1;
    const BIYOS_status status = BIYOS_getquadterms(mdl.get(),
                                                   beg,
                                                   n,
                                                   size,
                                                   read.starts.data(),
                                                   &needed,
                                                   read.firsts.data(),
                                                   read.seconds.data(),
                                                   read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return needed;
}

std::int64_t read_terms(const Model &mdl, int beg, int n, Compressed &read)
{
    return read_with(mdl, beg, n, read, static_cast<std::int64_t>(read.firsts.size()));
}

std::vector<std::uint64_t> bits_of(const std::vector<double> &values)
{
    std::vector<std::uint64_t> bits;
    bits.reserve(values.size());
    for (const double value : values)
    {
        bits.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return bits;
}

void expect_compressed(const Compressed &found, const Compressed &wanted)
{
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.firsts, wanted.firsts);
    EXPECT_EQ(found.seconds, wanted.seconds);
    EXPECT_EQ(bits_of(found.values), bits_of(wanted.values));
}

void expect_sample(const Model &mdl)
{
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_terms(mdl, 0, 3, read), 5);
    expect_compressed(read, kSorted);
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
    const BIYOS_index fun = 0;
    BIYOS_offset needed = 0;
    note(attempt, BIYOS_addquadterms(mdl, 1, &fun, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getquadterms(mdl, 0, 1, 0, nullptr, &needed, nullptr, nullptr, nullptr));
}

}

TEST(CApiTest, QuadraticTerms)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    expect_sample(mdl);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, QuadraticSums)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const Compressed more{
        .starts = {0, 1, 3},
        .firsts = {2, 1, 1},
        .seconds = {2, 0, 2},
        .values = {0.5, 0.25, 4.0},
    };
    ASSERT_EQ(add_terms(mdl, {0, 2}, more), BIYOS_OK);
    Compressed read = blank(3, 7);
    EXPECT_EQ(read_terms(mdl, 0, 3, read), 7);
    const Compressed wanted{
        .starts = {0, 4, 4, 7},
        .firsts = {0, 1, 1, 2, 0, 1, 2},
        .seconds = {0, 1, 2, 2, 1, 2, 2},
        .values = {2.0, -0.0, -1.0, 0.5, 3.25, 4.0, 1.0},
    };
    expect_compressed(read, wanted);
}

TEST(CApiTest, QuadraticSizes)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_offset needed = -1;
    BIYOS_model *held = mdl.get();
    EXPECT_EQ(BIYOS_getquadterms(held, 0, 3, 0, nullptr, &needed, nullptr, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(needed, 5);
    Compressed none = blank(3, 2);
    EXPECT_EQ(read_with(mdl, 0, 3, none, 0), 5);
    Compressed unread = blank(3, 2);
    unread.starts = kSorted.starts;
    expect_compressed(none, unread);
    Compressed small = blank(3, 4);
    EXPECT_EQ(read_with(mdl, 0, 3, small, 2), 5);
    const Compressed cut{
        .starts = kSorted.starts,
        .firsts = {0, 1, kNoVariable, kNoVariable},
        .seconds = {0, 1, kNoVariable, kNoVariable},
        .values = {2.0, -0.0, kNoValue, kNoValue},
    };
    expect_compressed(small, cut);
    Compressed large = blank(3, 6);
    EXPECT_EQ(read_terms(mdl, 0, 3, large), 5);
    const Compressed whole{
        .starts = kSorted.starts,
        .firsts = {0, 1, 1, 0, 2, kNoVariable},
        .seconds = {0, 1, 2, 1, 2, kNoVariable},
        .values = {2.0, -0.0, -1.0, 3.0, 1.0, kNoValue},
    };
    expect_compressed(large, whole);
}

TEST(CApiTest, QuadraticRange)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Compressed read = blank(2, 2);
    EXPECT_EQ(read_terms(mdl, 1, 2, read), 2);
    const Compressed wanted{
        .starts = {0, 0, 2},
        .firsts = {0, 2},
        .seconds = {1, 2},
        .values = {3.0, 1.0},
    };
    expect_compressed(read, wanted);
    Compressed none = blank(0, 0);
    EXPECT_EQ(read_terms(mdl, 3, 0, none), 0);
    EXPECT_EQ(none.starts, std::vector<std::int64_t>{0});
}

TEST(CApiTest, NullQuadratic)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const int funs[] = {1, 2};
    const std::int64_t one[] = {0, 1};
    const BIYOS_index var = 0;
    const BIYOS_real value = 1.0;
    EXPECT_EQ(BIYOS_addquadterms(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addquadterms(held, 2, funs, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addquadterms(held, 1, nullptr, one, &var, &var, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': is null");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, one, nullptr, &var, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'varidx1': is null");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, one, &var, nullptr, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'varidx2': is null");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, one, &var, &var, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'coef': is null");
    expect_sample(mdl);
}

TEST(CApiTest, NullPairs)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Compressed read = blank(3, 5);
    BIYOS_offset needed = -1;
    BIYOS_model *held = mdl.get();
    std::int64_t *starts = read.starts.data();
    int *firsts = read.firsts.data();
    int *seconds = read.seconds.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getquadterms(held, 0, 3, 5, nullptr, &needed, firsts, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_getquadterms(held, 0, 3, 5, nullptr, &needed, nullptr, seconds, nullptr),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_getquadterms(held, 0, 3, 5, starts, &needed, nullptr, nullptr, values),
              BIYOS_OK);
    expect_compressed(read, kSorted);
    Compressed kept = blank(3, 5);
    starts = kept.starts.data();
    firsts = kept.firsts.data();
    EXPECT_EQ(BIYOS_getquadterms(held, 0, 3, 5, starts, nullptr, firsts, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
    expect_compressed(kept, blank(3, 5));
}

TEST(CApiTest, RefusedQuadratic)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const int funs[] = {0, 1};
    const int repeated[] = {1, 1};
    const int unknown[] = {3};
    const std::int64_t late[] = {1, 1};
    const std::int64_t falling[] = {0, 1, 0};
    const std::int64_t negative[] = {0, -1};
    const std::int64_t one[] = {0, 1};
    const std::int64_t both[] = {0, 2};
    const int firsts[] = {1, 3};
    const int seconds[] = {0, 1};
    const int crossed[] = {1, 0};
    const double values[] = {1.0, 2.0};
    const BIYOS_real nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, late, firsts, seconds, values), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': begins at 1, not 0");
    EXPECT_EQ(BIYOS_addquadterms(held, 2, funs, falling, firsts, seconds, values),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': decreases from 1 to 0 at entry 2");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, negative, firsts, seconds, values),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': decreases from 0 to -1 at entry 1");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, unknown, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 3: unknown");
    EXPECT_EQ(BIYOS_addquadterms(held, 2, repeated, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': function 1 is repeated");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, both, firsts, seconds, values), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 0: variable 3 is unknown");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, both, seconds, firsts, values), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 0: variable 3 is unknown");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, one, firsts, seconds, &nan), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 0: coefficient of pair (1, 0) is NaN");
    EXPECT_EQ(BIYOS_addquadterms(held, 1, funs, both, crossed, seconds, values), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 0: pair (0, 1) is repeated");
    EXPECT_EQ(BIYOS_addquadterms(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    expect_sample(mdl);
}

TEST(CApiTest, RefusedPairs)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const std::vector<std::pair<int, int>> ranges{{2, 2}, {-1, 1}, {0, -1}, {4, 0}};
    const std::vector<std::string> msgs{
        "invalid argument: function 3: unknown",
        "invalid argument: function -1: unknown",
        "invalid argument: argument 'n': -1 is negative",
        "invalid argument: function 4: unknown",
    };
    for (std::size_t k = 0; k < ranges.size(); ++k)
    {
        Compressed read = blank(3, 5);
        BIYOS_offset needed = -1;
        const auto [beg, n] = ranges[k];
        EXPECT_EQ(BIYOS_getquadterms(mdl.get(),
                                     beg,
                                     n,
                                     5,
                                     read.starts.data(),
                                     &needed,
                                     read.firsts.data(),
                                     read.seconds.data(),
                                     read.values.data()),
                  BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), msgs[k]);
        EXPECT_EQ(needed, -1);
        expect_compressed(read, blank(3, 5));
    }
    Compressed read = blank(3, 5);
    BIYOS_offset needed = -1;
    EXPECT_EQ(BIYOS_getquadterms(mdl.get(),
                                 0,
                                 3,
                                 -1,
                                 read.starts.data(),
                                 &needed,
                                 read.firsts.data(),
                                 read.seconds.data(),
                                 read.values.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'size': -1 is negative");
    EXPECT_EQ(needed, -1);
    expect_compressed(read, blank(3, 5));
}

TEST(CApiTest, LockedQuadratic)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = mdl.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
    expect_sample(mdl);
}

TEST(CApiTest, HugeQuadratic)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const std::vector<int> fun{1};
    const Compressed most{
        .starts = {0, (std::numeric_limits<std::int64_t>::max)()},
        .firsts = {0, 1},
        .seconds = {0, 1},
        .values = {1.0, 2.0},
    };
    Compressed large = most;
    large.starts = {0, std::int64_t{1} << 59};
    const std::string msg = "out of memory: model: no quadratic term offset left";
    EXPECT_EQ(add_terms(mdl, fun, most), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), msg);
    EXPECT_EQ(add_terms(mdl, fun, large), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), msg);
    expect_sample(mdl);
}
