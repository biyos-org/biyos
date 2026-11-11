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

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

struct Records
{
    std::vector<int> functions;
    std::vector<int> senses;
    std::vector<int> priorities;
    std::vector<double> weights;
    std::vector<double> absolute;
    std::vector<double> relative;
};

struct Fields
{
    const int *senses = nullptr;
    const int *priorities = nullptr;
    const double *weights = nullptr;
    const double *absolute = nullptr;
    const double *relative = nullptr;
};

struct Outputs
{
    int *functions = nullptr;
    int *senses = nullptr;
    int *priorities = nullptr;
    double *weights = nullptr;
    double *absolute = nullptr;
    double *relative = nullptr;
};

const Records kSample{
    .functions = {1, 0, 1},
    .senses = {BIYOS_MAXIMIZE, BIYOS_MINIMIZE, BIYOS_MINIMIZE},
    .priorities = {2, -1, 2},
    .weights = {0.5, 1.0, 3.0},
    .absolute = {0.0, 1.5, kInfinity},
    .relative = {0.25, 0.0, 0.0},
};

biyos_status add_with(biyos_model *model,
                      int count,
                      const int *functions,
                      const Fields &fields,
                      int *first = nullptr)
{
    return biyos_add_objectives(model,
                                count,
                                functions,
                                fields.senses,
                                fields.priorities,
                                fields.weights,
                                fields.absolute,
                                fields.relative,
                                first);
}

biyos_status set_with(biyos_model *model, int count, const int *indices, const Fields &fields)
{
    return biyos_set_objectives(model,
                                count,
                                indices,
                                fields.senses,
                                fields.priorities,
                                fields.weights,
                                fields.absolute,
                                fields.relative);
}

biyos_status get_with(biyos_model *model, int first, int count, const Outputs &outputs)
{
    return biyos_get_objectives(model,
                                first,
                                count,
                                outputs.functions,
                                outputs.senses,
                                outputs.priorities,
                                outputs.weights,
                                outputs.absolute,
                                outputs.relative);
}

biyos_status add_records(const Model &model, const Records &records, int *first = nullptr)
{
    const Fields fields{
        .senses = records.senses.data(),
        .priorities = records.priorities.data(),
        .weights = records.weights.data(),
        .absolute = records.absolute.data(),
        .relative = records.relative.data(),
    };
    const auto count = static_cast<int>(records.functions.size());
    return add_with(model.get(), count, records.functions.data(), fields, first);
}

void add_functions(const Model &model, int count)
{
    ASSERT_EQ(biyos_add_functions(model.get(), count, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
}

void add_sample(const Model &model)
{
    add_functions(model, 2);
    ASSERT_EQ(add_records(model, kSample), BIYOS_OK);
}

int objective_count(const Model &model)
{
    int count = -1;
    EXPECT_EQ(biyos_objective_count(model.get(), &count), BIYOS_OK);
    return count;
}

Records read_range(const Model &model, int first, int count)
{
    const auto size = static_cast<std::size_t>(count);
    Records read{
        .functions = std::vector<int>(size, -9),
        .senses = std::vector<int>(size, -9),
        .priorities = std::vector<int>(size, -9),
        .weights = std::vector<double>(size, -9.0),
        .absolute = std::vector<double>(size, -9.0),
        .relative = std::vector<double>(size, -9.0),
    };
    const Outputs outputs{
        .functions = read.functions.data(),
        .senses = read.senses.data(),
        .priorities = read.priorities.data(),
        .weights = read.weights.data(),
        .absolute = read.absolute.data(),
        .relative = read.relative.data(),
    };
    EXPECT_EQ(get_with(model.get(), first, count, outputs), BIYOS_OK);
    return read;
}

Records read_all(const Model &model)
{
    return read_range(model, 0, objective_count(model));
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

void expect_records(const Records &found, const Records &wanted)
{
    EXPECT_EQ(found.functions, wanted.functions);
    EXPECT_EQ(found.senses, wanted.senses);
    EXPECT_EQ(found.priorities, wanted.priorities);
    EXPECT_EQ(bits_of(found.weights), bits_of(wanted.weights));
    EXPECT_EQ(bits_of(found.absolute), bits_of(wanted.absolute));
    EXPECT_EQ(bits_of(found.relative), bits_of(wanted.relative));
}

Records defaults(const std::vector<int> &functions)
{
    const std::size_t size = functions.size();
    Records records{
        .functions = functions,
        .senses = std::vector<int>(size, BIYOS_MINIMIZE),
        .priorities = std::vector<int>(size),
        .weights = std::vector<double>(size, 1.0),
        .absolute = std::vector<double>(size, 0.0),
        .relative = std::vector<double>(size, 0.0),
    };
    for (std::size_t k = 0; k < size; ++k)
    {
        records.priorities[k] = static_cast<int>(k);
    }
    return records;
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
    const double weight = 2.0;
    int count = 0;
    note(attempt, add_with(model, 1, &index, {}));
    note(attempt, set_with(model, 1, &index, {.weights = &weight}));
    note(attempt, biyos_objective_count(model, &count));
    note(attempt, get_with(model, 0, 1, {}));
}

}

TEST(CApiTest, AddedObjectives)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(objective_count(model), 3);
    expect_records(read_all(model), kSample);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstObjective)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0, 0};
    int first = -1;
    ASSERT_EQ(add_with(model.get(), 0, nullptr, {}, &first), BIYOS_OK);
    EXPECT_EQ(first, 0);
    ASSERT_EQ(add_records(model, defaults({0, 0}), &first), BIYOS_OK);
    EXPECT_EQ(first, 0);
    ASSERT_EQ(add_with(model.get(), 3, functions, {}), BIYOS_OK);
    ASSERT_EQ(add_with(model.get(), 0, functions, {}, &first), BIYOS_OK);
    EXPECT_EQ(first, 5);
    ASSERT_EQ(add_records(model, defaults({0}), &first), BIYOS_OK);
    EXPECT_EQ(first, 5);
    EXPECT_EQ(add_records(model, defaults({1}), &first), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(first, 5);
    EXPECT_EQ(objective_count(model), 6);
}

