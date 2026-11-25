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
    std::vector<int> rows;
    std::vector<int> columns;
    std::vector<double> values;
};

const std::vector<int> kFunctions{2, 0};

const Compressed kSample{
    .starts = {0, 2, 5},
    .rows = {1, 2, 0, 2, 1},
    .columns = {0, 2, 0, 1, 1},
    .values = {3.0, 1.0, 2.0, -1.0, -0.0},
};

const Compressed kSorted{
    .starts = {0, 3, 3, 5},
    .rows = {0, 1, 1, 0, 2},
    .columns = {0, 1, 2, 1, 2},
    .values = {2.0, -0.0, -1.0, 3.0, 1.0},
};

std::vector<int> owners_of(const std::vector<int> &functions, const Compressed &terms)
{
    std::vector<int> owners;
    for (std::size_t j = 0; j < functions.size(); ++j)
    {
        const auto count = static_cast<std::size_t>(terms.starts[j + 1] - terms.starts[j]);
        owners.insert(owners.end(), count, functions[j]);
    }
    return owners;
}

BIYOS_status add_terms(const Model &model,
                       const std::vector<int> &functions,
                       const Compressed &terms)
{
    const std::vector<int> owners = owners_of(functions, terms);
    return BIYOS_addquadtermsp(model.get(),
                               static_cast<BIYOS_offset>(owners.size()),
                               owners.data(),
                               terms.rows.data(),
                               terms.columns.data(),
                               terms.values.data());
}

void add_sample(const Model &model)
{
    BIYOS_model *held = model.get();
    ASSERT_EQ(BIYOS_addvar(held, 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(held, 3, nullptr), BIYOS_OK);
    ASSERT_EQ(add_terms(model, kFunctions, kSample), BIYOS_OK);
}

Compressed blank(std::size_t functions, std::size_t terms)
{
    return Compressed{
        .starts = std::vector<std::int64_t>(functions + 1, -1),
        .rows = std::vector<int>(terms, kNoVariable),
        .columns = std::vector<int>(terms, kNoVariable),
        .values = std::vector<double>(terms, kNoValue),
    };
}

BIYOS_status read_with(const Model &model,
                       int start,
                       int size,
                       Compressed &read,
                       std::int64_t capacity)
{
    return BIYOS_getquadtermrg(model.get(),
                               start,
                               start + size,
                               capacity,
                               read.starts.data(),
                               read.rows.data(),
                               read.columns.data(),
                               read.values.data());
}

std::int64_t count_of(const Model &model, int start, int size)
{
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnquadtermrg(model.get(), start, start + size, &nnz), BIYOS_OK);
    return nnz;
}

std::int64_t read_terms(const Model &model, int start, int size, Compressed &read)
{
    const auto capacity = static_cast<std::int64_t>(read.rows.size());
    EXPECT_EQ(read_with(model, start, size, read, capacity), BIYOS_OK);
    return count_of(model, start, size);
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
    EXPECT_EQ(found.rows, wanted.rows);
    EXPECT_EQ(found.columns, wanted.columns);
    EXPECT_EQ(bits_of(found.values), bits_of(wanted.values));
}

void expect_sample(const Model &model)
{
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_terms(model, 0, 3, read), 5);
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
    attempt.messages.emplace_back(biyos::test::last_message(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    BIYOS_offset needed = 0;
    note(attempt, BIYOS_addquadtermsp(model, 0, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_addquadtermrg(model, 0, 1, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getnquadtermrg(model, 0, 1, &needed));
}

}

TEST(CApiTest, QuadraticTerms)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    expect_sample(model);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, QuadraticSums)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Compressed more{
        .starts = {0, 1, 3},
        .rows = {2, 1, 1},
        .columns = {2, 0, 2},
        .values = {0.5, 0.25, 4.0},
    };
    ASSERT_EQ(add_terms(model, {0, 2}, more), BIYOS_OK);
    Compressed read = blank(3, 7);
    EXPECT_EQ(read_terms(model, 0, 3, read), 7);
    const Compressed wanted{
        .starts = {0, 4, 4, 7},
        .rows = {0, 1, 1, 2, 0, 1, 2},
        .columns = {0, 1, 2, 2, 1, 2, 2},
        .values = {2.0, -0.0, -1.0, 0.5, 3.25, 4.0, 1.0},
    };
    expect_compressed(read, wanted);
}

