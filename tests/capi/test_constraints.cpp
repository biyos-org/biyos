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
using biyos::test::expect_dbls;
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

void add_funs(const Model &mdl, int n)
{
    ASSERT_EQ(BIYOS_addfuns(mdl.get(), n, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
}

void add_sample(const Model &mdl)
{
    add_funs(mdl, 2);
    const BIYOS_status status = BIYOS_addcon(mdl.get(),
                                             3,
                                             kSample.functions.data(),
                                             kSample.lower.data(),
                                             kSample.upper.data(),
                                             nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

int n_cons(const Model &mdl)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getncon(mdl.get(), &n), BIYOS_OK);
    return n;
}

Records read_all(const Model &mdl)
{
    const int n = n_cons(mdl);
    const auto size = static_cast<std::size_t>(n);
    Records read{
        .functions = std::vector<int>(size),
        .lower = std::vector<double>(size),
        .upper = std::vector<double>(size),
    };
    const BIYOS_status status =
        BIYOS_getcons(mdl.get(), 0, n, read.functions.data(), read.lower.data(), read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void expect_records(const Records &found, const Records &wanted)
{
    EXPECT_EQ(found.functions, wanted.functions);
    expect_dbls(found.lower, wanted.lower);
    expect_dbls(found.upper, wanted.upper);
}

Records read_bounds(const Model &mdl, const std::vector<BIYOS_index> &idx)
{
    const auto n = static_cast<BIYOS_index>(idx.size());
    Records read{
        .functions = {},
        .lower = std::vector<double>(idx.size(), 9.0),
        .upper = std::vector<double>(idx.size(), 9.0),
    };
    const BIYOS_status status =
        BIYOS_getconbnd(mdl.get(), n, idx.data(), read.lower.data(), read.upper.data());
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
    attempt.messages.emplace_back(BIYOS_lasterror(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *mdl = attempt.model;
    const BIYOS_index idx = 0;
    const BIYOS_real bound = 0.0;
    BIYOS_index n = 0;
    note(attempt, BIYOS_addcon(mdl, 1, &idx, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_setconbnd(mdl, 1, &idx, &bound, &bound));
    note(attempt, BIYOS_getconbnd(mdl, 1, &idx, nullptr, nullptr));
    note(attempt, BIYOS_getncon(mdl, &n));
    note(attempt, BIYOS_getcons(mdl, 0, 1, nullptr, nullptr, nullptr));
}

}

TEST(CApiTest, AddedConstraints)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    EXPECT_EQ(n_cons(mdl), 3);
    expect_records(read_all(mdl), kSample);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstConstraint)
{
    const Env env;
    const Model mdl(env);
    add_funs(mdl, 1);
    const int funs[] = {0, 0, 0};
    BIYOS_index beg = -1;
    ASSERT_EQ(BIYOS_addcon(mdl.get(), 0, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    ASSERT_EQ(BIYOS_addcon(mdl.get(), 2, funs, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    ASSERT_EQ(BIYOS_addcon(mdl.get(), 3, funs, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(mdl.get(), 1, funs, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 5);
    const BIYOS_index unknown = 1;
    EXPECT_EQ(BIYOS_addcon(mdl.get(), 1, &unknown, nullptr, nullptr, &beg), BIYOS_ERR_BADARG);
    EXPECT_EQ(beg, 5);
    EXPECT_EQ(n_cons(mdl), 6);
}

TEST(CApiTest, SharedFunction)
{
    const Env env;
    const Model mdl(env);
    add_funs(mdl, 1);
    const int funs[] = {0, 0};
    const double lower[] = {0.0, -kInfinity};
    const double upper[] = {1.0, 3.0};
    ASSERT_EQ(BIYOS_addcon(mdl.get(), 2, funs, lower, upper, nullptr), BIYOS_OK);
    const Records wanted{.functions = {0, 0}, .lower = {0.0, -kInfinity}, .upper = {1.0, 3.0}};
    expect_records(read_all(mdl), wanted);
}

TEST(CApiTest, ConstraintRange)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    int funs[] = {-1, -1};
    double lower[] = {0.0, 0.0};
    double upper[] = {0.0, 0.0};
    EXPECT_EQ(BIYOS_getcons(mdl.get(), 1, 2, funs, lower, upper), BIYOS_OK);
    const Records found{
        .functions = {funs[0], funs[1]},
        .lower = {lower[0], lower[1]},
        .upper = {upper[0], upper[1]},
    };
    const Records wanted{
        .functions = {0, 1},
        .lower = {-kInfinity, 2.0},
        .upper = {kInfinity, 2.0},
    };
    expect_records(found, wanted);
    EXPECT_EQ(BIYOS_getcons(mdl.get(), 3, 0, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullConstraints)
{
    const Env env;
    const Model mdl(env);
    add_funs(mdl, 1);
    const int funs[] = {0, 0};
    const double bnds[] = {0.5, 0.75};
    BIYOS_model *held = mdl.get();
    ASSERT_EQ(BIYOS_addcon(held, 2, funs, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(held, 2, funs, bnds, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(held, 2, funs, nullptr, bnds, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(held, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Records wanted{
        .functions = {0, 0, 0, 0, 0, 0},
        .lower = {-kInfinity, -kInfinity, 0.5, 0.75, -kInfinity, -kInfinity},
        .upper = {kInfinity, kInfinity, kInfinity, kInfinity, 0.5, 0.75},
    };
    expect_records(read_all(mdl), wanted);
    EXPECT_EQ(BIYOS_addcon(held, 1, nullptr, bnds, bnds, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': is null");
    EXPECT_EQ(n_cons(mdl), 6);
}

TEST(CApiTest, NullBounds)
{
    const Env env;
    const Model mdl(env);
    add_funs(mdl, 1);
    const int funs[] = {0, 0};
    ASSERT_EQ(BIYOS_addcon(mdl.get(), 2, funs, nullptr, nullptr, nullptr), BIYOS_OK);
    const int idx[] = {1, 0};
    const double lower[] = {1.0, 2.0};
    const double upper[] = {3.0, 4.0};
    BIYOS_model *held = mdl.get();
    ASSERT_EQ(BIYOS_setconbnd(held, 2, idx, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setconbnd(held, 1, idx, nullptr, upper), BIYOS_OK);
    ASSERT_EQ(BIYOS_setconbnd(held, 2, idx, nullptr, nullptr), BIYOS_OK);
    const Records wanted{.functions = {0, 0}, .lower = {2.0, 1.0}, .upper = {kInfinity, 3.0}};
    expect_records(read_all(mdl), wanted);
    EXPECT_EQ(BIYOS_setconbnd(held, 2, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_setconbnd(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullReads)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    EXPECT_EQ(BIYOS_getcons(mdl.get(), 0, 3, nullptr, nullptr, nullptr), BIYOS_OK);
    int funs[] = {-1, -1};
    double high[] = {9.0, 9.0};
    EXPECT_EQ(BIYOS_getcons(mdl.get(), 1, 2, funs, nullptr, high), BIYOS_OK);
    EXPECT_EQ(std::vector<int>({funs[0], funs[1]}), std::vector<int>({0, 1}));
    expect_dbls({high[0], high[1]}, {kInfinity, 2.0});
    EXPECT_EQ(BIYOS_getncon(mdl.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
}

TEST(CApiTest, RefusedConstraints)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const int funs[] = {0, 2};
    const BIYOS_real nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(BIYOS_addcon(held, 2, funs, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 4: function 2 is unknown");
    EXPECT_EQ(BIYOS_addcon(held, 1, funs, &nan, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: lower bound is NaN");
    EXPECT_EQ(BIYOS_addcon(held, -1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    const BIYOS_index idx = 3;
    const BIYOS_real bound = 0.0;
    EXPECT_EQ(BIYOS_setconbnd(held, 1, &idx, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: unknown");
    const BIYOS_index first = 0;
    const BIYOS_real infinite = kInfinity;
    EXPECT_EQ(BIYOS_setconbnd(held, 1, &first, &infinite, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 0: lower bound is infinity");
    EXPECT_EQ(BIYOS_getcons(held, 2, 2, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: unknown");
    EXPECT_EQ(BIYOS_getcons(held, -1, 1, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint -1: unknown");
    expect_records(read_all(mdl), kSample);
}

TEST(CApiTest, LockedConstraints)
{
    const Env env;
    const Model mdl(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = mdl.get();
    add_funs(mdl, 1);
    const BIYOS_index fun = 0;
    ASSERT_EQ(BIYOS_addcon(mdl.get(), 1, &fun, nullptr, nullptr, nullptr), BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status> statuses{refused, refused, BIYOS_OK, BIYOS_OK, BIYOS_OK};
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0], locked);
    EXPECT_EQ(attempt.messages[1], locked);
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(n_cons(mdl), 1);
}

TEST(CApiTest, ListedConstraints)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const Records wanted{
        .functions = {},
        .lower = {2.0, -1.0, 2.0, -kInfinity},
        .upper = {2.0, 1.0, 2.0, kInfinity},
    };
    expect_records(read_bounds(mdl, {2, 0, 2, 1}), wanted);
    expect_records(read_bounds(mdl, {}), Records{});
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ConstraintNulls)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const BIYOS_index idx[] = {1, 2};
    double lower[] = {9.0, 9.0};
    double upper[] = {9.0, 9.0};
    const BIYOS_model *held = mdl.get();
    EXPECT_EQ(BIYOS_getconbnd(held, 2, idx, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getconbnd(held, 2, idx, lower, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getconbnd(held, 1, idx, nullptr, upper), BIYOS_OK);
    expect_dbls({lower[0], lower[1]}, {-kInfinity, 2.0});
    expect_dbls({upper[0], upper[1]}, {kInfinity, 9.0});
    EXPECT_EQ(BIYOS_getconbnd(held, 0, nullptr, lower, upper), BIYOS_OK);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ConstraintSetters)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const BIYOS_index idx[] = {1, 0, 1};
    const double lower[] = {-2.0, 0.0, -3.0};
    const double upper[] = {2.0, 0.5, 3.0};
    ASSERT_EQ(BIYOS_setconbnd(mdl.get(), 3, idx, lower, upper), BIYOS_OK);
    std::vector<double> lb(3, 9.0);
    std::vector<double> ub(3, 9.0);
    ASSERT_EQ(BIYOS_getconbnd(mdl.get(), 3, idx, lb.data(), ub.data()), BIYOS_OK);
    expect_dbls(lb, {-3.0, 0.0, -3.0});
    expect_dbls(ub, {3.0, 0.5, 3.0});
    const Records untouched{.functions = {}, .lower = {2.0}, .upper = {2.0}};
    expect_records(read_bounds(mdl, {2}), untouched);
}

TEST(CApiTest, ConstraintRefusals)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const BIYOS_model *held = mdl.get();
    const BIYOS_index beyond[] = {0, 3};
    const BIYOS_index negative[] = {1, -1};
    double lower[] = {9.0, 9.0};
    double upper[] = {9.0, 9.0};
    EXPECT_EQ(BIYOS_getconbnd(held, 2, beyond, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint 3: unknown");
    EXPECT_EQ(BIYOS_getconbnd(held, 2, negative, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: constraint -1: unknown");
    EXPECT_EQ(BIYOS_getconbnd(held, -1, beyond, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_getconbnd(held, 1, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    expect_dbls({lower[0], lower[1]}, {9.0, 9.0});
    expect_dbls({upper[0], upper[1]}, {9.0, 9.0});
}