TEST(CApiTest, DefaultObjectives)
{
    const Env env;
    const Model model(env);
    add_functions(model, 2);
    const int functions[] = {1, 0, 1};
    ASSERT_EQ(add_with(model.get(), 2, functions, {}), BIYOS_OK);
    expect_records(read_all(model), defaults({1, 0}));
    const int priorities[] = {-4, 0};
    ASSERT_EQ(add_with(model.get(), 2, functions, {.priorities = priorities}), BIYOS_OK);
    ASSERT_EQ(add_with(model.get(), 3, functions, {}), BIYOS_OK);
    Records wanted = defaults({1, 0, 1, 0, 1, 0, 1});
    wanted.priorities[2] = -4;
    wanted.priorities[3] = 0;
    expect_records(read_all(model), wanted);
}

TEST(CApiTest, NullObjectives)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0};
    const int senses[] = {BIYOS_MAXIMIZE, BIYOS_MINIMIZE};
    const int priorities[] = {7, 7};
    const double values[] = {0.5, 2.0};
    biyos_model *held = model.get();
    ASSERT_EQ(add_with(held, 2, functions, {.senses = senses}), BIYOS_OK);
    ASSERT_EQ(add_with(held, 2, functions, {.priorities = priorities}), BIYOS_OK);
    ASSERT_EQ(add_with(held, 2, functions, {.weights = values}), BIYOS_OK);
    ASSERT_EQ(add_with(held, 2, functions, {.absolute = values}), BIYOS_OK);
    ASSERT_EQ(add_with(held, 2, functions, {.relative = values}), BIYOS_OK);
    Records wanted = defaults(std::vector<int>(10, 0));
    wanted.senses[0] = BIYOS_MAXIMIZE;
    wanted.priorities[2] = 7;
    wanted.priorities[3] = 7;
    wanted.weights[4] = 0.5;
    wanted.weights[5] = 2.0;
    wanted.absolute[6] = 0.5;
    wanted.absolute[7] = 2.0;
    wanted.relative[8] = 0.5;
    wanted.relative[9] = 2.0;
    expect_records(read_all(model), wanted);
    EXPECT_EQ(add_with(held, 1, nullptr, {.senses = senses}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'functions': is null");
    EXPECT_EQ(objective_count(model), 10);
}

TEST(CApiTest, NullSetters)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    biyos_model *held = model.get();
    const int indices[] = {2, 0};
    const int senses[] = {BIYOS_MAXIMIZE, BIYOS_MINIMIZE};
    const int priorities[] = {-5, 6};
    const double weights[] = {4.0, 8.0};
    const double absolute[] = {kInfinity, 0.125};
    const double relative[] = {1.0, 0.75};
    EXPECT_EQ(set_with(held, 2, indices, {.senses = senses}), BIYOS_OK);
    EXPECT_EQ(set_with(held, 2, indices, {.priorities = priorities}), BIYOS_OK);
    EXPECT_EQ(set_with(held, 1, indices, {.weights = weights}), BIYOS_OK);
    EXPECT_EQ(set_with(held, 2, indices, {.absolute = absolute}), BIYOS_OK);
    EXPECT_EQ(set_with(held, 1, indices, {.relative = relative}), BIYOS_OK);
    EXPECT_EQ(set_with(held, 2, indices, {}), BIYOS_OK);
    const Records wanted{
        .functions = kSample.functions,
        .senses = {BIYOS_MINIMIZE, BIYOS_MINIMIZE, BIYOS_MAXIMIZE},
        .priorities = {6, -1, -5},
        .weights = {0.5, 1.0, 4.0},
        .absolute = {0.125, 1.5, kInfinity},
        .relative = {0.25, 0.0, 1.0},
    };
    expect_records(read_all(model), wanted);
    EXPECT_EQ(set_with(held, 2, nullptr, {.senses = senses}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'indices': is null");
    EXPECT_EQ(set_with(held, 0, nullptr, {}), BIYOS_OK);
    expect_records(read_all(model), wanted);
}

TEST(CApiTest, RepeatedObjective)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const int indices[] = {1, 1};
    const int senses[] = {BIYOS_MINIMIZE, BIYOS_MAXIMIZE};
    const double weights[] = {2.0, 3.0};
    ASSERT_EQ(set_with(model.get(), 2, indices, {.senses = senses, .weights = weights}), BIYOS_OK);
    Records wanted = kSample;
    wanted.senses[1] = BIYOS_MAXIMIZE;
    wanted.weights[1] = 3.0;
    expect_records(read_all(model), wanted);
}

TEST(CApiTest, ObjectiveRange)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Records wanted{
        .functions = {0, 1},
        .senses = {BIYOS_MINIMIZE, BIYOS_MINIMIZE},
        .priorities = {-1, 2},
        .weights = {1.0, 3.0},
        .absolute = {1.5, kInfinity},
        .relative = {0.0, 0.0},
    };
    expect_records(read_range(model, 1, 2), wanted);
    expect_records(read_range(model, 3, 0), Records{});
}

