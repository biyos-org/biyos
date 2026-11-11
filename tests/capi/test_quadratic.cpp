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

biyos_status add_terms(const Model &model,
                       const std::vector<int> &functions,
                       const Compressed &terms)
{
    return biyos_add_quadratic_terms(model.get(),
                                     static_cast<int>(functions.size()),
                                     functions.data(),
                                     terms.starts.data(),
                                     terms.firsts.data(),
                                     terms.seconds.data(),
                                     terms.values.data());
}

void add_sample(const Model &model)
{
    biyos_model *held = model.get();
    ASSERT_EQ(biyos_add_variables(held, 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_functions(held, 3, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(add_terms(model, kFunctions, kSample), BIYOS_OK);
}

Compressed blank(std::size_t functions, std::size_t terms)
{
    return Compressed{
        .starts = std::vector<std::int64_t>(functions + 1, -1),
        .firsts = std::vector<int>(terms, kNoVariable),
        .seconds = std::vector<int>(terms, kNoVariable),
        .values = std::vector<double>(terms, kNoValue),
    };
}

std::int64_t read_with(const Model &model,
                       int first,
                       int count,
                       Compressed &read,
                       std::int64_t size)
{
    std::int64_t needed = -1;
    const biyos_status status = biyos_get_quadratic_terms(model.get(),
                                                          first,
                                                          count,
                                                          read.starts.data(),
                                                          size,
                                                          &needed,
                                                          read.firsts.data(),
                                                          read.seconds.data(),
                                                          read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return needed;
}

std::int64_t read_terms(const Model &model, int first, int count, Compressed &read)
{
    return read_with(model, first, count, read, static_cast<std::int64_t>(read.firsts.size()));
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

void expect_sample(const Model &model)
{
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_terms(model, 0, 3, read), 5);
    expect_compressed(read, kSorted);
}

struct Attempt
{
    biyos_env *env = nullptr;
    biyos_model *model = nullptr;
    std::vector<biyos_status> statuses;
    std::vector<std::string> messages;
};

void note(Attempt &attempt, biyos_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(biyos_last_error_message(attempt.env));
}

void change(const biyos_event *, void *user_data)
{
    Attempt &attempt = *static_cast<Attempt *>(user_data);
    biyos_model *model = attempt.model;
    const int function = 0;
    std::int64_t needed = 0;
    note(attempt,
         biyos_add_quadratic_terms(model, 1, &function, nullptr, nullptr, nullptr, nullptr));
    note(attempt,
         biyos_get_quadratic_terms(model, 0, 1, nullptr, 0, &needed, nullptr, nullptr, nullptr));
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
        .firsts = {2, 1, 1},
        .seconds = {2, 0, 2},
        .values = {0.5, 0.25, 4.0},
    };
    ASSERT_EQ(add_terms(model, {0, 2}, more), BIYOS_OK);
    Compressed read = blank(3, 7);
    EXPECT_EQ(read_terms(model, 0, 3, read), 7);
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
    const Model model(env);
    add_sample(model);
    std::int64_t needed = -1;
    biyos_model *held = model.get();
    EXPECT_EQ(biyos_get_quadratic_terms(held, 0, 3, nullptr, 0, &needed, nullptr, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(needed, 5);
    Compressed none = blank(3, 2);
    EXPECT_EQ(read_with(model, 0, 3, none, 0), 5);
    Compressed unread = blank(3, 2);
    unread.starts = kSorted.starts;
    expect_compressed(none, unread);
    Compressed small = blank(3, 4);
    EXPECT_EQ(read_with(model, 0, 3, small, 2), 5);
    const Compressed cut{
        .starts = kSorted.starts,
        .firsts = {0, 1, kNoVariable, kNoVariable},
        .seconds = {0, 1, kNoVariable, kNoVariable},
        .values = {2.0, -0.0, kNoValue, kNoValue},
    };
    expect_compressed(small, cut);
    Compressed large = blank(3, 6);
    EXPECT_EQ(read_terms(model, 0, 3, large), 5);
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
    const Model model(env);
    add_sample(model);
    Compressed read = blank(2, 2);
    EXPECT_EQ(read_terms(model, 1, 2, read), 2);
    const Compressed wanted{
        .starts = {0, 0, 2},
        .firsts = {0, 2},
        .seconds = {1, 2},
        .values = {3.0, 1.0},
    };
    expect_compressed(read, wanted);
    Compressed none = blank(0, 0);
    EXPECT_EQ(read_terms(model, 3, 0, none), 0);
    EXPECT_EQ(none.starts, std::vector<std::int64_t>{0});
}

TEST(CApiTest, NullQuadratic)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    biyos_model *held = model.get();
    const int functions[] = {1, 2};
    const std::int64_t one[] = {0, 1};
    const int variable = 0;
    const double value = 1.0;
    EXPECT_EQ(biyos_add_quadratic_terms(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(biyos_add_quadratic_terms(held, 2, functions, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, nullptr, one, &variable, &variable, &value),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'functions': is null");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, one, nullptr, &variable, &value),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'firsts': is null");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, one, &variable, nullptr, &value),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'seconds': is null");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, one, &variable, &variable, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'values': is null");
    expect_sample(model);
}

TEST(CApiTest, NullPairs)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Compressed read = blank(3, 5);
    std::int64_t needed = -1;
    biyos_model *held = model.get();
    std::int64_t *starts = read.starts.data();
    int *firsts = read.firsts.data();
    int *seconds = read.seconds.data();
    double *values = read.values.data();
    EXPECT_EQ(biyos_get_quadratic_terms(held, 0, 3, nullptr, 5, &needed, firsts, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(biyos_get_quadratic_terms(held, 0, 3, nullptr, 5, &needed, nullptr, seconds, nullptr),
              BIYOS_OK);
    EXPECT_EQ(biyos_get_quadratic_terms(held, 0, 3, starts, 5, &needed, nullptr, nullptr, values),
              BIYOS_OK);
    expect_compressed(read, kSorted);
    Compressed kept = blank(3, 5);
    starts = kept.starts.data();
    firsts = kept.firsts.data();
    EXPECT_EQ(biyos_get_quadratic_terms(held, 0, 3, starts, 5, nullptr, firsts, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'needed': is null");
    expect_compressed(kept, blank(3, 5));
}

TEST(CApiTest, RefusedQuadratic)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    biyos_model *held = model.get();
    const int functions[] = {0, 1};
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
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, late, firsts, seconds, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'starts': begins at 1, not 0");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 2, functions, falling, firsts, seconds, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(),
              "invalid argument: argument 'starts': decreases from 1 to 0 at entry 2");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, negative, firsts, seconds, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(),
              "invalid argument: argument 'starts': decreases from 0 to -1 at entry 1");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, unknown, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function 3: unknown");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 2, repeated, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'functions': function 1 is repeated");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, both, firsts, seconds, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function 0: variable 3 is unknown");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, both, seconds, firsts, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function 0: variable 3 is unknown");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, one, firsts, seconds, &nan),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function 0: coefficient of pair (1, 0) is NaN");
    EXPECT_EQ(biyos_add_quadratic_terms(held, 1, functions, both, crossed, seconds, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function 0: pair (0, 1) is repeated");
    EXPECT_EQ(biyos_add_quadratic_terms(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': -1 is negative");
    expect_sample(model);
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
        "invalid argument: argument 'count': -1 is negative",
        "invalid argument: function 4: unknown",
    };
    for (std::size_t k = 0; k < ranges.size(); ++k)
    {
        Compressed read = blank(3, 5);
        std::int64_t needed = -1;
        const auto [first, count] = ranges[k];
        EXPECT_EQ(biyos_get_quadratic_terms(model.get(),
                                            first,
                                            count,
                                            read.starts.data(),
                                            5,
                                            &needed,
                                            read.firsts.data(),
                                            read.seconds.data(),
                                            read.values.data()),
                  BIYOS_INVALID_ARGUMENT);
        EXPECT_EQ(env.message(), messages[k]);
        EXPECT_EQ(needed, -1);
        expect_compressed(read, blank(3, 5));
    }
    Compressed read = blank(3, 5);
    std::int64_t needed = -1;
    EXPECT_EQ(biyos_get_quadratic_terms(model.get(),
                                        0,
                                        3,
                                        read.starts.data(),
                                        -1,
                                        &needed,
                                        read.firsts.data(),
                                        read.seconds.data(),
                                        read.values.data()),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'size': -1 is negative");
    EXPECT_EQ(needed, -1);
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
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(biyos_subscribe(env.get(), change, &attempt, message, BIYOS_LEVEL_ERROR, &id),
              BIYOS_OK);
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
    const std::vector<biyos_status> statuses{BIYOS_INVALID_ARGUMENT, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(biyos_unsubscribe(env.get(), id), BIYOS_OK);
    expect_sample(model);
}

TEST(CApiTest, HugeQuadratic)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const std::vector<int> function{1};
    const Compressed most{
        .starts = {0, (std::numeric_limits<std::int64_t>::max)()},
        .firsts = {0, 1},
        .seconds = {0, 1},
        .values = {1.0, 2.0},
    };
    Compressed large = most;
    large.starts = {0, std::int64_t{1} << 59};
    const std::string message = "out of memory: model: no quadratic term offset left";
    EXPECT_EQ(add_terms(model, function, most), BIYOS_NO_MEMORY);
    EXPECT_EQ(env.message(), message);
    EXPECT_EQ(add_terms(model, function, large), BIYOS_NO_MEMORY);
    EXPECT_EQ(env.message(), message);
    expect_sample(model);
}