TEST(CApiTest, QuadraticSizes)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(count_of(model, 0, 3), 5);
    Compressed none = blank(3, 2);
    EXPECT_EQ(read_with(model, 0, 3, none, 0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 0 of the 5 terms");
    expect_compressed(none, blank(3, 2));
    Compressed small = blank(3, 4);
    EXPECT_EQ(read_with(model, 0, 3, small, 4), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 4 of the 5 terms");
    expect_compressed(small, blank(3, 4));
    Compressed empty = blank(1, 0);
    EXPECT_EQ(read_with(model, 1, 1, empty, 0), BIYOS_OK);
    EXPECT_EQ(empty.starts, (std::vector<std::int64_t>{0, 0}));
    Compressed large = blank(3, 6);
    EXPECT_EQ(read_terms(model, 0, 3, large), 5);
    const Compressed whole{
        .starts = kSorted.starts,
        .rows = {0, 1, 1, 0, 2, kNoVariable},
        .columns = {0, 1, 2, 1, 2, kNoVariable},
        .values = {2.0, -0.0, -1.0, 3.0, 1.0, kNoValue},
    };
    expect_compressed(large, whole);
}

TEST(CApiTest, QuadraticRange)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Compressed read = blank(2, 2);
    EXPECT_EQ(read_terms(model, 1, 2, read), 2);
    const Compressed wanted{
        .starts = {0, 0, 2},
        .rows = {0, 2},
        .columns = {1, 2},
        .values = {3.0, 1.0},
    };
    expect_compressed(read, wanted);
    Compressed none = blank(0, 0);
    EXPECT_EQ(read_terms(model, 3, 0, none), 0);
    EXPECT_EQ(none.starts, std::vector<std::int64_t>{0});
}

TEST(CApiTest, NullPairs)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Compressed read = blank(3, 5);
    BIYOS_model *held = model.get();
    std::int64_t *starts = read.starts.data();
    int *rows = read.rows.data();
    int *columns = read.columns.data();
    double *values = read.values.data();
    EXPECT_EQ(BIYOS_getquadtermrg(held, 0, 3, 5, nullptr, rows, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getquadtermrg(held, 0, 3, 5, nullptr, nullptr, columns, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getquadtermrg(held, 0, 3, 5, starts, nullptr, nullptr, values), BIYOS_OK);
    expect_compressed(read, kSorted);
    EXPECT_EQ(BIYOS_getnquadtermrg(held, 0, 3, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
}

TEST(CApiTest, RefusedPairs)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const std::vector<std::pair<int, int>> ranges{{2, 2}, {-1, 1}, {0, -1}, {4, 0}};
    const std::vector<std::string> messages{
        "invalid argument: function 3: unknown",
        "invalid argument: function -1: unknown",
        "invalid argument: argument 'end': -1 is below 'beg' 0",
        "invalid argument: function 4: unknown",
    };
    for (std::size_t k = 0; k < ranges.size(); ++k)
    {
        Compressed read = blank(3, 5);
        const auto [start, n] = ranges[k];
        EXPECT_EQ(read_with(model, start, n, read, 5), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), messages[k]);
        expect_compressed(read, blank(3, 5));
    }
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_with(model, 0, 3, read, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': -1 is negative");
    expect_compressed(read, blank(3, 5));
}

TEST(CApiTest, LockedQuadratic)
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
    const std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG, BIYOS_ERR_BADARG, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(attempt.messages[1], attempt.messages[0]);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    expect_sample(model);
}

TEST(CApiTest, HugeQuadratic)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Compressed most{
        .starts = {0, (std::numeric_limits<std::int64_t>::max)()},
        .rows = {0, 1},
        .columns = {0, 1},
        .values = {1.0, 2.0},
    };
    Compressed large = most;
    large.starts = {0, std::int64_t{1} << 59};
    const std::string message = "out of memory: model: no quadratic term offset left";
    BIYOS_model *held = model.get();
    const int *rows = most.rows.data();
    const int *columns = most.columns.data();
    const double *values = most.values.data();
    EXPECT_EQ(BIYOS_addquadtermrg(held, 1, 2, most.starts.data(), rows, columns, values),
              BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), message);
    EXPECT_EQ(BIYOS_addquadtermrg(held, 1, 2, large.starts.data(), rows, columns, values),
              BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), message);
    expect_sample(model);
}
