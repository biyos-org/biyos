#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

constexpr double kLarge = 1.0e308;

struct Terms
{
    std::vector<std::int64_t> starts;
    std::vector<int> indices;
    std::vector<double> values;
};

const Terms kFunctions{
    .starts = {0, 3, 3, 5},
    .indices = {2, 0, 1, 1, 0},
    .values = {3.0, 1.0, 0.0, 5.0, 4.0},
};

const Terms kHeld{
    .starts = {0, 3, 3, 5},
    .indices = {0, 1, 2, 0, 1},
    .values = {1.0, 0.0, 3.0, 4.0, 5.0},
};

const std::vector<int> kOwners{2, 1};

const Terms kAdded{
    .starts = {0, 2, 4},
    .indices = {2, 0, 2, 0},
    .values = {0.5, -1.0, 2.0, -0.0},
};

const Terms kMerged{
    .starts = {0, 3, 5, 8},
    .indices = {0, 1, 2, 0, 2, 0, 1, 2},
    .values = {1.0, 0.0, 3.0, -0.0, 2.0, 3.0, 5.0, 0.5},
};

BIYOS_status add_funs(const Model &mdl, int n, const Terms &terms)
{
    return BIYOS_addfuns(mdl.get(),
                         n,
                         nullptr,
                         terms.starts.data(),
                         terms.indices.data(),
                         terms.values.data(),
                         nullptr);
}

BIYOS_status add_terms(const Model &mdl, const std::vector<int> &funs, const Terms &terms)
{
    return BIYOS_addlinterms(mdl.get(),
                             static_cast<int>(funs.size()),
                             funs.data(),
                             terms.starts.data(),
                             terms.indices.data(),
                             terms.values.data());
}

void add_sample(const Model &mdl)
{
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(add_funs(mdl, 3, kFunctions), BIYOS_OK);
}

int n_funs(const Model &mdl)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnfuns(mdl.get(), &n), BIYOS_OK);
    return n;
}

Terms read_funs(const Model &mdl)
{
    const int n = n_funs(mdl);
    BIYOS_offset nnz = -1;
    BIYOS_model *held = mdl.get();
    EXPECT_EQ(BIYOS_getfuns(held, 0, n, 0, nullptr, nullptr, &nnz, nullptr, nullptr), BIYOS_OK);
    const auto size = static_cast<std::size_t>(nnz);
    Terms read{
        .starts = std::vector<std::int64_t>(static_cast<std::size_t>(n) + 1),
        .indices = std::vector<int>(size),
        .values = std::vector<double>(size),
    };
    const BIYOS_status status = BIYOS_getfuns(held,
                                              0,
                                              n,
                                              nnz,
                                              nullptr,
                                              read.starts.data(),
                                              &nnz,
                                              read.indices.data(),
                                              read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
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

void expect_terms(const Terms &found, const Terms &wanted)
{
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.indices, wanted.indices);
    EXPECT_EQ(bits_of(found.values), bits_of(wanted.values));
}

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *mdl = nullptr;
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
    const BIYOS_index fun = 0;
    note(attempt, BIYOS_addlinterms(attempt.mdl, 1, &fun, nullptr, nullptr, nullptr));
}

}

TEST(CApiTest, LinearTerms)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    expect_terms(read_funs(mdl), kHeld);
    ASSERT_EQ(add_terms(mdl, kOwners, kAdded), BIYOS_OK);
    expect_terms(read_funs(mdl), kMerged);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, LinearSums)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    ASSERT_EQ(add_terms(mdl, kOwners, kAdded), BIYOS_OK);
    const Terms more{.starts = {0, 2}, .indices = {1, 2}, .values = {-5.0, 0.25}};
    ASSERT_EQ(add_terms(mdl, {2}, more), BIYOS_OK);
    const Terms wanted{
        .starts = {0, 3, 5, 8},
        .indices = {0, 1, 2, 0, 2, 0, 1, 2},
        .values = {1.0, 0.0, 3.0, -0.0, 2.0, 3.0, 0.0, 0.75},
    };
    expect_terms(read_funs(mdl), wanted);
}

