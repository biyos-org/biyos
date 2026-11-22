#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
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
};

struct Outputs
{
    int *functions = nullptr;
    int *senses = nullptr;
    int *priorities = nullptr;
    double *weights = nullptr;
};

const Records kSample{
    .functions = {1, 0, 1},
    .senses = {BIYOS_MAXIMIZE, BIYOS_MINIMIZE, BIYOS_MINIMIZE},
    .priorities = {2, -1, 2},
    .weights = {0.5, 1.0, 3.0},
    .absolute = {0.0, 1.5, kInfinity},
    .relative = {0.25, 0.0, 0.0},
};

BIYOS_status add_with(BIYOS_model *mdl,
                      int n,
                      const BIYOS_index *funs,
                      const Fields &fields,
                      BIYOS_index *beg = nullptr)
{
    return BIYOS_addobj(mdl, n, funs, fields.senses, fields.priorities, fields.weights, beg);
}

BIYOS_status get_with(BIYOS_model *mdl, int beg, int n, const Outputs &outputs)
{
    return BIYOS_getobjs(mdl,
                         beg,
                         n,
                         outputs.functions,
                         outputs.senses,
                         outputs.priorities,
                         outputs.weights);
}

BIYOS_status add_records(const Model &mdl, const Records &records, BIYOS_index *beg = nullptr)
{
    const Fields fields{
        .senses = records.senses.data(),
        .priorities = records.priorities.data(),
        .weights = records.weights.data(),
    };
    const auto n = static_cast<int>(records.functions.size());
    return add_with(mdl.get(), n, records.functions.data(), fields, beg);
}

void add_funs(const Model &mdl, int n)
{
    ASSERT_EQ(BIYOS_addfuns(mdl.get(), n, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
}

void add_sample(const Model &mdl)
{
    add_funs(mdl, 2);
    ASSERT_EQ(add_records(mdl, kSample), BIYOS_OK);
    const BIYOS_index idx[] = {0, 1, 2};
    const double *atol = kSample.absolute.data();
    const double *rtol = kSample.relative.data();
    ASSERT_EQ(BIYOS_setobjtolsp(mdl.get(), 3, idx, atol, rtol), BIYOS_OK);
}

int n_objs(const Model &mdl)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnobj(mdl.get(), &n), BIYOS_OK);
    return n;
}

Records read_range(const Model &mdl, int beg, int n)
{
    const auto size = static_cast<std::size_t>(n);
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
    };
    EXPECT_EQ(get_with(mdl.get(), beg, n, outputs), BIYOS_OK);
    std::vector<BIYOS_index> idx(size);
    std::iota(idx.begin(), idx.end(), beg);
    const BIYOS_status status =
        BIYOS_getobjtolsp(mdl.get(), n, idx.data(), read.absolute.data(), read.relative.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Records read_all(const Model &mdl)
{
    return read_range(mdl, 0, n_objs(mdl));
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

Records read_listed(const Model &mdl, const std::vector<BIYOS_index> &idx)
{
    const auto n = static_cast<BIYOS_index>(idx.size());
    const std::size_t size = idx.size();
    Records read{
        .functions = {},
        .senses = std::vector<int>(size, -9),
        .priorities = std::vector<int>(size, -9),
        .weights = std::vector<double>(size, -9.0),
        .absolute = std::vector<double>(size, -9.0),
        .relative = std::vector<double>(size, -9.0),
    };
    const BIYOS_model *held = mdl.get();
    const BIYOS_index *listed = idx.data();
    EXPECT_EQ(BIYOS_getobjsensesp(held, n, listed, read.senses.data()), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjpriosp(held, n, listed, read.priorities.data()), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjcoefsp(held, n, listed, read.weights.data()), BIYOS_OK);
    const BIYOS_status status =
        BIYOS_getobjtolsp(held, n, listed, read.absolute.data(), read.relative.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Records picked(const Records &all, const std::vector<BIYOS_index> &idx)
{
    Records found;
    for (const BIYOS_index i : idx)
    {
        const auto k = static_cast<std::size_t>(i);
        found.senses.push_back(all.senses[k]);
        found.priorities.push_back(all.priorities[k]);
        found.weights.push_back(all.weights[k]);
        found.absolute.push_back(all.absolute[k]);
        found.relative.push_back(all.relative[k]);
    }
    return found;
}

void expect_refused(const Env &env,
                    const BIYOS_model *mdl,
                    BIYOS_index n,
                    const BIYOS_index *idx,
                    const std::string &msg)
{
    int ints[] = {-9, -9};
    double reals[] = {-9.0, -9.0};
    double others[] = {-9.0, -9.0};
    EXPECT_EQ(BIYOS_getobjsensesp(mdl, n, idx, ints), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), msg);
    EXPECT_EQ(BIYOS_getobjpriosp(mdl, n, idx, ints), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), msg);
    EXPECT_EQ(BIYOS_getobjcoefsp(mdl, n, idx, reals), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), msg);
    EXPECT_EQ(BIYOS_getobjtolsp(mdl, n, idx, reals, others), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), msg);
    EXPECT_EQ(std::vector<int>({ints[0], ints[1]}), std::vector<int>(2, -9));
    const std::vector<double> written{reals[0], reals[1], others[0], others[1]};
    EXPECT_EQ(bits_of(written), bits_of(std::vector<double>(4, -9.0)));
}

