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
constexpr int kNoColumn = -7;
constexpr double kNoValue = -7.5;

struct Compressed
{
    std::vector<std::int64_t> starts;
    std::vector<int> columns;
    std::vector<double> values;
};

struct Bounds
{
    std::vector<double> lower;
    std::vector<double> upper;
};

const Compressed kSample{
    .starts = {0, 3, 3, 5},
    .columns = {2, 0, 1, 1, 0},
    .values = {3.0, 1.0, 0.0, 5.0, 4.0},
};

const Compressed kSorted{
    .starts = {0, 3, 3, 5},
    .columns = {0, 1, 2, 0, 1},
    .values = {1.0, 0.0, 3.0, 4.0, 5.0},
};

void add_sample(const Model &model)
{
    ASSERT_EQ(biyos_add_variables(model.get(), 3, nullptr, nullptr, nullptr), BIYOS_OK);
    const double lower[] = {-1.0, -kInfinity, 2.0};
    const double upper[] = {1.0, kInfinity, 2.0};
    const biyos_status status = biyos_add_rows(model.get(), 3, lower, upper, kSample.starts.data(),
                                               kSample.columns.data(), kSample.values.data());
    ASSERT_EQ(status, BIYOS_OK);
}

Bounds read_bounds(const Model &model)
{
    int count = -1;
    EXPECT_EQ(biyos_row_count(model.get(), &count), BIYOS_OK);
    const auto size = static_cast<std::size_t>(count);
    Bounds read{.lower = std::vector<double>(size), .upper = std::vector<double>(size)};
    EXPECT_EQ(biyos_get_rows(model.get(), 0, count, read.lower.data(), read.upper.data()),
              BIYOS_OK);
    return read;
}

Compressed blank(std::size_t rows, std::size_t entries)
{
    return Compressed{.starts = std::vector<std::int64_t>(rows + 1, -1),
                      .columns = std::vector<int>(entries, kNoColumn),
                      .values = std::vector<double>(entries, kNoValue)};
}

std::int64_t read_entries(const Model &model, int first, int count, Compressed &read)
{
    std::int64_t needed = -1;
    const auto size = static_cast<std::int64_t>(read.columns.size());
    const biyos_status status =
        biyos_get_entries(model.get(), first, count, read.starts.data(), size, &needed,
                          read.columns.data(), read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return needed;
}

void expect_compressed(const Compressed &found, const Compressed &wanted)
{
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.columns, wanted.columns);
    expect_doubles(found.values, wanted.values);
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
    const int index = 0;
    const double bound = 0.0;
    int count = 0;
    std::int64_t needed = 0;
    note(attempt, biyos_add_rows(model, 1, nullptr, nullptr, nullptr, nullptr, nullptr));
    note(attempt, biyos_set_row_bounds(model, 1, &index, &bound, &bound));
    note(attempt, biyos_row_count(model, &count));
    note(attempt, biyos_get_rows(model, 0, 1, nullptr, nullptr));
    note(attempt, biyos_get_entries(model, 0, 1, nullptr, 0, &needed, nullptr, nullptr));
}

}

TEST(CApiTest, CompressedRows)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Bounds bounds = read_bounds(model);
    expect_doubles(bounds.lower, {-1.0, -kInfinity, 2.0});
    expect_doubles(bounds.upper, {1.0, kInfinity, 2.0});
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_entries(model, 0, 3, read), 5);
    expect_compressed(read, kSorted);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, SizeZero)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    std::int64_t needed = -1;
    EXPECT_EQ(biyos_get_entries(model.get(), 0, 3, nullptr, 0, &needed, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(needed, 5);
    Compressed read = blank(3, 2);
    needed = -1;
    EXPECT_EQ(biyos_get_entries(model.get(), 0, 3, read.starts.data(), 0, &needed,
                                read.columns.data(), read.values.data()),
              BIYOS_OK);
    EXPECT_EQ(needed, 5);
    expect_compressed(read, Compressed{.starts = kSorted.starts,
                                       .columns = {kNoColumn, kNoColumn},
                                       .values = {kNoValue, kNoValue}});
}

TEST(CApiTest, SmallSize)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Compressed read = blank(3, 4);
    const std::int64_t size = 2;
    std::int64_t needed = -1;
    EXPECT_EQ(biyos_get_entries(model.get(), 0, 3, read.starts.data(), size, &needed,
                                read.columns.data(), read.values.data()),
              BIYOS_OK);
    EXPECT_EQ(needed, 5);
    expect_compressed(read, Compressed{.starts = kSorted.starts,
                                       .columns = {0, 1, kNoColumn, kNoColumn},
                                       .values = {1.0, 0.0, kNoValue, kNoValue}});
}

