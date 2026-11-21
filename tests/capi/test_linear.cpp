#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

constexpr double kLarge = 1.0e308;
constexpr int kNoFunction = -7;
constexpr double kNoValue = -7.5;

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

Terms blank(std::size_t n, std::size_t nnz)
{
    return Terms{
        .starts = std::vector<std::int64_t>(n + 1, -1),
        .indices = std::vector<int>(nnz, kNoFunction),
        .values = std::vector<double>(nnz, kNoValue),
    };
}

std::int64_t read_with(const Model &mdl, int beg, int n, Terms &read, std::int64_t size)
{
    BIYOS_offset nnz = -1;
    const BIYOS_status status = BIYOS_getvarterms(mdl.get(),
                                                  beg,
                                                  n,
                                                  size,
                                                  read.starts.data(),
                                                  &nnz,
                                                  read.indices.data(),
                                                  read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return nnz;
}

Terms read_vars(const Model &mdl, int beg, int n)
{
    BIYOS_offset nnz = -1;
    BIYOS_model *held = mdl.get();
    EXPECT_EQ(BIYOS_getvarterms(held, beg, n, 0, nullptr, &nnz, nullptr, nullptr), BIYOS_OK);
    Terms read = blank(static_cast<std::size_t>(n), static_cast<std::size_t>(nnz));
    EXPECT_EQ(read_with(mdl, beg, n, read, nnz), nnz);
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
    BIYOS_offset nnz = 0;
    note(attempt, BIYOS_addlinterms(attempt.mdl, 1, &fun, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getvarterms(attempt.mdl, 0, 1, 0, nullptr, &nnz, nullptr, nullptr));
}

using Engine = std::mt19937;

int below(Engine &engine, int n)
{
    return static_cast<int>(engine() % static_cast<unsigned>(n));
}

void add_random(Engine &engine, int n_vars, Terms &terms)
{
    for (int var = n_vars - 1; var >= 0; --var)
    {
        if (below(engine, 2) == 0)
        {
            terms.indices.push_back(var);
            terms.values.push_back(static_cast<double>(below(engine, 9) - 4));
        }
    }
    terms.starts.push_back(static_cast<std::int64_t>(terms.indices.size()));
}

Terms random_terms(Engine &engine, std::size_t n_funs, int n_vars)
{
    Terms terms{.starts = {0}, .indices = {}, .values = {}};
    for (std::size_t k = 0; k < n_funs; ++k)
    {
        add_random(engine, n_vars, terms);
    }
    return terms;
}

std::vector<int> random_funs(Engine &engine, int n_funs)
{
    std::vector<int> funs;
    for (int fun = n_funs - 1; fun >= 0; --fun)
    {
        if (below(engine, 2) == 0)
        {
            funs.push_back(fun);
        }
    }
    return funs;
}

void build_random(Engine &engine, const Model &mdl, int n_vars, int n_funs)
{
    ASSERT_EQ(BIYOS_addvars(mdl.get(), n_vars, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const auto all = static_cast<std::size_t>(n_funs);
    ASSERT_EQ(add_funs(mdl, n_funs, random_terms(engine, all, n_vars)), BIYOS_OK);
    for (int round = 0; round < 3; ++round)
    {
        const std::vector<int> funs = random_funs(engine, n_funs);
        ASSERT_EQ(add_terms(mdl, funs, random_terms(engine, funs.size(), n_vars)), BIYOS_OK);
    }
}

void add_holders(const Terms &by_fun, int var, Terms &by_var)
{
    for (std::size_t fun = 0; fun + 1 < by_fun.starts.size(); ++fun)
    {
        const auto beg = static_cast<std::size_t>(by_fun.starts[fun]);
        const auto end = static_cast<std::size_t>(by_fun.starts[fun + 1]);
        for (std::size_t k = beg; k < end; ++k)
        {
            if (by_fun.indices[k] == var)
            {
                by_var.indices.push_back(static_cast<int>(fun));
                by_var.values.push_back(by_fun.values[k]);
            }
        }
    }
    by_var.starts.push_back(static_cast<std::int64_t>(by_var.indices.size()));
}

Terms transposed(const Terms &by_fun, int beg, int n)
{
    Terms by_var{.starts = {0}, .indices = {}, .values = {}};
    for (int var = beg; var < beg + n; ++var)
    {
        add_holders(by_fun, var, by_var);
    }
    return by_var;
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
    const std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
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

TEST(CApiTest, VarTerms)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const Terms held{
        .starts = {0, 2, 4, 5},
        .indices = {0, 2, 0, 2, 0},
        .values = {1.0, 4.0, 0.0, 5.0, 3.0},
    };
    expect_terms(read_vars(mdl, 0, 3), held);
    ASSERT_EQ(add_terms(mdl, kOwners, kAdded), BIYOS_OK);
    const Terms merged{
        .starts = {0, 3, 5, 8},
        .indices = {0, 1, 2, 0, 2, 0, 1, 2},
        .values = {1.0, -0.0, 3.0, 0.0, 5.0, 3.0, 2.0, 0.5},
    };
    expect_terms(read_vars(mdl, 0, 3), merged);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, VarSizes)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const std::vector<std::int64_t> starts{0, 2, 4, 5};
    Terms none = blank(3, 2);
    EXPECT_EQ(read_with(mdl, 0, 3, none, 0), 5);
    Terms unread = blank(3, 2);
    unread.starts = starts;
    expect_terms(none, unread);
    Terms small = blank(3, 4);
    EXPECT_EQ(read_with(mdl, 0, 3, small, 3), 5);
    const Terms cut{
        .starts = starts,
        .indices = {0, 2, 0, kNoFunction},
        .values = {1.0, 4.0, 0.0, kNoValue},
    };
    expect_terms(small, cut);
    Terms large = blank(3, 6);
    EXPECT_EQ(read_with(mdl, 0, 3, large, 6), 5);
    const Terms whole{
        .starts = starts,
        .indices = {0, 2, 0, 2, 0, kNoFunction},
        .values = {1.0, 4.0, 0.0, 5.0, 3.0, kNoValue},
    };
    expect_terms(large, whole);
}

TEST(CApiTest, VarRange)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const Terms tail{.starts = {0, 2, 3}, .indices = {0, 2, 0}, .values = {0.0, 5.0, 3.0}};
    expect_terms(read_vars(mdl, 1, 2), tail);
    Terms none = blank(0, 0);
    EXPECT_EQ(read_with(mdl, 3, 0, none, 0), 0);
    EXPECT_EQ(none.starts, std::vector<std::int64_t>{0});
}

TEST(CApiTest, VarNulls)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    Terms read = blank(3, 5);
    BIYOS_offset nnz = -1;
    BIYOS_model *held = mdl.get();
    std::int64_t *starts = read.starts.data();
    int *funs = read.indices.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getvarterms(held, 0, 3, 5, nullptr, &nnz, funs, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarterms(held, 0, 3, 5, starts, &nnz, nullptr, values), BIYOS_OK);
    EXPECT_EQ(nnz, 5);
    expect_terms(read, read_vars(mdl, 0, 3));
    Terms kept = blank(3, 5);
    starts = kept.starts.data();
    funs = kept.indices.data();
    EXPECT_EQ(BIYOS_getvarterms(held, 0, 3, 5, starts, nullptr, funs, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
    expect_terms(kept, blank(3, 5));
}

TEST(CApiTest, VarRefused)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const std::vector<std::pair<int, int>> ranges{{2, 2}, {-1, 1}, {0, -1}, {4, 0}, {0, 3}};
    const std::vector<std::int64_t> sizes{5, 5, 5, 5, -1};
    const std::string size = "invalid argument: argument 'size': -1 is negative";
    const std::vector<std::string> messages{
        "invalid argument: variable 3: unknown",
        "invalid argument: variable -1: unknown",
        "invalid argument: argument 'n': -1 is negative",
        "invalid argument: variable 4: unknown",
        size,
    };
    for (std::size_t k = 0; k < ranges.size(); ++k)
    {
        Terms read = blank(3, 5);
        BIYOS_offset nnz = -1;
        const auto [beg, n] = ranges[k];
        EXPECT_EQ(BIYOS_getvarterms(mdl.get(),
                                    beg,
                                    n,
                                    sizes[k],
                                    read.starts.data(),
                                    &nnz,
                                    read.indices.data(),
                                    read.values.data()),
                  BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), messages[k]);
        EXPECT_EQ(nnz, -1);
        expect_terms(read, blank(3, 5));
    }
    Terms read = blank(3, 5);
    BIYOS_model *held = mdl.get();
    std::int64_t *starts = read.starts.data();
    int *funs = read.indices.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getvarterms(held, 0, 5, -1, starts, nullptr, funs, values), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    EXPECT_EQ(BIYOS_getvarterms(held, 0, 3, -1, starts, nullptr, funs, values), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), size);
    expect_terms(read, blank(3, 5));
}

TEST(CApiTest, VarTranspose)
{
    Engine engine(20261121);
    for (int trial = 0; trial < 40; ++trial)
    {
        const Env env;
        const Model mdl(env);
        const int n_vars = 1 + below(engine, 7);
        const int n_funs = below(engine, 7);
        build_random(engine, mdl, n_vars, n_funs);
        const Terms by_fun = read_funs(mdl);
        expect_terms(read_vars(mdl, 0, n_vars), transposed(by_fun, 0, n_vars));
        const int beg = below(engine, n_vars + 1);
        const int n = below(engine, n_vars - beg + 1);
        expect_terms(read_vars(mdl, beg, n), transposed(by_fun, beg, n));
    }
}