Records defaults(const std::vector<int> &funs)
{
    const std::size_t size = funs.size();
    Records records{
        .functions = funs,
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
    const double weight = 2.0;
    const BIYOS_int sense = BIYOS_MAXIMIZE;
    BIYOS_index n = 0;
    note(attempt, add_with(mdl, 1, &idx, {}));
    note(attempt, BIYOS_setobjsensesp(mdl, 1, &idx, &sense));
    note(attempt, BIYOS_setobjpriosp(mdl, 1, &idx, &idx));
    note(attempt, BIYOS_setobjcoefsp(mdl, 1, &idx, &weight));
    note(attempt, BIYOS_setobjtolsp(mdl, 1, &idx, &weight, &weight));
    note(attempt, BIYOS_getnobj(mdl, &n));
    note(attempt, get_with(mdl, 0, 1, {}));
    note(attempt, BIYOS_getobjsensesp(mdl, 1, &idx, nullptr));
    note(attempt, BIYOS_getobjpriosp(mdl, 1, &idx, nullptr));
    note(attempt, BIYOS_getobjcoefsp(mdl, 1, &idx, nullptr));
    note(attempt, BIYOS_getobjtolsp(mdl, 1, &idx, nullptr, nullptr));
}

}

TEST(CApiTest, AddedObjectives)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    EXPECT_EQ(n_objs(mdl), 3);
    expect_records(read_all(mdl), kSample);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstObjective)
{
    const Env env;
    const Model mdl(env);
    add_funs(mdl, 1);
    const int funs[] = {0, 0, 0};
    BIYOS_index beg = -1;
    ASSERT_EQ(add_with(mdl.get(), 0, nullptr, {}, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    ASSERT_EQ(add_records(mdl, defaults({0, 0}), &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    ASSERT_EQ(add_with(mdl.get(), 3, funs, {}), BIYOS_OK);
    ASSERT_EQ(add_with(mdl.get(), 0, funs, {}, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 5);
    ASSERT_EQ(add_records(mdl, defaults({0}), &beg), BIYOS_OK);
    EXPECT_EQ(beg, 5);
    EXPECT_EQ(add_records(mdl, defaults({1}), &beg), BIYOS_ERR_BADARG);
    EXPECT_EQ(beg, 5);
    EXPECT_EQ(n_objs(mdl), 6);
}

TEST(CApiTest, DefaultObjectives)
{
    const Env env;
    const Model mdl(env);
    add_funs(mdl, 2);
    const int funs[] = {1, 0, 1};
    ASSERT_EQ(add_with(mdl.get(), 2, funs, {}), BIYOS_OK);
    expect_records(read_all(mdl), defaults({1, 0}));
    const int priorities[] = {-4, 0};
    ASSERT_EQ(add_with(mdl.get(), 2, funs, {.priorities = priorities}), BIYOS_OK);
    ASSERT_EQ(add_with(mdl.get(), 3, funs, {}), BIYOS_OK);
    Records wanted = defaults({1, 0, 1, 0, 1, 0, 1});
    wanted.priorities[2] = -4;
    wanted.priorities[3] = 0;
    expect_records(read_all(mdl), wanted);
}

TEST(CApiTest, NullObjectives)
{
    const Env env;
    const Model mdl(env);
    add_funs(mdl, 1);
    const int funs[] = {0, 0};
    const int senses[] = {BIYOS_MAXIMIZE, BIYOS_MINIMIZE};
    const int priorities[] = {7, 7};
    const double values[] = {0.5, 2.0};
    BIYOS_model *held = mdl.get();
    ASSERT_EQ(add_with(held, 2, funs, {.senses = senses}), BIYOS_OK);
    ASSERT_EQ(add_with(held, 2, funs, {.priorities = priorities}), BIYOS_OK);
    ASSERT_EQ(add_with(held, 2, funs, {.weights = values}), BIYOS_OK);
    Records wanted = defaults(std::vector<int>(6, 0));
    wanted.senses[0] = BIYOS_MAXIMIZE;
    wanted.priorities[2] = 7;
    wanted.priorities[3] = 7;
    wanted.weights[4] = 0.5;
    wanted.weights[5] = 2.0;
    expect_records(read_all(mdl), wanted);
    EXPECT_EQ(add_with(held, 1, nullptr, {.senses = senses}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': is null");
    EXPECT_EQ(n_objs(mdl), 6);
}

TEST(CApiTest, NullSetters)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const int idx[] = {2, 0};
    const int senses[] = {BIYOS_MAXIMIZE, BIYOS_MINIMIZE};
    const int priorities[] = {-5, 6};
    const double weights[] = {4.0, 8.0};
    const double absolute[] = {kInfinity, 0.125};
    const double relative[] = {1.0, 0.75};
    EXPECT_EQ(BIYOS_setobjsensesp(held, 2, idx, senses), BIYOS_OK);
    EXPECT_EQ(BIYOS_setobjpriosp(held, 2, idx, priorities), BIYOS_OK);
    EXPECT_EQ(BIYOS_setobjcoefsp(held, 1, idx, weights), BIYOS_OK);
    EXPECT_EQ(BIYOS_setobjtolsp(held, 2, idx, absolute, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_setobjtolsp(held, 1, idx, nullptr, relative), BIYOS_OK);
    EXPECT_EQ(BIYOS_setobjtolsp(held, 2, idx, nullptr, nullptr), BIYOS_OK);
    const Records wanted{
        .functions = kSample.functions,
        .senses = {BIYOS_MINIMIZE, BIYOS_MINIMIZE, BIYOS_MAXIMIZE},
        .priorities = {6, -1, -5},
        .weights = {0.5, 1.0, 4.0},
        .absolute = {0.125, 1.5, kInfinity},
        .relative = {0.25, 0.0, 1.0},
    };
    expect_records(read_all(mdl), wanted);
    EXPECT_EQ(BIYOS_setobjsensesp(held, 2, nullptr, senses), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_setobjpriosp(held, 0, nullptr, nullptr), BIYOS_OK);
    expect_records(read_all(mdl), wanted);
}

TEST(CApiTest, RepeatedObjective)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const int idx[] = {1, 1};
    const int senses[] = {BIYOS_MINIMIZE, BIYOS_MAXIMIZE};
    const double weights[] = {2.0, 3.0};
    ASSERT_EQ(BIYOS_setobjsensesp(mdl.get(), 2, idx, senses), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjcoefsp(mdl.get(), 2, idx, weights), BIYOS_OK);
    Records wanted = kSample;
    wanted.senses[1] = BIYOS_MAXIMIZE;
    wanted.weights[1] = 3.0;
    expect_records(read_all(mdl), wanted);
}

TEST(CApiTest, ObjectiveRange)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const Records wanted{
        .functions = {0, 1},
        .senses = {BIYOS_MINIMIZE, BIYOS_MINIMIZE},
        .priorities = {-1, 2},
        .weights = {1.0, 3.0},
        .absolute = {1.5, kInfinity},
        .relative = {0.0, 0.0},
    };
    expect_records(read_range(mdl, 1, 2), wanted);
    expect_records(read_range(mdl, 3, 0), Records{});
}

TEST(CApiTest, NullOutputs)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    EXPECT_EQ(get_with(held, 0, 3, {}), BIYOS_OK);
    int senses[] = {-1, -1};
    double weights[] = {9.0, 9.0};
    EXPECT_EQ(get_with(held, 1, 2, {.senses = senses, .weights = weights}), BIYOS_OK);
    EXPECT_EQ(std::vector<int>({senses[0], senses[1]}), std::vector<int>(2, BIYOS_MINIMIZE));
    EXPECT_EQ(bits_of({weights[0], weights[1]}), bits_of({1.0, 3.0}));
    EXPECT_EQ(BIYOS_getnobj(held, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
}

TEST(CApiTest, RefusedObjectives)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const int funs[] = {0, 2};
    EXPECT_EQ(add_with(held, 2, funs, {}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 4: function 2 is unknown");
    const int sense = 3;
    EXPECT_EQ(add_with(held, 1, funs, {.senses = &sense}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: sense 3 is unknown");
    const double negative = -1.0;
    EXPECT_EQ(add_with(held, 1, funs, {.weights = &negative}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: weight -1 is not positive");
    EXPECT_EQ(add_with(held, -1, nullptr, {}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    const BIYOS_index unknown = 3;
    EXPECT_EQ(BIYOS_setobjcoefsp(held, 1, &unknown, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: unknown");
    const BIYOS_index zero = 0;
    EXPECT_EQ(BIYOS_setobjsensesp(held, 1, &zero, &zero), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: sense 0 is unknown");
    EXPECT_EQ(BIYOS_setobjtolsp(held, 1, &zero, nullptr, &negative), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: relative tolerance -1 is negative");
    EXPECT_EQ(BIYOS_setobjcoefsp(held, 1, &zero, &kInfinity), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: weight is infinity");
    EXPECT_EQ(get_with(held, 2, 2, {}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: unknown");
    EXPECT_EQ(get_with(held, -1, 1, {}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective -1: unknown");
    expect_records(read_all(mdl), kSample);
}

TEST(CApiTest, LockedObjectives)
{
    const Env env;
    const Model mdl(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = mdl.get();
    add_funs(mdl, 1);
    const BIYOS_index fun = 0;
    ASSERT_EQ(add_with(mdl.get(), 1, &fun, {}), BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    std::vector<BIYOS_status> statuses(5, BIYOS_ERR_BADARG);
    statuses.resize(11, BIYOS_OK);
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, std::vector<std::string>(11, locked));
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
    expect_records(read_all(mdl), defaults({0}));
}

TEST(CApiTest, ListedObjectives)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const std::vector<BIYOS_index> idx{2, 0, 2, 1};
    const Records wanted{
        .functions = {},
        .senses = {BIYOS_MINIMIZE, BIYOS_MAXIMIZE, BIYOS_MINIMIZE, BIYOS_MINIMIZE},
        .priorities = {2, 2, 2, -1},
        .weights = {3.0, 0.5, 3.0, 1.0},
        .absolute = {kInfinity, 0.0, kInfinity, 1.5},
        .relative = {0.0, 0.25, 0.0, 0.0},
    };
    expect_records(read_listed(mdl, idx), wanted);
    expect_records(picked(read_all(mdl), idx), wanted);
    expect_records(read_listed(mdl, {}), Records{});
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ObjectiveSetters)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const BIYOS_index idx[] = {2, 0, 2};
    const int sense[] = {BIYOS_MINIMIZE, BIYOS_MAXIMIZE, BIYOS_MAXIMIZE};
    const int prio[] = {5, -3, 7};
    const double coef[] = {2.0, 4.0, 8.0};
    const double atol[] = {0.5, kInfinity, 1.0};
    const double rtol[] = {0.125, 0.25, 0.0};
    ASSERT_EQ(BIYOS_setobjsensesp(held, 3, idx, sense), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjpriosp(held, 3, idx, prio), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjcoefsp(held, 3, idx, coef), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjtolsp(held, 3, idx, atol, rtol), BIYOS_OK);
    const Records listed{
        .functions = {},
        .senses = {BIYOS_MAXIMIZE, BIYOS_MAXIMIZE, BIYOS_MAXIMIZE},
        .priorities = {7, -3, 7},
        .weights = {8.0, 4.0, 8.0},
        .absolute = {1.0, kInfinity, 1.0},
        .relative = {0.0, 0.25, 0.0},
    };
    expect_records(read_listed(mdl, {2, 0, 2}), listed);
    ASSERT_EQ(BIYOS_setobjsensesp(held, 3, idx, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjpriosp(held, 3, idx, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjcoefsp(held, 3, idx, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjtolsp(held, 3, idx, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjtolsp(held, 1, idx, atol, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjtolsp(held, 0, nullptr, atol, rtol), BIYOS_OK);
    const Records wanted{
        .functions = kSample.functions,
        .senses = {BIYOS_MAXIMIZE, BIYOS_MINIMIZE, BIYOS_MAXIMIZE},
        .priorities = {-3, -1, 7},
        .weights = {4.0, 1.0, 8.0},
        .absolute = {kInfinity, 1.5, 0.5},
        .relative = {0.25, 0.0, 0.0},
    };
    expect_records(read_all(mdl), wanted);
    expect_records(read_listed(mdl, {0, 1, 2}), picked(wanted, {0, 1, 2}));
}

TEST(CApiTest, ObjectiveNulls)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    const BIYOS_model *held = mdl.get();
    const BIYOS_index idx[] = {1, 2};
    double atol[] = {-9.0, -9.0};
    double rtol[] = {-9.0, -9.0};
    int found[] = {-9, -9};
    EXPECT_EQ(BIYOS_getobjsensesp(held, 2, idx, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjpriosp(held, 2, idx, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjcoefsp(held, 2, idx, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjtolsp(held, 2, idx, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjtolsp(held, 2, idx, atol, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjtolsp(held, 1, idx, nullptr, rtol), BIYOS_OK);
    EXPECT_EQ(bits_of({atol[0], atol[1]}), bits_of({1.5, kInfinity}));
    EXPECT_EQ(bits_of({rtol[0], rtol[1]}), bits_of({0.0, -9.0}));
    EXPECT_EQ(BIYOS_getobjsensesp(held, 0, nullptr, found), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjpriosp(held, 0, nullptr, found), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjcoefsp(held, 0, nullptr, atol), BIYOS_OK);
    EXPECT_EQ(BIYOS_getobjtolsp(held, 0, nullptr, atol, rtol), BIYOS_OK);
    EXPECT_EQ(std::vector<int>({found[0], found[1]}), std::vector<int>(2, -9));
    EXPECT_EQ(bits_of({atol[0], rtol[0]}), bits_of({1.5, 0.0}));
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ObjectiveRefusals)
{
    const Env env;
    const Model mdl(env);
    add_sample(mdl);
    BIYOS_model *held = mdl.get();
    const BIYOS_index beyond[] = {0, 3};
    const BIYOS_index negative[] = {1, -1};
    expect_refused(env, held, 2, beyond, "invalid argument: objective 3: unknown");
    expect_refused(env, held, 2, negative, "invalid argument: objective -1: unknown");
    expect_refused(env, held, -1, beyond, "invalid argument: argument 'n': -1 is negative");
    expect_refused(env, held, 1, nullptr, "invalid argument: argument 'idx': is null");
    const BIYOS_index idx[] = {1, 0};
    const int sense[] = {BIYOS_MAXIMIZE, 0};
    const int prio[] = {4, 5};
    const double coef[] = {2.0, -1.0};
    const double atol[] = {0.5, -1.0};
    const double rtol[] = {0.5, kNan};
    EXPECT_EQ(BIYOS_setobjsensesp(held, 2, beyond, sense), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 3: unknown");
    EXPECT_EQ(BIYOS_setobjpriosp(held, 2, negative, prio), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective -1: unknown");
    EXPECT_EQ(BIYOS_setobjcoefsp(held, -1, idx, coef), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_setobjtolsp(held, 1, nullptr, atol, rtol), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_setobjsensesp(held, 2, idx, sense), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: sense 0 is unknown");
    EXPECT_EQ(BIYOS_setobjcoefsp(held, 2, idx, coef), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: weight -1 is not positive");
    EXPECT_EQ(BIYOS_setobjcoefsp(held, 1, idx, &kInfinity), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 1: weight is infinity");
    EXPECT_EQ(BIYOS_setobjtolsp(held, 2, idx, atol, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: absolute tolerance -1 is negative");
    EXPECT_EQ(BIYOS_setobjtolsp(held, 2, idx, nullptr, rtol), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: objective 0: relative tolerance is NaN");
    expect_records(read_all(mdl), kSample);
}

TEST(CApiTest, NewTolerances)
{
    const Env env;
    const Model mdl(env);
    add_funs(mdl, 1);
    const BIYOS_index funidx[] = {0, 0};
    BIYOS_index beg = -1;
    ASSERT_EQ(BIYOS_addobj(mdl.get(), 2, funidx, nullptr, nullptr, nullptr, &beg), BIYOS_OK);
    EXPECT_EQ(beg, 0);
    expect_records(read_all(mdl), defaults({0, 0}));
    const BIYOS_index idx[] = {1, 0};
    const double atol[] = {0.5, kInfinity};
    const double rtol[] = {0.25, 0.0};
    ASSERT_EQ(BIYOS_setobjtolsp(mdl.get(), 2, idx, atol, rtol), BIYOS_OK);
    Records wanted = defaults({0, 0});
    wanted.absolute = {kInfinity, 0.5};
    wanted.relative = {0.0, 0.25};
    expect_records(read_all(mdl), wanted);
    int funs[] = {-1, -1};
    double coef[] = {-9.0, -9.0};
    EXPECT_EQ(BIYOS_getobjs(mdl.get(), 0, 2, funs, nullptr, nullptr, coef), BIYOS_OK);
    EXPECT_EQ(std::vector<int>({funs[0], funs[1]}), std::vector<int>(2, 0));
    EXPECT_EQ(bits_of({coef[0], coef[1]}), bits_of({1.0, 1.0}));
}