TEST(CApiTest, NullOutputs)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    biyos_model *held = model.get();
    EXPECT_EQ(get_with(held, 0, 3, {}), BIYOS_OK);
    int senses[] = {-1, -1};
    double absolute[] = {9.0, 9.0};
    EXPECT_EQ(get_with(held, 1, 2, {.senses = senses, .absolute = absolute}), BIYOS_OK);
    EXPECT_EQ(std::vector<int>({senses[0], senses[1]}), std::vector<int>(2, BIYOS_MINIMIZE));
    EXPECT_EQ(bits_of({absolute[0], absolute[1]}), bits_of({1.5, kInfinity}));
    EXPECT_EQ(biyos_objective_count(held, nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': is null");
}

TEST(CApiTest, RefusedObjectives)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    biyos_model *held = model.get();
    const int functions[] = {0, 2};
    EXPECT_EQ(add_with(held, 2, functions, {}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 4: function 2 is unknown");
    const int sense = 3;
    EXPECT_EQ(add_with(held, 1, functions, {.senses = &sense}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: sense 3 is unknown");
    const double negative = -1.0;
    EXPECT_EQ(add_with(held, 1, functions, {.weights = &negative}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: weight -1 is not positive");
    EXPECT_EQ(add_with(held, 1, functions, {.absolute = &kNan}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: absolute tolerance is NaN");
    EXPECT_EQ(add_with(held, -1, nullptr, {}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'count': -1 is negative");
    const int unknown = 3;
    EXPECT_EQ(set_with(held, 1, &unknown, {}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: unknown");
    const int zero = 0;
    EXPECT_EQ(set_with(held, 1, &zero, {.senses = &zero}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: sense 0 is unknown");
    EXPECT_EQ(set_with(held, 1, &zero, {.relative = &negative}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: relative tolerance -1 is negative");
    EXPECT_EQ(set_with(held, 1, &zero, {.weights = &kInfinity}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: weight is infinity");
    EXPECT_EQ(get_with(held, 2, 2, {}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: unknown");
    EXPECT_EQ(get_with(held, -1, 1, {}), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: objective -1: unknown");
    expect_records(read_all(model), kSample);
}

TEST(CApiTest, LockedObjectives)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    add_functions(model, 1);
    const int function = 0;
    ASSERT_EQ(add_with(model.get(), 1, &function, {}), BIYOS_OK);
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
    expect_records(read_all(model), defaults({0}));
}