TEST(CApiTest, NullLinear)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const int funs[] = {1, 2};
    const std::int64_t one[] = {0, 1};
    const BIYOS_index var = 0;
    const BIYOS_real value = 1.0;
    EXPECT_EQ(BIYOS_addlinterms(held, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addlinterms(held, 2, funs, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addlinterms(held, 1, nullptr, one, &var, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': is null");
    EXPECT_EQ(BIYOS_addlinterms(held, 1, funs, one, nullptr, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'varidx': is null");
    EXPECT_EQ(BIYOS_addlinterms(held, 1, funs, one, &var, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'coef': is null");
    expect_terms(read_funs(mdl), kHeld);
}

TEST(CApiTest, RefusedLinear)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    ASSERT_EQ(add_funs(mdl, 1, Terms{.starts = {0, 1}, .indices = {1}, .values = {kLarge}}),
              BIYOS_OK);
    const Terms before = read_funs(mdl);
    const Terms one{.starts = {0, 1}, .indices = {0}, .values = {1.0}};
    const Terms none{.starts = {0, 0, 0}, .indices = {}, .values = {}};
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(add_terms(mdl, {4}, one), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 4: unknown");
    EXPECT_EQ(add_terms(mdl, {-1}, one), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function -1: unknown");
    EXPECT_EQ(add_terms(mdl, {2, 2}, none), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'funidx': function 2 is repeated");
    const Terms late{.starts = {1, 1}, .indices = {0}, .values = {1.0}};
    EXPECT_EQ(add_terms(mdl, {0}, late), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': begins at 1, not 0");
    const Terms falling{.starts = {0, 1, 0}, .indices = {0}, .values = {1.0}};
    EXPECT_EQ(add_terms(mdl, {0, 1}, falling), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': decreases from 1 to 0 at entry 2");
    const Terms negative{.starts = {0, -1}, .indices = {0}, .values = {1.0}};
    EXPECT_EQ(add_terms(mdl, {0}, negative), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': decreases from 0 to -1 at entry 1");
    const Terms unknown{.starts = {0, 1, 2}, .indices = {0, 3}, .values = {1.0, 1.0}};
    EXPECT_EQ(add_terms(mdl, {1, 0}, unknown), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 0: variable 3 is unknown");
    const Terms twice{.starts = {0, 2}, .indices = {2, 2}, .values = {1.0, 2.0}};
    EXPECT_EQ(add_terms(mdl, {1}, twice), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 1: variable 2 is repeated");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const Terms invalid{.starts = {0, 1}, .indices = {1}, .values = {nan}};
    EXPECT_EQ(add_terms(mdl, {0}, invalid), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 0: coefficient of variable 1 is NaN");
    const Terms large{.starts = {0, 1, 2}, .indices = {0, 1}, .values = {1.0, kLarge}};
    EXPECT_EQ(add_terms(mdl, {0, 3}, large), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 3: sum of coefficients of variable 1 is infinity");
    EXPECT_EQ(BIYOS_addlinterms(mdl.get(), -1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'n': -1 is negative");
    expect_terms(read_funs(mdl), before);
}

TEST(CApiTest, LockedLinear)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Attempt attempt;
    attempt.env = env.get();
    attempt.mdl = mdl.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(attempt.statuses, std::vector<BIYOS_status>{BIYOS_ERR_BADARG});
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
    expect_terms(read_funs(mdl), kHeld);
}

TEST(CApiTest, HugeLinear)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const Terms most{
        .starts = {0, (std::numeric_limits<std::int64_t>::max)()},
        .indices = {0, 1},
        .values = {1.0, 2.0},
    };
    Terms large = most;
    large.starts = {0, std::int64_t{1} << 59};
    const std::string message = "out of memory: model: no term offset left";
    EXPECT_EQ(add_terms(mdl, {1}, most), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), message);
    EXPECT_EQ(add_terms(mdl, {1}, large), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), message);
    expect_terms(read_funs(mdl), kHeld);
}
