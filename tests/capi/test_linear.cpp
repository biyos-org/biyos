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

BIYOS_status add_functions(const Model &model, int n, const Terms &terms)
{
    return BIYOS_addfuns(model.get(),
                         n,
                         nullptr,
                         terms.starts.data(),
                         terms.indices.data(),
                         terms.values.data(),
                         nullptr);
}

BIYOS_status add_terms(const Model &model, const std::vector<int> &functions, const Terms &terms)
{
    return BIYOS_addlinterms(model.get(),
                             static_cast<int>(functions.size()),
                             functions.data(),
                             terms.starts.data(),
                             terms.indices.data(),
                             terms.values.data());
}

void add_sample(const Model &model)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(add_functions(model, 3, kFunctions), BIYOS_OK);
}

int n_functions(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnfun(model.get(), &n), BIYOS_OK);
    return n;
}

Terms read_functions(const Model &model)
{
    const int n = n_functions(model);
    BIYOS_offset nnz = -1;
    BIYOS_model *held = model.get();
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

std::int64_t read_with(const Model &model, int start, int size, Terms &read, std::int64_t capacity)
{
    BIYOS_offset nnz = -1;
    const BIYOS_status status = BIYOS_getclintermrg(model.get(),
                                                    start,
                                                    size,
                                                    capacity,
                                                    read.starts.data(),
                                                    &nnz,
                                                    read.indices.data(),
                                                    read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return nnz;
}

Terms read_variables(const Model &model, int start, int size)
{
    BIYOS_offset nnz = -1;
    BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_getclintermrg(held, start, size, 0, nullptr, &nnz, nullptr, nullptr), BIYOS_OK);
    Terms read = blank(static_cast<std::size_t>(size), static_cast<std::size_t>(nnz));
    EXPECT_EQ(read_with(model, start, size, read, nnz), nnz);
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
    const BIYOS_index function = 0;
    BIYOS_offset nnz = 0;
    note(attempt, BIYOS_addlinterms(attempt.model, 1, &function, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getclintermrg(attempt.model, 0, 1, 0, nullptr, &nnz, nullptr, nullptr));
}

using Engine = std::mt19937;

int below(Engine &engine, int n)
{
    return static_cast<int>(engine() % static_cast<unsigned>(n));
}

void add_random(Engine &engine, int n_variables, Terms &terms)
{
    for (int variable = n_variables - 1; variable >= 0; --variable)
    {
        if (below(engine, 2) == 0)
        {
            terms.indices.push_back(variable);
            terms.values.push_back(static_cast<double>(below(engine, 9) - 4));
        }
    }
    terms.starts.push_back(static_cast<std::int64_t>(terms.indices.size()));
}

Terms random_terms(Engine &engine, std::size_t n_functions, int n_variables)
{
    Terms terms{.starts = {0}, .indices = {}, .values = {}};
    for (std::size_t k = 0; k < n_functions; ++k)
    {
        add_random(engine, n_variables, terms);
    }
    return terms;
}

std::vector<int> random_functions(Engine &engine, int n_functions)
{
    std::vector<int> functions;
    for (int function = n_functions - 1; function >= 0; --function)
    {
        if (below(engine, 2) == 0)
        {
            functions.push_back(function);
        }
    }
    return functions;
}

void build_random(Engine &engine, const Model &model, int n_variables, int n_functions)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), n_variables, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const auto all = static_cast<std::size_t>(n_functions);
    ASSERT_EQ(add_functions(model, n_functions, random_terms(engine, all, n_variables)), BIYOS_OK);
    for (int round = 0; round < 3; ++round)
    {
        const std::vector<int> functions = random_functions(engine, n_functions);
        ASSERT_EQ(add_terms(model, functions, random_terms(engine, functions.size(), n_variables)),
                  BIYOS_OK);
    }
}

void add_holders(const Terms &rows, int variable, Terms &columns)
{
    for (std::size_t function = 0; function + 1 < rows.starts.size(); ++function)
    {
        const auto start = static_cast<std::size_t>(rows.starts[function]);
        const auto end = static_cast<std::size_t>(rows.starts[function + 1]);
        for (std::size_t k = start; k < end; ++k)
        {
            if (rows.indices[k] == variable)
            {
                columns.indices.push_back(static_cast<int>(function));
                columns.values.push_back(rows.values[k]);
            }
        }
    }
    columns.starts.push_back(static_cast<std::int64_t>(columns.indices.size()));
}

