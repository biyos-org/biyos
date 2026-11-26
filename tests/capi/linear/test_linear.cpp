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
using biyos::test::last_message;
using biyos::test::Model;

constexpr int kNoFunction = -7;
constexpr double kNoValue = -7.5;

struct Terms
{
    std::vector<std::int64_t> counts;
    std::vector<int> indices;
    std::vector<double> values;
};

const Terms kFunctions{
    .counts = {3, 0, 2},
    .indices = {2, 0, 1, 1, 0},
    .values = {3.0, 1.0, 0.0, 5.0, 4.0},
};

const Terms kHeld{
    .counts = {3, 0, 2},
    .indices = {0, 1, 2, 0, 1},
    .values = {1.0, 0.0, 3.0, 4.0, 5.0},
};

const std::vector<int> kOwners{2, 1};

const Terms kAdded{
    .counts = {2, 2},
    .indices = {2, 0, 2, 0},
    .values = {0.5, -1.0, 2.0, -0.0},
};

const Terms kMerged{
    .counts = {3, 2, 3},
    .indices = {0, 1, 2, 0, 2, 0, 1, 2},
    .values = {1.0, 0.0, 3.0, -0.0, 2.0, 3.0, 5.0, 0.5},
};

BIYOS_status add_functions(const Model &model, int n, const Terms &terms)
{
    BIYOS_index start = -1;
    const BIYOS_status status = BIYOS_addfun(model.get(), n, &start);
    if (status != BIYOS_OK)
    {
        return status;
    }
    return BIYOS_addrlintermrg(model.get(),
                               start,
                               start + n,
                               terms.counts.data(),
                               terms.indices.data(),
                               terms.values.data());
}

std::vector<int> owners_of(const std::vector<int> &functions, const Terms &terms)
{
    std::vector<int> owners;
    for (std::size_t j = 0; j < functions.size(); ++j)
    {
        const auto count = static_cast<std::size_t>(terms.counts[j]);
        owners.insert(owners.end(), count, functions[j]);
    }
    return owners;
}