TEST(CApiTest, LargeSize)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Compressed read = blank(3, 7);
    EXPECT_EQ(read_entries(model, 0, 3, read), 5);
    const Compressed wanted{
        .starts = kSorted.starts,
        .columns = {0, 1, 2, 0, 1, kNoColumn, kNoColumn},
        .values = {1.0, 0.0, 3.0, 4.0, 5.0, kNoValue, kNoValue},
    };
    expect_compressed(read, wanted);
}

TEST(CApiTest, RowRange)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Compressed read = blank(2, 2);
    EXPECT_EQ(read_entries(model, 1, 2, read), 2);
    expect_compressed(read,
                      Compressed{.starts = {0, 0, 2}, .columns = {0, 1}, .values = {4.0, 5.0}});
    Compressed none = blank(0, 0);
    EXPECT_EQ(read_entries(model, 3, 0, none), 0);
    EXPECT_EQ(none.starts, std::vector<std::int64_t>{0});
    double lower = 0.0;
    double upper = 0.0;
    EXPECT_EQ(biyos_get_rows(model.get(), 2, 1, &lower, &upper), BIYOS_OK);
    EXPECT_DOUBLE_EQ(lower, 2.0);
    EXPECT_DOUBLE_EQ(upper, 2.0);
}

TEST(CApiTest, NullRows)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(biyos_add_variables(model.get(), 2, nullptr, nullptr, nullptr), BIYOS_OK);
    const double bounds[] = {0.5, 0.75};
    const std::int64_t empty[] = {0, 0, 0};
    ASSERT_EQ(biyos_add_rows(model.get(), 2, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(biyos_add_rows(model.get(), 2, bounds, nullptr, empty, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_rows(model.get(), 2, nullptr, bounds, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_rows(model.get(), 0, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    const Bounds read = read_bounds(model);
    expect_doubles(read.lower, {-kInfinity, -kInfinity, 0.5, 0.75, -kInfinity, -kInfinity});
    expect_doubles(read.upper, {kInfinity, kInfinity, kInfinity, kInfinity, 0.5, 0.75});
    std::int64_t needed = -1;
    EXPECT_EQ(biyos_get_entries(model.get(), 0, 6, nullptr, 0, &needed, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(needed, 0);
    const std::int64_t one[] = {0, 1};
    const int column = 0;
    const double value = 1.0;
    EXPECT_EQ(biyos_add_rows(model.get(), 1, nullptr, nullptr, one, nullptr, &value),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'columns': is null");
    EXPECT_EQ(biyos_add_rows(model.get(), 1, nullptr, nullptr, one, &column, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'values': is null");
    int count = -1;
    EXPECT_EQ(biyos_row_count(model.get(), &count), BIYOS_OK);
    EXPECT_EQ(count, 6);
}

TEST(CApiTest, NullBounds)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(biyos_add_rows(model.get(), 2, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    const int indices[] = {1, 0};
    const double lower[] = {1.0, 2.0};
    const double upper[] = {3.0, 4.0};
    ASSERT_EQ(biyos_set_row_bounds(model.get(), 2, indices, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_set_row_bounds(model.get(), 1, indices, nullptr, upper), BIYOS_OK);
    ASSERT_EQ(biyos_set_row_bounds(model.get(), 2, indices, nullptr, nullptr), BIYOS_OK);
    const Bounds read = read_bounds(model);
    expect_doubles(read.lower, {2.0, 1.0});
    expect_doubles(read.upper, {kInfinity, 3.0});
    EXPECT_EQ(biyos_set_row_bounds(model.get(), 2, nullptr, lower, upper), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'indices': is null");
    EXPECT_EQ(biyos_set_row_bounds(model.get(), 0, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullEntries)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(biyos_get_rows(model.get(), 0, 3, nullptr, nullptr), BIYOS_OK);
    double high[] = {9.0, 9.0};
    EXPECT_EQ(biyos_get_rows(model.get(), 1, 2, nullptr, high), BIYOS_OK);
    expect_doubles({high[0], high[1]}, {kInfinity, 2.0});
    Compressed read = blank(3, 5);
    std::int64_t needed = -1;
    EXPECT_EQ(
        biyos_get_entries(model.get(), 0, 3, nullptr, 5, &needed, read.columns.data(), nullptr),
        BIYOS_OK);
    EXPECT_EQ(biyos_get_entries(model.get(), 0, 3, read.starts.data(), 5, &needed, nullptr,
                                read.values.data()),
              BIYOS_OK);
    expect_compressed(read, kSorted);
    EXPECT_EQ(biyos_get_entries(model.get(), 0, 3, nullptr, 0, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'needed': is null");
    EXPECT_EQ(biyos_row_count(model.get(), nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': is null");
}

TEST(CApiTest, RefusedRows)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const std::int64_t late[] = {1, 1};
    const std::int64_t falling[] = {0, 1, 0};
    const std::int64_t negative[] = {0, -1};
    const int columns[] = {0, 3};
    const double values[] = {1.0, 2.0};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(biyos_add_rows(model.get(), 1, nullptr, nullptr, late, columns, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'starts': begins at 1, not 0");
    EXPECT_EQ(biyos_add_rows(model.get(), 2, nullptr, nullptr, falling, columns, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(),
              "invalid argument: argument 'starts': decreases from 1 to 0 at entry 2");
    EXPECT_EQ(biyos_add_rows(model.get(), 1, nullptr, nullptr, negative, columns, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(),
              "invalid argument: argument 'starts': decreases from 0 to -1 at entry 1");
    const std::int64_t both[] = {0, 2};
    EXPECT_EQ(biyos_add_rows(model.get(), 1, nullptr, nullptr, both, columns, values),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: row 3: column 3 is unknown");
    EXPECT_EQ(biyos_add_rows(model.get(), 1, &nan, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: row 3: lower bound is NaN");
    EXPECT_EQ(biyos_add_rows(model.get(), -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': -1 is negative");
    const int index = 3;
    EXPECT_EQ(biyos_set_row_bounds(model.get(), 1, &index, values, values), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: row 3: unknown");
    std::int64_t needed = -1;
    EXPECT_EQ(biyos_get_entries(model.get(), 2, 2, nullptr, 0, &needed, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: row 3: unknown");
    EXPECT_EQ(biyos_get_entries(model.get(), 0, 1, nullptr, -1, &needed, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'size': -1 is negative");
    EXPECT_EQ(needed, -1);
    EXPECT_EQ(biyos_get_rows(model.get(), -1, 1, nullptr, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: row -1: unknown");
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_entries(model, 0, 3, read), 5);
    expect_compressed(read, kSorted);
}

TEST(CApiTest, LockedRows)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    ASSERT_EQ(biyos_add_rows(model.get(), 1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(biyos_subscribe(env.get(), change, &attempt, message, BIYOS_LEVEL_ERROR, &id),
              BIYOS_OK);
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
    const biyos_status refused = BIYOS_INVALID_ARGUMENT;
    const std::vector<biyos_status> statuses{refused, refused, BIYOS_OK, BIYOS_OK, BIYOS_OK};
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0], locked);
    EXPECT_EQ(attempt.messages[1], locked);
    EXPECT_EQ(biyos_unsubscribe(env.get(), id), BIYOS_OK);
    int count = 0;
    EXPECT_EQ(biyos_row_count(model.get(), &count), BIYOS_OK);
    EXPECT_EQ(count, 1);
}

TEST(CApiTest, HugeRows)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const std::int64_t most[] = {0, (std::numeric_limits<std::int64_t>::max)()};
    const std::int64_t large[] = {0, std::int64_t{1} << 59};
    const int columns[] = {0, 1};
    const double values[] = {1.0, 2.0};
    EXPECT_EQ(biyos_add_rows(model.get(), 1, nullptr, nullptr, most, columns, values),
              BIYOS_NO_MEMORY);
    EXPECT_EQ(env.message(), "out of memory: model: no entry offset left");
    EXPECT_EQ(biyos_add_rows(model.get(), 1, nullptr, nullptr, large, columns, values),
              BIYOS_NO_MEMORY);
    Compressed read = blank(3, 5);
    EXPECT_EQ(read_entries(model, 0, 3, read), 5);
    expect_compressed(read, kSorted);
}
