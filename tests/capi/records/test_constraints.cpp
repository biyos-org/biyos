#include "biyos/biyos.h"
#include "doubles.hpp"
#include "handles.hpp"
#include "tolerance.hpp"

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
struct Records
{
    std::vector<int> functions;
    std::vector<double> lower;
    std::vector<double> upper;
};

const Records kSample{
    .functions = {1, 0, 1},
    .lower = {-1.0, -BIYOS_INF, 2.0},
    .upper = {1.0, BIYOS_INF, 2.0},
};

void add_functions(const Model &model, int n)
{
    ASSERT_EQ(BIYOS_addfun(model.get(), n, nullptr), BIYOS_OK);
}

void add_sample(const Model &model)
{
    add_functions(model, 2);
    const BIYOS_status status = BIYOS_addcon(model.get(),
                                             3,
                                             kSample.functions.data(),
                                             kSample.lower.data(),
                                             kSample.upper.data(),
                                             nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

int n_constraints(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getncon(model.get(), &n), BIYOS_OK);
    return n;
}

Records read_all(const Model &model)
{
    const int n = n_constraints(model);
    const auto size = static_cast<std::size_t>(n);
    Records read{
        .functions = std::vector<int>(size),
        .lower = std::vector<double>(size),
        .upper = std::vector<double>(size),
    };
    const BIYOS_status status = BIYOS_getconrg(model.get(),
                                               0,
                                               n,
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

Records read_bounds(const Model &model, const std::vector<BIYOS_index> &indices)
{
    const auto n = static_cast<BIYOS_index>(indices.size());
    Records read{
        .functions = {},
        .lower = std::vector<double>(indices.size(), 9.0),
        .upper = std::vector<double>(indices.size(), 9.0),
    };
    const BIYOS_status status =
        BIYOS_getconbndsp(model.get(), n, indices.data(), read.lower.data(), read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
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
    const BIYOS_index index = 0;
    const BIYOS_real bound = 0.0;
    BIYOS_index n = 0;
    note(attempt, BIYOS_addcon(model, 1, &index, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_setconbndsp(model, 1, &index, &bound, &bound));
    note(attempt, BIYOS_getconbndsp(model, 1, &index, nullptr, nullptr));
    note(attempt, BIYOS_getncon(model, &n));
    note(attempt, BIYOS_getconrg(model, 0, 1, nullptr, nullptr, nullptr));
}

}

TEST(CApiTest, AddedConstraints)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(n_constraints(model), 3);
    expect_records(read_all(model), kSample);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstConstraint)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0, 0};
    BIYOS_index start = -1;
    ASSERT_EQ(BIYOS_addcon(model.get(), 0, nullptr, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addcon(model.get(), 2, functions, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addcon(model.get(), 3, functions, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(model.get(), 1, functions, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 5);
    const BIYOS_index unknown = 1;
    EXPECT_EQ(BIYOS_addcon(model.get(), 1, &unknown, nullptr, nullptr, &start), BIYOS_ERR_BADARG);
    EXPECT_EQ(start, 5);
    EXPECT_EQ(n_constraints(model), 6);
}

TEST(CApiTest, SharedFunction)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0};
    const double lower[] = {0.0, -BIYOS_INF};
    const double upper[] = {1.0, 3.0};
    ASSERT_EQ(BIYOS_addcon(model.get(), 2, functions, lower, upper, nullptr), BIYOS_OK);
    const Records wanted{.functions = {0, 0}, .lower = {0.0, -BIYOS_INF}, .upper = {1.0, 3.0}};
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
    EXPECT_EQ(BIYOS_getconrg(model.get(), 1, 2, functions, lower, upper), BIYOS_OK);
    const Records found{
        .functions = {functions[0], functions[1]},
        .lower = {lower[0], lower[1]},
        .upper = {upper[0], upper[1]},
    };
    const Records wanted{
        .functions = {0, 1},
        .lower = {-BIYOS_INF, 2.0},
        .upper = {BIYOS_INF, 2.0},
    };
    expect_records(found, wanted);
    EXPECT_EQ(BIYOS_getconrg(model.get(), 3, 0, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullConstraints)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0};
    const double bounds[] = {0.5, 0.75};
    BIYOS_model *held = model.get();
    ASSERT_EQ(BIYOS_addcon(held, 2, functions, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(held, 2, functions, bounds, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(held, 2, functions, nullptr, bounds, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(held, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Records wanted{
        .functions = {0, 0, 0, 0, 0, 0},
        .lower = {-BIYOS_INF, -BIYOS_INF, 0.5, 0.75, -BIYOS_INF, -BIYOS_INF},
        .upper = {BIYOS_INF, BIYOS_INF, BIYOS_INF, BIYOS_INF, 0.5, 0.75},
    };
    expect_records(read_all(model), wanted);
    EXPECT_EQ(BIYOS_addcon(held, 1, nullptr, bounds, bounds, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'fun': is null");
    EXPECT_EQ(n_constraints(model), 6);
}

TEST(CApiTest, NullBounds)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    const int functions[] = {0, 0};
    ASSERT_EQ(BIYOS_addcon(model.get(), 2, functions, nullptr, nullptr, nullptr), BIYOS_OK);
    const int indices[] = {1, 0};
    const double lower[] = {1.0, 2.0};
    const double upper[] = {3.0, 4.0};
    BIYOS_model *held = model.get();
    ASSERT_EQ(BIYOS_setconbndsp(held, 2, indices, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setconbndsp(held, 1, indices, nullptr, upper), BIYOS_OK);
    ASSERT_EQ(BIYOS_setconbndsp(held, 2, indices, nullptr, nullptr), BIYOS_OK);
    const Records wanted{.functions = {0, 0}, .lower = {2.0, 1.0}, .upper = {BIYOS_INF, 3.0}};
    expect_records(read_all(model), wanted);
    EXPECT_EQ(BIYOS_setconbndsp(held, 2, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_setconbndsp(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullReads)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(BIYOS_getconrg(model.get(), 0, 3, nullptr, nullptr, nullptr), BIYOS_OK);
    int functions[] = {-1, -1};
    double high[] = {9.0, 9.0};
    EXPECT_EQ(BIYOS_getconrg(model.get(), 1, 2, functions, nullptr, high), BIYOS_OK);
    EXPECT_EQ(std::vector<int>({functions[0], functions[1]}), std::vector<int>({0, 1}));
    expect_doubles({high[0], high[1]}, {BIYOS_INF, 2.0});
    EXPECT_EQ(BIYOS_getncon(model.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
}

TEST(CApiTest, RefusedConstraints)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const int functions[] = {0, 2};
    const BIYOS_real nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(BIYOS_addcon(held, 2, functions, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 4: function 2 is unknown");
    EXPECT_EQ(BIYOS_addcon(held, 1, functions, &nan, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: lower bound is NaN");
    EXPECT_EQ(BIYOS_addcon(held, -1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    const BIYOS_index index = 3;
    const BIYOS_real bound = 0.0;
    EXPECT_EQ(BIYOS_setconbndsp(held, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: unknown");
    const BIYOS_index first = 0;
    const BIYOS_real infinite = BIYOS_INF;
    EXPECT_EQ(BIYOS_setconbndsp(held, 1, &first, &infinite, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 0: lower bound is infinity");
    EXPECT_EQ(BIYOS_getconrg(held, 2, 2, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: unknown");
    EXPECT_EQ(BIYOS_getconrg(held, -1, 1, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
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
    const BIYOS_index function = 0;
    ASSERT_EQ(BIYOS_addcon(model.get(), 1, &function, nullptr, nullptr, nullptr), BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status> statuses{refused, refused, BIYOS_OK, BIYOS_OK, BIYOS_OK};
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0], locked);
    EXPECT_EQ(attempt.messages[1], locked);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(n_constraints(model), 1);
}

TEST(CApiTest, ListedConstraints)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Records wanted{
        .functions = {},
        .lower = {2.0, -1.0, 2.0, -BIYOS_INF},
        .upper = {2.0, 1.0, 2.0, BIYOS_INF},
    };
    expect_records(read_bounds(model, {2, 0, 2, 1}), wanted);
    expect_records(read_bounds(model, {}), Records{});
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ConstraintNulls)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const BIYOS_index indices[] = {1, 2};
    double lower[] = {9.0, 9.0};
    double upper[] = {9.0, 9.0};
    const BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_getconbndsp(held, 2, indices, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getconbndsp(held, 2, indices, lower, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getconbndsp(held, 1, indices, nullptr, upper), BIYOS_OK);
    expect_doubles({lower[0], lower[1]}, {-BIYOS_INF, 2.0});
    expect_doubles({upper[0], upper[1]}, {BIYOS_INF, 9.0});
    EXPECT_EQ(BIYOS_getconbndsp(held, 0, nullptr, lower, upper), BIYOS_OK);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ConstraintSetters)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const BIYOS_index indices[] = {1, 0, 1};
    const double lower[] = {-2.0, 0.0, -3.0};
    const double upper[] = {2.0, 0.5, 3.0};
    ASSERT_EQ(BIYOS_setconbndsp(model.get(), 3, indices, lower, upper), BIYOS_OK);
    std::vector<double> read_lower(3, 9.0);
    std::vector<double> read_upper(3, 9.0);
    ASSERT_EQ(BIYOS_getconbndsp(model.get(), 3, indices, read_lower.data(), read_upper.data()),
              BIYOS_OK);
    expect_doubles(read_lower, {-3.0, 0.0, -3.0});
    expect_doubles(read_upper, {3.0, 0.5, 3.0});
    const Records untouched{.functions = {}, .lower = {2.0}, .upper = {2.0}};
    expect_records(read_bounds(model, {2}), untouched);
}

TEST(CApiTest, ConstraintRefusals)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const BIYOS_model *held = model.get();
    const BIYOS_index beyond[] = {0, 3};
    const BIYOS_index negative[] = {1, -1};
    double lower[] = {9.0, 9.0};
    double upper[] = {9.0, 9.0};
    EXPECT_EQ(BIYOS_getconbndsp(held, 2, beyond, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: unknown");
    EXPECT_EQ(BIYOS_getconbndsp(held, 2, negative, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint -1: unknown");
    EXPECT_EQ(BIYOS_getconbndsp(held, -1, beyond, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_getconbndsp(held, 1, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    expect_doubles({lower[0], lower[1]}, {9.0, 9.0});
    expect_doubles({upper[0], upper[1]}, {9.0, 9.0});
}

TEST(CApiTest, ConstraintTolerances)
{
    biyos::test::check_tolerances(biyos::test::kConstraintTolerances, add_sample);
}