BIYOS_status add_terms(const Model &model, const std::vector<int> &functions, const Terms &terms)
{
    const std::vector<int> owners = owners_of(functions, terms);
    return BIYOS_addlinterm(model.get(),
                            static_cast<BIYOS_offset>(owners.size()),
                            owners.data(),
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
    EXPECT_EQ(BIYOS_getnrlintermrg(held, 0, n, &nnz), BIYOS_OK);
    const auto size = static_cast<std::size_t>(nnz);
    Terms read{
        .counts = std::vector<std::int64_t>(static_cast<std::size_t>(n)),
        .indices = std::vector<int>(size),
        .values = std::vector<double>(size),
    };
    const BIYOS_status status = BIYOS_getrlintermrg(held,
                                                    0,
                                                    n,
                                                    nnz,
                                                    read.counts.data(),
                                                    read.indices.data(),
                                                    read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Terms blank(std::size_t n, std::size_t nnz)
{
    return Terms{
        .counts = std::vector<std::int64_t>(n, -1),
        .indices = std::vector<int>(nnz, kNoFunction),
        .values = std::vector<double>(nnz, kNoValue),
    };
}

BIYOS_status read_with(const Model &model, int start, int size, Terms &read, std::int64_t capacity)
{
    return BIYOS_getclintermrg(model.get(),
                               start,
                               start + size,
                               capacity,
                               read.counts.data(),
                               read.indices.data(),
                               read.values.data());
}

Terms read_variables(const Model &model, int start, int size)
{
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnclintermrg(model.get(), start, start + size, &nnz), BIYOS_OK);
    Terms read = blank(static_cast<std::size_t>(size), static_cast<std::size_t>(nnz));
    EXPECT_EQ(read_with(model, start, size, read, nnz), BIYOS_OK);
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
    EXPECT_EQ(found.counts, wanted.counts);
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
    attempt.messages.emplace_back(last_message(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_offset nnz = 0;
    note(attempt, BIYOS_addlinterm(attempt.model, 0, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_addrlintermrg(attempt.model, 0, 1, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_addclintermrg(attempt.model, 0, 1, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getnclintermrg(attempt.model, 0, 1, &nnz));
}

using Engine = std::mt19937;

int below(Engine &engine, int n)
{
    return static_cast<int>(engine() % static_cast<unsigned>(n));
}

void add_random(Engine &engine, int n_variables, Terms &terms)
{
    const std::size_t before = terms.indices.size();
    for (int variable = n_variables - 1; variable >= 0; --variable)
    {
        if (below(engine, 2) == 0)
        {
            terms.indices.push_back(variable);
            terms.values.push_back(static_cast<double>(below(engine, 9) - 4));
        }
    }
    terms.counts.push_back(static_cast<std::int64_t>(terms.indices.size() - before));
}

Terms random_terms(Engine &engine, std::size_t n_functions, int n_variables)
{
    Terms terms;
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
    const std::size_t before = columns.indices.size();
    std::size_t k = 0;
    for (std::size_t function = 0; function < rows.counts.size(); ++function)
    {
        const std::size_t end = k + static_cast<std::size_t>(rows.counts[function]);
        for (; k < end; ++k)
        {
            if (rows.indices[k] == variable)
            {
                columns.indices.push_back(static_cast<int>(function));
                columns.values.push_back(rows.values[k]);
            }
        }
    }
    columns.counts.push_back(static_cast<std::int64_t>(columns.indices.size() - before));
}

Terms transposed(const Terms &rows, int start, int size)
{
    Terms columns;
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
    const Terms more{.counts = {2}, .indices = {1, 2}, .values = {-5.0, 0.25}};
    ASSERT_EQ(add_terms(model, {2}, more), BIYOS_OK);
    const Terms wanted{
        .counts = {3, 2, 3},
        .indices = {0, 1, 2, 0, 2, 0, 1, 2},
        .values = {1.0, 0.0, 3.0, -0.0, 2.0, 3.0, 0.0, 0.75},
    };
    expect_terms(read_functions(model), wanted);
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
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG,
                                             BIYOS_ERR_BADARG,
                                             BIYOS_ERR_BADARG,
                                             BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(attempt.messages[1], attempt.messages[0]);
    EXPECT_EQ(attempt.messages[2], attempt.messages[0]);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    expect_terms(read_functions(model), kHeld);
}

TEST(CApiTest, HugeLinear)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Terms most{
        .counts = {(std::numeric_limits<std::int64_t>::max)()},
        .indices = {0, 1},
        .values = {1.0, 2.0},
    };
    Terms large = most;
    large.counts = {std::int64_t{1} << 59};
    const std::string message = "out of memory: model: no term offset left";
    BIYOS_model *held = model.get();
    const int *variables = most.indices.data();
    const double *values = most.values.data();
    EXPECT_EQ(BIYOS_addrlintermrg(held, 1, 2, most.counts.data(), variables, values),
              BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), message);
    EXPECT_EQ(BIYOS_addclintermrg(held, 1, 2, large.counts.data(), variables, values),
              BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), message);
    expect_terms(read_functions(model), kHeld);
}

TEST(CApiTest, ColumnTerms)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Terms held{
        .counts = {2, 2, 1},
        .indices = {0, 2, 0, 2, 0},
        .values = {1.0, 4.0, 0.0, 5.0, 3.0},
    };
    expect_terms(read_variables(model, 0, 3), held);
    ASSERT_EQ(add_terms(model, kOwners, kAdded), BIYOS_OK);
    const Terms merged{
        .counts = {3, 2, 3},
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
    const std::vector<std::int64_t> counts{2, 2, 1};
    Terms small = blank(3, 4);
    EXPECT_EQ(read_with(model, 0, 3, small, 4), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 4 of the 5 terms");
    expect_terms(small, blank(3, 4));
    Terms large = blank(3, 6);
    EXPECT_EQ(read_with(model, 0, 3, large, 6), BIYOS_OK);
    const Terms whole{
        .counts = counts,
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
    const Terms tail{.counts = {2, 1}, .indices = {0, 2, 0}, .values = {0.0, 5.0, 3.0}};
    expect_terms(read_variables(model, 1, 2), tail);
    Terms none = blank(0, 0);
    EXPECT_EQ(read_with(model, 3, 0, none, 0), BIYOS_OK);
}

TEST(CApiTest, ColumnNulls)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Terms read = blank(3, 5);
    BIYOS_model *held = model.get();
    std::int64_t *counts = read.counts.data();
    int *functions = read.indices.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 3, 5, nullptr, functions, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 3, 5, counts, nullptr, values), BIYOS_OK);
    expect_terms(read, read_variables(model, 0, 3));
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnclintermrg(held, 0, 3, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
    EXPECT_EQ(BIYOS_getnclintermrg(held, 0, 3, &nnz), BIYOS_OK);
    EXPECT_EQ(nnz, 5);
}

TEST(CApiTest, ColumnRefused)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const std::vector<std::pair<int, int>> ranges{{2, 2}, {-1, 1}, {0, -1}, {4, 0}, {0, 3}};
    const std::vector<std::int64_t> sizes{5, 5, 5, 5, -1};
    const std::string size = "invalid argument: argument 'cap': -1 is negative";
    const std::vector<std::string> messages{
        "invalid argument: variable 3: unknown",
        "invalid argument: variable -1: unknown",
        "invalid argument: argument 'end': -1 is below 'beg' 0",
        "invalid argument: variable 4: unknown",
        size,
    };
    for (std::size_t k = 0; k < ranges.size(); ++k)
    {
        Terms read = blank(3, 5);
        const auto [start, n] = ranges[k];
        EXPECT_EQ(read_with(model, start, n, read, sizes[k]), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), messages[k]);
        expect_terms(read, blank(3, 5));
    }
    Terms read = blank(3, 5);
    BIYOS_model *held = model.get();
    std::int64_t *counts = read.counts.data();
    int *functions = read.indices.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 5, -1, counts, functions, values), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    EXPECT_EQ(BIYOS_getclintermrg(held, 0, 3, -1, counts, functions, values), BIYOS_ERR_BADARG);
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
