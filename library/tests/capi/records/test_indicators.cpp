#include "biyos/biyos.h"
#include "doubles.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::expect_doubles;
using biyos::test::Model;
struct Records
{
    std::vector<int> variables;
    std::vector<int> values;
    std::vector<int> functions;
    std::vector<double> lower;
    std::vector<double> upper;
};

const Records kSample{
    .variables = {0, 2, 0},
    .values = {1, 0, 0},
    .functions = {1, 1, 0},
    .lower = {-BIYOS_INF, 1.0, 0.0},
    .upper = {4.0, 1.0, BIYOS_INF},
};

void add_parts(const Model &model)
{
    const int kinds[] = {BIYOS_VARKND_BIN, BIYOS_VARKND_CONT, BIYOS_VARKND_BIN};
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, kinds, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 2, nullptr), BIYOS_OK);
}

void add_sample(const Model &model)
{
    add_parts(model);
    const BIYOS_status status = BIYOS_addind(model.get(),
                                             3,
                                             kSample.variables.data(),
                                             kSample.values.data(),
                                             kSample.functions.data(),
                                             kSample.lower.data(),
                                             kSample.upper.data(),
                                             nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

int n_indicators(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnind(model.get(), &n), BIYOS_OK);
    return n;
}

Records sized(std::size_t size)
{
    return Records{
        .variables = std::vector<int>(size, -1),
        .values = std::vector<int>(size, -1),
        .functions = std::vector<int>(size, -1),
        .lower = std::vector<double>(size, 9.0),
        .upper = std::vector<double>(size, 9.0),
    };
}

Records read_all(const Model &model)
{
    const int n = n_indicators(model);
    Records read = sized(static_cast<std::size_t>(n));
    const BIYOS_status status = BIYOS_getindrg(model.get(),
                                               0,
                                               n,
                                               read.variables.data(),
                                               read.values.data(),
                                               read.functions.data(),
                                               read.lower.data(),
                                               read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Records read_listed(const Model &model, const std::vector<BIYOS_index> &indices)
{
    const auto n = static_cast<BIYOS_index>(indices.size());
    Records read = sized(indices.size());
    const BIYOS_status status = BIYOS_getindsp(model.get(),
                                               n,
                                               indices.data(),
                                               read.variables.data(),
                                               read.values.data(),
                                               read.functions.data(),
                                               read.lower.data(),
                                               read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void expect_records(const Records &found, const Records &wanted)
{
    EXPECT_EQ(found.variables, wanted.variables);
    EXPECT_EQ(found.values, wanted.values);
    EXPECT_EQ(found.functions, wanted.functions);
    expect_doubles(found.lower, wanted.lower);
    expect_doubles(found.upper, wanted.upper);
}

}

TEST(CApiTest, AddedIndicators)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(n_indicators(model), 3);
    expect_records(read_all(model), kSample);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstIndicator)
{
    const Env env;
    const Model model(env);
    add_parts(model);
    const int variables[] = {0, 2};
    const int values[] = {1, 1};
    const int functions[] = {0, 1};
    BIYOS_model *held = model.get();
    BIYOS_index start = -1;
    ASSERT_EQ(BIYOS_addind(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addind(held, 2, variables, values, functions, nullptr, nullptr, &start),
              BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addind(held, 1, variables + 1, values, functions, nullptr, nullptr, &start),
              BIYOS_OK);
    EXPECT_EQ(start, 2);
    const int continuous = 1;
    EXPECT_EQ(BIYOS_addind(held, 1, &continuous, values, functions, nullptr, nullptr, &start),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(start, 2);
    EXPECT_EQ(n_indicators(model), 3);
}

TEST(CApiTest, NullIndicators)
{
    const Env env;
    const Model model(env);
    add_parts(model);
    const int variables[] = {0, 2};
    const int values[] = {1, 0};
    const int functions[] = {0, 0};
    const double bounds[] = {0.5, 0.75};
    BIYOS_model *held = model.get();
    ASSERT_EQ(BIYOS_addind(held, 2, variables, values, functions, bounds, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_addind(held, 2, variables, values, functions, nullptr, bounds, nullptr),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_addind(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    const Records wanted{
        .variables = {0, 2, 0, 2},
        .values = {1, 0, 1, 0},
        .functions = {0, 0, 0, 0},
        .lower = {0.5, 0.75, -BIYOS_INF, -BIYOS_INF},
        .upper = {BIYOS_INF, BIYOS_INF, 0.5, 0.75},
    };
    expect_records(read_all(model), wanted);
    EXPECT_EQ(BIYOS_addind(held, 1, nullptr, values, functions, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'var': is null");
    EXPECT_EQ(BIYOS_addind(held, 1, variables, nullptr, functions, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'val': is null");
    EXPECT_EQ(BIYOS_addind(held, 1, variables, values, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'fun': is null");
    EXPECT_EQ(n_indicators(model), 4);
}

TEST(CApiTest, IndicatorRange)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Records found = sized(2);
    EXPECT_EQ(BIYOS_getindrg(model.get(),
                             1,
                             3,
                             found.variables.data(),
                             found.values.data(),
                             found.functions.data(),
                             found.lower.data(),
                             found.upper.data()),
              BIYOS_OK);
    const Records wanted{
        .variables = {2, 0},
        .values = {0, 0},
        .functions = {1, 0},
        .lower = {1.0, 0.0},
        .upper = {1.0, BIYOS_INF},
    };
    expect_records(found, wanted);
    EXPECT_EQ(BIYOS_getindrg(model.get(), 3, 3, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    const Records listed{
        .variables = {0, 0},
        .values = {0, 1},
        .functions = {0, 1},
        .lower = {0.0, -BIYOS_INF},
        .upper = {BIYOS_INF, 4.0},
    };
    expect_records(read_listed(model, {2, 0}), listed);
}

TEST(CApiTest, IndicatorBounds)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const BIYOS_index indices[] = {2, 0};
    const double lower[] = {-1.0, -2.0};
    const double upper[] = {1.0, 2.0};
    ASSERT_EQ(BIYOS_setindbndsp(held, 2, indices, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setindbndrg(held, 1, 2, nullptr, upper + 1), BIYOS_OK);
    ASSERT_EQ(BIYOS_setindbndrg(held, 0, 3, nullptr, nullptr), BIYOS_OK);
    std::vector<double> low(3, 9.0);
    std::vector<double> high(3, 9.0);
    ASSERT_EQ(BIYOS_getindbndrg(held, 0, 3, low.data(), high.data()), BIYOS_OK);
    expect_doubles(low, {-2.0, 1.0, -1.0});
    expect_doubles(high, {4.0, 2.0, BIYOS_INF});
    std::vector<double> listed(2, 9.0);
    ASSERT_EQ(BIYOS_getindbndsp(held, 2, indices, nullptr, listed.data()), BIYOS_OK);
    expect_doubles(listed, {BIYOS_INF, 4.0});
    const double infinite = BIYOS_INF;
    EXPECT_EQ(BIYOS_setindbndsp(held, 1, indices, &infinite, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: indicator 2: lower bound is infinity");
}

TEST(CApiTest, IndicatorSwitch)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const BIYOS_index indices[] = {1};
    const int variables[] = {0, 2};
    const int values[] = {1, 0};
    ASSERT_EQ(BIYOS_setindvarsp(held, 1, indices, variables, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setindvarrg(held, 2, 3, variables + 1, values), BIYOS_OK);
    std::vector<int> found(3, -1);
    std::vector<int> active(3, -1);
    ASSERT_EQ(BIYOS_getindvarrg(held, 0, 3, found.data(), active.data()), BIYOS_OK);
    EXPECT_EQ(found, std::vector<int>({0, 0, 2}));
    EXPECT_EQ(active, std::vector<int>({1, 0, 1}));
    std::vector<int> listed(1, -1);
    ASSERT_EQ(BIYOS_getindvarsp(held, 1, indices, nullptr, listed.data()), BIYOS_OK);
    EXPECT_EQ(listed, std::vector<int>({0}));
    const int continuous = 1;
    EXPECT_EQ(BIYOS_setindvarsp(held, 1, indices, &continuous, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: indicator 1: variable 1 is not binary");
    const int value = 2;
    EXPECT_EQ(BIYOS_setindvarrg(held, 0, 1, nullptr, &value), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: indicator 0: value 2 is neither 0 nor 1");
    EXPECT_EQ(found, read_all(model).variables);
}

TEST(CApiTest, KindInUse)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const BIYOS_index index = 2;
    const BIYOS_int continuous = BIYOS_VARKND_CONT;
    EXPECT_EQ(BIYOS_setvarkndsp(held, 1, &index, &continuous), BIYOS_ERR_INUSE);
    EXPECT_EQ(env.message(), "in use: variable 2: indicator 1 needs it binary");
    const BIYOS_int kinds[] = {BIYOS_VARKND_CONT, BIYOS_VARKND_INT};
    EXPECT_EQ(BIYOS_setvarkndrg(held, 0, 2, kinds), BIYOS_ERR_INUSE);
    EXPECT_EQ(env.message(), "in use: variable 0: indicator 0 needs it binary");
    BIYOS_int kind = -1;
    ASSERT_EQ(BIYOS_getvarkndsp(held, 1, &index, &kind), BIYOS_OK);
    EXPECT_EQ(kind, BIYOS_VARKND_BIN);
    const BIYOS_index spare = 1;
    const BIYOS_int integer = BIYOS_VARKND_INT;
    EXPECT_EQ(BIYOS_setvarkndsp(held, 1, &spare, &integer), BIYOS_OK);
}

TEST(CApiTest, RefusedIndicators)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const int variables[] = {0, 3};
    const int values[] = {1, 1};
    const int functions[] = {0, 2};
    const BIYOS_real nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(BIYOS_addind(held, 2, variables, values, functions, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: indicator 4: variable 3 is unknown");
    EXPECT_EQ(BIYOS_addind(held, 1, variables, values, functions + 1, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: indicator 3: function 2 is unknown");
    EXPECT_EQ(BIYOS_addind(held, 1, variables, values, functions, &nan, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: indicator 3: lower bound is NaN");
    EXPECT_EQ(BIYOS_addind(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    const BIYOS_index index = 3;
    const BIYOS_real bound = 0.0;
    EXPECT_EQ(BIYOS_setindbndsp(held, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: indicator 3: unknown");
    EXPECT_EQ(BIYOS_getindbndrg(held, 2, 4, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: indicator 3: unknown");
    EXPECT_EQ(BIYOS_getindvarsp(held, 1, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_getnind(held, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
    expect_records(read_all(model), kSample);
}
