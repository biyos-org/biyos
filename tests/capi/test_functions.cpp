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

void add_sample(const Model &model)
{
    ASSERT_EQ(biyos_add_variables(model.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const biyos_status status = biyos_add_functions(model.get(),
                                                    3,
                                                    kSample.constants.data(),
                                                    kSample.starts.data(),
                                                    kSample.variables.data(),
                                                    kSample.values.data(),
                                                    nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

Compressed blank(std::size_t functions, std::size_t terms)
{
    return Compressed{
        .constants = std::vector<double>(functions, kNoConstant),
        .starts = std::vector<std::int64_t>(functions + 1, -1),
        .variables = std::vector<int>(terms, kNoVariable),
        .values = std::vector<double>(terms, kNoValue),
    };
}

biyos_status fill(const Model &model,
                  int first,
                  int count,
                  Compressed &read,
                  std::int64_t size,
                  std::int64_t *needed)
{
    return biyos_get_functions(model.get(),
                               first,
                               count,
                               read.constants.data(),
                               read.starts.data(),
                               size,
                               needed,
                               read.variables.data(),
                               read.values.data());
}

std::int64_t read_with(const Model &model,
                       int first,
                       int count,
                       Compressed &read,
                       std::int64_t size)
{
    std::int64_t needed = -1;
    EXPECT_EQ(fill(model, first, count, read, size, &needed), BIYOS_OK);
    return needed;
}

std::int64_t read_functions(const Model &model, int first, int count, Compressed &read)
{
    return read_with(model, first, count, read, static_cast<std::int64_t>(read.variables.size()));
}

void expect_compressed(const Compressed &found, const Compressed &wanted)
{
    expect_doubles(found.constants, wanted.constants);
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.variables, wanted.variables);
    expect_doubles(found.values, wanted.values);
}

int function_count(const Model &model)
{
    int count = -1;
    EXPECT_EQ(biyos_function_count(model.get(), &count), BIYOS_OK);
    return count;
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
    int count = 0;
    std::int64_t needed = 0;
    note(attempt, biyos_add_functions(model, 1, nullptr, nullptr, nullptr, nullptr, nullptr));
    note(attempt, biyos_function_count(model, &count));
    note(attempt, biyos_get_functions(model, 0, 1, nullptr, nullptr, 0, &needed, nullptr, nullptr));
}

}

TEST(CApiTest, CompressedFunctions)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(function_count(model), 3);
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_functions(model, 0, 3, read), 5);
    expect_compressed(read, kSorted);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstFunction)
{
    const Env env;
    const Model model(env);
    int first = -1;
    ASSERT_EQ(biyos_add_functions(model.get(), 0, nullptr, nullptr, nullptr, nullptr, &first),
              BIYOS_OK);
    EXPECT_EQ(first, 0);
    ASSERT_EQ(biyos_add_functions(model.get(), 2, nullptr, nullptr, nullptr, nullptr, &first),
              BIYOS_OK);
    EXPECT_EQ(first, 0);
    ASSERT_EQ(biyos_add_functions(model.get(), 3, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(biyos_add_functions(model.get(), 1, nullptr, nullptr, nullptr, nullptr, &first),
              BIYOS_OK);
    EXPECT_EQ(first, 5);
    const double infinite = kInfinity;
    EXPECT_EQ(biyos_add_functions(model.get(), 1, &infinite, nullptr, nullptr, nullptr, &first),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(first, 5);
    EXPECT_EQ(function_count(model), 6);
}

TEST(CApiTest, SizeZero)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    std::int64_t needed = -1;
    EXPECT_EQ(
        biyos_get_functions(model.get(), 0, 3, nullptr, nullptr, 0, &needed, nullptr, nullptr),
        BIYOS_OK);
    EXPECT_EQ(needed, 5);
    Compressed read = blank(3, 2);
    EXPECT_EQ(read_with(model, 0, 3, read, 0), 5);
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
    const Model model(env);
    add_sample(model);
    Compressed read = blank(3, 4);
    EXPECT_EQ(read_with(model, 0, 3, read, 2), 5);
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
    const Model model(env);
    add_sample(model);
    Compressed read = blank(3, 7);
    EXPECT_EQ(read_functions(model, 0, 3, read), 5);
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
    const Model model(env);
    add_sample(model);
    Compressed read = blank(2, 2);
    EXPECT_EQ(read_functions(model, 1, 2, read), 2);
    const Compressed wanted{
        .constants = {0.0, -2.0},
        .starts = {0, 0, 2},
        .variables = {0, 1},
        .values = {4.0, 5.0},
    };
    expect_compressed(read, wanted);
    Compressed none = blank(0, 0);
    EXPECT_EQ(read_functions(model, 3, 0, none), 0);
    EXPECT_EQ(none.starts, std::vector<std::int64_t>{0});
}

TEST(CApiTest, NullFunctions)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(biyos_add_variables(model.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const double constants[] = {0.5, 0.75};
    const std::int64_t empty[] = {0, 0, 0};
    biyos_model *held = model.get();
    ASSERT_EQ(biyos_add_functions(held, 2, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_functions(held, 2, constants, empty, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_functions(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    Compressed read = blank(4, 0);
    EXPECT_EQ(read_functions(model, 0, 4, read), 0);
    expect_doubles(read.constants, {0.0, 0.0, 0.5, 0.75});
    EXPECT_EQ(read.starts, (std::vector<std::int64_t>{0, 0, 0, 0, 0}));
    const std::int64_t one[] = {0, 1};
    const int variable = 0;
    const double value = 1.0;
    EXPECT_EQ(biyos_add_functions(held, 1, nullptr, one, nullptr, &value, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'variables': is null");
    EXPECT_EQ(biyos_add_functions(held, 1, nullptr, one, &variable, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'values': is null");
    EXPECT_EQ(function_count(model), 4);
}

TEST(CApiTest, NullTerms)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Compressed read = blank(3, 5);
    std::int64_t needed = -1;
    biyos_model *held = model.get();
    double *constants = read.constants.data();
    std::int64_t *starts = read.starts.data();
    int *variables = read.variables.data();
    double *values = read.values.data();
    EXPECT_EQ(biyos_get_functions(held, 0, 3, nullptr, nullptr, 5, &needed, variables, nullptr),
              BIYOS_OK);
    EXPECT_EQ(biyos_get_functions(held, 0, 3, nullptr, starts, 5, &needed, nullptr, values),
              BIYOS_OK);
    EXPECT_EQ(biyos_get_functions(held, 0, 3, constants, nullptr, 0, &needed, nullptr, nullptr),
              BIYOS_OK);
    expect_compressed(read, kSorted);
    Compressed untouched = blank(3, 5);
    EXPECT_EQ(fill(model, 0, 3, untouched, 5, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'needed': is null");
    expect_compressed(untouched, blank(3, 5));
    EXPECT_EQ(biyos_function_count(held, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': is null");
}

TEST(CApiTest, RefusedFunctions)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    biyos_model *held = model.get();
    const std::int64_t late[] = {1, 1};
    const std::int64_t falling[] = {0, 1, 0};
    const std::int64_t negative[] = {0, -1};
    const int variables[] = {0, 3};
    const double values[] = {1.0, 2.0};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(biyos_add_functions(held, 1, nullptr, late, variables, values, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'starts': begins at 1, not 0");
    EXPECT_EQ(biyos_add_functions(held, 2, nullptr, falling, variables, values, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(),
              "invalid argument: argument 'starts': decreases from 1 to 0 at entry 2");
    EXPECT_EQ(biyos_add_functions(held, 1, nullptr, negative, variables, values, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(),
              "invalid argument: argument 'starts': decreases from 0 to -1 at entry 1");
    const std::int64_t both[] = {0, 2};
    EXPECT_EQ(biyos_add_functions(held, 1, nullptr, both, variables, values, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function 3: variable 3 is unknown");
    EXPECT_EQ(biyos_add_functions(held, 1, &nan, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function 3: constant is NaN");
    EXPECT_EQ(biyos_add_functions(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': -1 is negative");
    std::int64_t needed = -1;
    Compressed untouched = blank(3, 5);
    EXPECT_EQ(fill(model, 2, 2, untouched, 5, &needed), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function 3: unknown");
    EXPECT_EQ(fill(model, -1, 1, untouched, 5, &needed), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: function -1: unknown");
    EXPECT_EQ(fill(model, 0, 1, untouched, -1, &needed), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'size': -1 is negative");
    EXPECT_EQ(needed, -1);
    expect_compressed(untouched, blank(3, 5));
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_functions(model, 0, 3, read), 5);
    expect_compressed(read, kSorted);
}

TEST(CApiTest, LockedFunctions)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    ASSERT_EQ(biyos_add_functions(model.get(), 1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(biyos_subscribe(env.get(), change, &attempt, message, BIYOS_LEVEL_ERROR, &id),
              BIYOS_OK);
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
    const std::vector<biyos_status> statuses{BIYOS_INVALID_ARGUMENT, BIYOS_OK, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(biyos_unsubscribe(env.get(), id), BIYOS_OK);
    EXPECT_EQ(function_count(model), 1);
}

TEST(CApiTest, HugeFunctions)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    biyos_model *held = model.get();
    const std::int64_t most[] = {0, (std::numeric_limits<std::int64_t>::max)()};
    const std::int64_t large[] = {0, std::int64_t{1} << 59};
    const int variables[] = {0, 1};
    const double values[] = {1.0, 2.0};
    EXPECT_EQ(biyos_add_functions(held, 1, nullptr, most, variables, values, nullptr),
              BIYOS_NO_MEMORY);
    EXPECT_EQ(env.message(), "out of memory: model: no term offset left");
    EXPECT_EQ(biyos_add_functions(held, 1, nullptr, large, variables, values, nullptr),
              BIYOS_NO_MEMORY);
    EXPECT_EQ(env.message(), "out of memory: model: no term offset left");
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_functions(model, 0, 3, read), 5);
    expect_compressed(read, kSorted);
}