Terms transposed(const Terms &rows, int start, int size)
{
    Terms columns{.starts = {0}, .indices = {}, .values = {}};
    for (int variable = start; variable < start + size; ++variable)
    {
        add_holders(rows, variable, columns);
    }
    return columns;
}

}

TEST(CApiTest, LinearTerms)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    expect_terms(read_functions(model), kHeld);
    ASSERT_EQ(add_terms(model, kOwners, kAdded), BIYOS_OK);
    expect_terms(read_functions(model), kMerged);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, LinearSums)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    ASSERT_EQ(add_terms(model, kOwners, kAdded), BIYOS_OK);
    const Terms more{.starts = {0, 2}, .indices = {1, 2}, .values = {-5.0, 0.25}};
    ASSERT_EQ(add_terms(model, {2}, more), BIYOS_OK);
    const Terms wanted{
        .starts = {0, 3, 5, 8},
        .indices = {0, 1, 2, 0, 2, 0, 1, 2},
        .values = {1.0, 0.0, 3.0, -0.0, 2.0, 3.0, 0.0, 0.75},
    };
    expect_terms(read_functions(model), wanted);
}

TEST(CApiTest, NullLinear)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const int functions[] = {1, 2};
    const std::int64_t one[] = {0, 1};
    const BIYOS_index variable = 0;
    const BIYOS_real value = 1.0;
    EXPECT_EQ(BIYOS_addlinterms(held, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addlinterms(held, 2, functions, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addlinterms(held, 1, nullptr, one, &variable, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': is null");
    EXPECT_EQ(BIYOS_addlinterms(held, 1, functions, one, nullptr, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'varidx': is null");
    EXPECT_EQ(BIYOS_addlinterms(held, 1, functions, one, &variable, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'coef': is null");
    expect_terms(read_functions(model), kHeld);
}

TEST(CApiTest, RefusedLinear)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    ASSERT_EQ(add_functions(model, 1, Terms{.starts = {0, 1}, .indices = {1}, .values = {kLarge}}),
              BIYOS_OK);
    const Terms before = read_functions(model);
    const Terms one{.starts = {0, 1}, .indices = {0}, .values = {1.0}};
    const Terms none{.starts = {0, 0, 0}, .indices = {}, .values = {}};
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(add_terms(model, {4}, one), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 4: unknown");
    EXPECT_EQ(add_terms(model, {-1}, one), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function -1: unknown");
    EXPECT_EQ(add_terms(model, {2, 2}, none), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'funidx': function 2 is repeated");
    const Terms late{.starts = {1, 1}, .indices = {0}, .values = {1.0}};
    EXPECT_EQ(add_terms(model, {0}, late), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': begins at 1, not 0");
    const Terms falling{.starts = {0, 1, 0}, .indices = {0}, .values = {1.0}};
    EXPECT_EQ(add_terms(model, {0, 1}, falling), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': decreases from 1 to 0 at entry 2");
    const Terms negative{.starts = {0, -1}, .indices = {0}, .values = {1.0}};
    EXPECT_EQ(add_terms(model, {0}, negative), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': decreases from 0 to -1 at entry 1");
    const Terms unknown{.starts = {0, 1, 2}, .indices = {0, 3}, .values = {1.0, 1.0}};
    EXPECT_EQ(add_terms(model, {1, 0}, unknown), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 0: variable 3 is unknown");
    const Terms twice{.starts = {0, 2}, .indices = {2, 2}, .values = {1.0, 2.0}};
    EXPECT_EQ(add_terms(model, {1}, twice), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 1: variable 2 is repeated");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const Terms invalid{.starts = {0, 1}, .indices = {1}, .values = {nan}};
    EXPECT_EQ(add_terms(model, {0}, invalid), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 0: coefficient of variable 1 is NaN");
    const Terms large{.starts = {0, 1, 2}, .indices = {0, 1}, .values = {1.0, kLarge}};
    EXPECT_EQ(add_terms(model, {0, 3}, large), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 3: sum of coefficients of variable 1 is infinity");
    EXPECT_EQ(BIYOS_addlinterms(model.get(), -1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'n': -1 is negative");
    expect_terms(read_functions(model), before);
}

TEST(CApiTest, LockedLinear)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    expect_terms(read_functions(model), kHeld);
}

TEST(CApiTest, HugeLinear)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Terms most{
        .starts = {0, (std::numeric_limits<std::int64_t>::max)()},
        .indices = {0, 1},
        .values = {1.0, 2.0},
    };
    Terms large = most;
    large.starts = {0, std::int64_t{1} << 59};
    const std::string message = "out of memory: model: no term offset left";
    EXPECT_EQ(add_terms(model, {1}, most), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), message);
    EXPECT_EQ(add_terms(model, {1}, large), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), message);
    expect_terms(read_functions(model), kHeld);
}

TEST(CApiTest, ColumnTerms)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Terms held{
        .starts = {0, 2, 4, 5},
        .indices = {0, 2, 0, 2, 0},
        .values = {1.0, 4.0, 0.0, 5.0, 3.0},
    };
    expect_terms(read_variables(model, 0, 3), held);
    ASSERT_EQ(add_terms(model, kOwners, kAdded), BIYOS_OK);
    const Terms merged{
        .starts = {0, 3, 5, 8},
        .indices = {0, 1, 2, 0, 2, 0, 1, 2},
        .values = {1.0, -0.0, 3.0, 0.0, 5.0, 3.0, 2.0, 0.5},
    };
    expect_terms(read_variables(model, 0, 3), merged);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ColumnSizes)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const std::vector<std::int64_t> starts{0, 2, 4, 5};
    Terms none = blank(3, 2);
    EXPECT_EQ(read_with(model, 0, 3, none, 0), 5);
    Terms unread = blank(3, 2);
    unread.starts = starts;
    expect_terms(none, unread);
    Terms small = blank(3, 4);
    EXPECT_EQ(read_with(model, 0, 3, small, 3), 5);
    const Terms cut{
        .starts = starts,
        .indices = {0, 2, 0, kNoFunction},
        .values = {1.0, 4.0, 0.0, kNoValue},
    };
    expect_terms(small, cut);
    Terms large = blank(3, 6);
    EXPECT_EQ(read_with(model, 0, 3, large, 6), 5);
    const Terms whole{
        .starts = starts,
        .indices = {0, 2, 0, 2, 0, kNoFunction},
        .values = {1.0, 4.0, 0.0, 5.0, 3.0, kNoValue},
    };
    expect_terms(large, whole);
}

TEST(CApiTest, ColumnRange)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Terms tail{.starts = {0, 2, 3}, .indices = {0, 2, 0}, .values = {0.0, 5.0, 3.0}};
    expect_terms(read_variables(model, 1, 2), tail);
    Terms none = blank(0, 0);
    EXPECT_EQ(read_with(model, 3, 0, none, 0), 0);
    EXPECT_EQ(none.starts, std::vector<std::int64_t>{0});
}

TEST(CApiTest, ColumnNulls)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Terms read = blank(3, 5);
    BIYOS_offset nnz = -1;
    BIYOS_model *held = model.get();
    std::int64_t *starts = read.starts.data();
    int *functions = read.indices.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 3, 5, nullptr, &nnz, functions, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 3, 5, starts, &nnz, nullptr, values), BIYOS_OK);
    EXPECT_EQ(nnz, 5);
    expect_terms(read, read_variables(model, 0, 3));
    Terms kept = blank(3, 5);
    starts = kept.starts.data();
    functions = kept.indices.data();
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 3, 5, starts, nullptr, functions, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
    expect_terms(kept, blank(3, 5));
}

TEST(CApiTest, ColumnRefused)
{
    const Env env;
    const Model model(env);
    add_sample(model);
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
        const auto [start, n] = ranges[k];
        EXPECT_EQ(BIYOS_getclintermrg(model.get(),
                                      start,
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
    BIYOS_model *held = model.get();
    std::int64_t *starts = read.starts.data();
    int *functions = read.indices.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 5, -1, starts, nullptr, functions, values),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 3, -1, starts, nullptr, functions, values),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), size);
    expect_terms(read, blank(3, 5));
}

TEST(CApiTest, ColumnTranspose)
{
    Engine engine(20261121);
    for (int trial = 0; trial < 40; ++trial)
    {
        const Env env;
        const Model model(env);
        const int n_variables = 1 + below(engine, 7);
        const int n_functions = below(engine, 7);
        build_random(engine, model, n_variables, n_functions);
        const Terms rows = read_functions(model);
        expect_terms(read_variables(model, 0, n_variables), transposed(rows, 0, n_variables));
        const int start = below(engine, n_variables + 1);
        const int n = below(engine, n_variables - start + 1);
        expect_terms(read_variables(model, start, n), transposed(rows, start, n));
    }
}
