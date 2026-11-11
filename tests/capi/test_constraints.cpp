#include "biyos/biyos.h"
#include "doubles.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::expect_doubles;
using biyos::test::Model;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Records
{
    std::vector<int> functions;
    std::vector<double> lower;
    std::vector<double> upper;
};

const Records kSample{
    .functions = {1, 0, 1},
    .lower = {-1.0, -kInfinity, 2.0},
    .upper = {1.0, kInfinity, 2.0},
};

void add_functions(const Model &model, int count)
{
    ASSERT_EQ(biyos_add_functions(model.get(), count, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
}

void add_sample(const Model &model)
{
    add_functions(model, 2);
    const biyos_status status = biyos_add_constraints(model.get(),
                                                      3,
                                                      kSample.functions.data(),
                                                      kSample.lower.data(),
                                                      kSample.upper.data(),
                                                      nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

int constraint_count(const Model &model)
{
    int count = -1;
    EXPECT_EQ(biyos_constraint_count(model.get(), &count), BIYOS_OK);
    return count;
}

Records read_all(const Model &model)
{
    const int count = constraint_count(model);
    const auto size = static_cast<std::size_t>(count);
    Records read{
        .functions = std::vector<int>(size),
        .lower = std::vector<double>(size),
        .upper = std::vector<double>(size),
    };
    const biyos_status status = biyos_get_constraints(model.get(),
                                                      0,
                                                      count,
                                                      read.functions.data(),
                                                      read.lower.data(),
                                                      read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void expect_records(const Records &found, const Records &wanted)
{
    EXPECT_EQ(found.functions, wanted.functions);
    expect_doubles(found.lower, wanted.lower);
    expect_doubles(found.upper, wanted.upper);
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
    note(attempt, biyos_add_constraints(model, 1, &index, nullptr, nullptr, nullptr));
    note(attempt, biyos_set_constraint_bounds(model, 1, &index, &bound, &bound));
    note(attempt, biyos_constraint_count(model, &count));
    note(attempt, biyos_get_constraints(model, 0, 1, nullptr, nullptr, nullptr));
}

}

TEST(CApiTest, AddedConstraints)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(constraint_count(model), 3);
    expect_records(read_all(model), kSample);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstConstraint)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0, 0};
    int first = -1;
    ASSERT_EQ(biyos_add_constraints(model.get(), 0, nullptr, nullptr, nullptr, &first), BIYOS_OK);
    EXPECT_EQ(first, 0);
    ASSERT_EQ(biyos_add_constraints(model.get(), 2, functions, nullptr, nullptr, &first), BIYOS_OK);
    EXPECT_EQ(first, 0);
    ASSERT_EQ(biyos_add_constraints(model.get(), 3, functions, nullptr, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(biyos_add_constraints(model.get(), 1, functions, nullptr, nullptr, &first), BIYOS_OK);
    EXPECT_EQ(first, 5);
    const int unknown = 1;
    EXPECT_EQ(biyos_add_constraints(model.get(), 1, &unknown, nullptr, nullptr, &first),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(first, 5);
    EXPECT_EQ(constraint_count(model), 6);
}

TEST(CApiTest, SharedFunction)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0};
    const double lower[] = {0.0, -kInfinity};
    const double upper[] = {1.0, 3.0};
    ASSERT_EQ(biyos_add_constraints(model.get(), 2, functions, lower, upper, nullptr), BIYOS_OK);
    const Records wanted{.functions = {0, 0}, .lower = {0.0, -kInfinity}, .upper = {1.0, 3.0}};
    expect_records(read_all(model), wanted);
}

TEST(CApiTest, ConstraintRange)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    int functions[] = {-1, -1};
    double lower[] = {0.0, 0.0};
    double upper[] = {0.0, 0.0};
    EXPECT_EQ(biyos_get_constraints(model.get(), 1, 2, functions, lower, upper), BIYOS_OK);
    const Records found{
        .functions = {functions[0], functions[1]},
        .lower = {lower[0], lower[1]},
        .upper = {upper[0], upper[1]},
    };
    const Records wanted{
        .functions = {0, 1},
        .lower = {-kInfinity, 2.0},
        .upper = {kInfinity, 2.0},
    };
    expect_records(found, wanted);
    EXPECT_EQ(biyos_get_constraints(model.get(), 3, 0, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullConstraints)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0};
    const double bounds[] = {0.5, 0.75};
    biyos_model *held = model.get();
    ASSERT_EQ(biyos_add_constraints(held, 2, functions, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_constraints(held, 2, functions, bounds, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_constraints(held, 2, functions, nullptr, bounds, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_constraints(held, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Records wanted{
        .functions = {0, 0, 0, 0, 0, 0},
        .lower = {-kInfinity, -kInfinity, 0.5, 0.75, -kInfinity, -kInfinity},
        .upper = {kInfinity, kInfinity, kInfinity, kInfinity, 0.5, 0.75},
    };
    expect_records(read_all(model), wanted);
    EXPECT_EQ(biyos_add_constraints(held, 1, nullptr, bounds, bounds, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'functions': is null");
    EXPECT_EQ(constraint_count(model), 6);
}

TEST(CApiTest, NullBounds)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0};
    ASSERT_EQ(biyos_add_constraints(model.get(), 2, functions, nullptr, nullptr, nullptr),
              BIYOS_OK);
    const int indices[] = {1, 0};
    const double lower[] = {1.0, 2.0};
    const double upper[] = {3.0, 4.0};
    biyos_model *held = model.get();
    ASSERT_EQ(biyos_set_constraint_bounds(held, 2, indices, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_set_constraint_bounds(held, 1, indices, nullptr, upper), BIYOS_OK);
    ASSERT_EQ(biyos_set_constraint_bounds(held, 2, indices, nullptr, nullptr), BIYOS_OK);
    const Records wanted{.functions = {0, 0}, .lower = {2.0, 1.0}, .upper = {kInfinity, 3.0}};
    expect_records(read_all(model), wanted);
    EXPECT_EQ(biyos_set_constraint_bounds(held, 2, nullptr, lower, upper), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'indices': is null");
    EXPECT_EQ(biyos_set_constraint_bounds(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullReads)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(biyos_get_constraints(model.get(), 0, 3, nullptr, nullptr, nullptr), BIYOS_OK);
    int functions[] = {-1, -1};
    double high[] = {9.0, 9.0};
    EXPECT_EQ(biyos_get_constraints(model.get(), 1, 2, functions, nullptr, high), BIYOS_OK);
    EXPECT_EQ(std::vector<int>({functions[0], functions[1]}), std::vector<int>({0, 1}));
    expect_doubles({high[0], high[1]}, {kInfinity, 2.0});
    EXPECT_EQ(biyos_constraint_count(model.get(), nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': is null");
}

TEST(CApiTest, RefusedConstraints)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    biyos_model *held = model.get();
    const int functions[] = {0, 2};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(biyos_add_constraints(held, 2, functions, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: constraint 4: function 2 is unknown");
    EXPECT_EQ(biyos_add_constraints(held, 1, functions, &nan, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: lower bound is NaN");
    EXPECT_EQ(biyos_add_constraints(held, -1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': -1 is negative");
    const int index = 3;
    const double bound = 0.0;
    EXPECT_EQ(biyos_set_constraint_bounds(held, 1, &index, &bound, &bound), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: unknown");
    const int first = 0;
    const double infinite = kInfinity;
    EXPECT_EQ(biyos_set_constraint_bounds(held, 1, &first, &infinite, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: constraint 0: lower bound is infinity");
    EXPECT_EQ(biyos_get_constraints(held, 2, 2, nullptr, nullptr, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: unknown");
    EXPECT_EQ(biyos_get_constraints(held, -1, 1, nullptr, nullptr, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: constraint -1: unknown");
    expect_records(read_all(model), kSample);
}

TEST(CApiTest, LockedConstraints)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    add_functions(model, 1);
    const int function = 0;
    ASSERT_EQ(biyos_add_constraints(model.get(), 1, &function, nullptr, nullptr, nullptr),
              BIYOS_OK);
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(biyos_subscribe(env.get(), change, &attempt, message, BIYOS_LEVEL_ERROR, &id),
              BIYOS_OK);
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
    const biyos_status refused = BIYOS_INVALID_ARGUMENT;
    const std::vector<biyos_status> statuses{refused, refused, BIYOS_OK, BIYOS_OK};
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0], locked);
    EXPECT_EQ(attempt.messages[1], locked);
    EXPECT_EQ(biyos_unsubscribe(env.get(), id), BIYOS_OK);
    EXPECT_EQ(constraint_count(model), 1);
}
