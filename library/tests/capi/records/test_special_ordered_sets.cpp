#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "doubles.hpp"
#include "handles.hpp"
#include "model/compare/compare.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::difference;
using biyos::test::Env;
using biyos::test::expect_doubles;
using biyos::test::last_message;
using biyos::test::Model;

struct SpecialOrderedSets
{
    std::vector<int> kinds;
    std::vector<BIYOS_offset> counts;
    std::vector<BIYOS_index> variables;
    std::vector<double> weights;
};

const SpecialOrderedSets kSample{
    .kinds = {BIYOS_SOSKND_TWO, BIYOS_SOSKND_ONE, BIYOS_SOSKND_ONE},
    .counts = {3, 0, 2},
    .variables = {2, 0, 3, 1, 0},
    .weights = {3.0, -1.0, 2.5, 0.5, 1.5},
};

const SpecialOrderedSets kEmpty{
    .kinds = {},
    .counts = {},
    .variables = {},
    .weights = {},
};

void add_variables(const Model &model, int n)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), n, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
}

BIYOS_status add_special_ordered_sets(const Model &model,
                                      const SpecialOrderedSets &soss,
                                      BIYOS_index *pos)
{
    return BIYOS_addsos(model.get(),
                        static_cast<BIYOS_index>(soss.kinds.size()),
                        soss.kinds.data(),
                        soss.counts.data(),
                        soss.variables.data(),
                        soss.weights.data(),
                        pos);
}

void add_sample(const Model &model)
{
    add_variables(model, 4);
    ASSERT_EQ(add_special_ordered_sets(model, kSample, nullptr), BIYOS_OK);
}

int n_special_ordered_sets(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnsos(model.get(), &n), BIYOS_OK);
    return n;
}

BIYOS_offset n_members(const Model &model, BIYOS_index pos, BIYOS_index n)
{
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnsosvarrg(model.get(), pos, pos + n, &nnz), BIYOS_OK);
    return nnz;
}

SpecialOrderedSets sized(std::size_t n, BIYOS_offset nnz)
{
    const auto size = static_cast<std::size_t>(nnz);
    return SpecialOrderedSets{
        .kinds = std::vector<int>(n, -1),
        .counts = std::vector<BIYOS_offset>(n, -1),
        .variables = std::vector<BIYOS_index>(size, -1),
        .weights = std::vector<double>(size, 9.0),
    };
}

BIYOS_status read_range(const Model &model,
                        BIYOS_index pos,
                        BIYOS_offset cap,
                        SpecialOrderedSets &read)
{
    return BIYOS_getsosrg(model.get(),
                          pos,
                          pos + static_cast<BIYOS_index>(read.kinds.size()),
                          cap,
                          read.kinds.data(),
                          read.counts.data(),
                          read.variables.data(),
                          read.weights.data());
}

BIYOS_status read_list(const Model &model,
                       const std::vector<BIYOS_index> &indices,
                       BIYOS_offset cap,
                       SpecialOrderedSets &read)
{
    return BIYOS_getsossp(model.get(),
                          static_cast<BIYOS_index>(indices.size()),
                          indices.data(),
                          cap,
                          read.kinds.data(),
                          read.counts.data(),
                          read.variables.data(),
                          read.weights.data());
}

SpecialOrderedSets read_all(const Model &model)
{
    const int n = n_special_ordered_sets(model);
    const BIYOS_offset nnz = n_members(model, 0, n);
    SpecialOrderedSets read = sized(static_cast<std::size_t>(n), nnz);
    EXPECT_EQ(read_range(model, 0, nnz, read), BIYOS_OK);
    return read;
}

SpecialOrderedSets read_listed(const Model &model, const std::vector<BIYOS_index> &indices)
{
    const auto n = static_cast<BIYOS_index>(indices.size());
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnsosvarsp(model.get(), n, indices.data(), &nnz), BIYOS_OK);
    SpecialOrderedSets read = sized(indices.size(), nnz);
    EXPECT_EQ(read_list(model, indices, nnz, read), BIYOS_OK);
    return read;
}

void expect_special_ordered_sets(const SpecialOrderedSets &found, const SpecialOrderedSets &wanted)
{
    EXPECT_EQ(found.kinds, wanted.kinds);
    EXPECT_EQ(found.counts, wanted.counts);
    EXPECT_EQ(found.variables, wanted.variables);
    expect_doubles(found.weights, wanted.weights);
}

void expect_refused(const Env &env,
                    const Model &model,
                    const SpecialOrderedSets &soss,
                    const std::string &text)
{
    const Model before(env, model);
    BIYOS_index pos = -1;
    EXPECT_EQ(add_special_ordered_sets(model, soss, &pos), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: " + text);
    EXPECT_EQ(pos, -1);
    EXPECT_EQ(difference(before.get()->model(), model.get()->model()), "");
}

SpecialOrderedSets single(int kind, std::vector<BIYOS_index> variables, std::vector<double> weights)
{
    const auto size = static_cast<BIYOS_offset>(variables.size());
    return SpecialOrderedSets{
        .kinds = {kind},
        .counts = {size},
        .variables = std::move(variables),
        .weights = std::move(weights),
    };
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
    attempt.messages.emplace_back(last_message(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    const int kind = BIYOS_SOSKND_ONE;
    BIYOS_index n = 0;
    BIYOS_offset nnz = 0;
    note(attempt, BIYOS_addsos(model, 1, &kind, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getnsos(model, &n));
    note(attempt, BIYOS_getnsosvarrg(model, 0, 1, &nnz));
    note(attempt, BIYOS_getsosrg(model, 0, 1, 0, nullptr, nullptr, nullptr, nullptr));
}

}

TEST(CApiTest, AddedSets)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(n_special_ordered_sets(model), 3);
    EXPECT_EQ(n_members(model, 0, 3), 5);
    EXPECT_EQ(n_members(model, 1, 1), 0);
    expect_special_ordered_sets(read_all(model), kSample);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstSet)
{
    const Env env;
    const Model model(env);
    add_variables(model, 2);
    const SpecialOrderedSets two{
        .kinds = {BIYOS_SOSKND_ONE, BIYOS_SOSKND_TWO},
        .counts = {1, 1},
        .variables = {0, 1},
        .weights = {1.0, 2.0},
    };
    BIYOS_index pos = -1;
    ASSERT_EQ(add_special_ordered_sets(model, kEmpty, &pos), BIYOS_OK);
    EXPECT_EQ(pos, 0);
    ASSERT_EQ(add_special_ordered_sets(model, two, &pos), BIYOS_OK);
    EXPECT_EQ(pos, 0);
    ASSERT_EQ(add_special_ordered_sets(model, two, nullptr), BIYOS_OK);
    ASSERT_EQ(add_special_ordered_sets(model, two, &pos), BIYOS_OK);
    EXPECT_EQ(pos, 4);
    EXPECT_EQ(add_special_ordered_sets(model, single(0, {}, {}), &pos), BIYOS_ERR_BADARG);
    EXPECT_EQ(pos, 4);
    EXPECT_EQ(n_special_ordered_sets(model), 6);
}

TEST(CApiTest, DefaultWeights)
{
    const Env env;
    const Model model(env);
    add_variables(model, 3);
    const int kinds[] = {BIYOS_SOSKND_TWO, BIYOS_SOSKND_ONE};
    const BIYOS_offset counts[] = {3, 1};
    const BIYOS_index variables[] = {2, 0, 1, 1};
    BIYOS_model *held = model.get();
    ASSERT_EQ(BIYOS_addsos(held, 2, kinds, counts, variables, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addsos(held, 2, kinds, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const SpecialOrderedSets wanted{
        .kinds = {BIYOS_SOSKND_TWO, BIYOS_SOSKND_ONE, BIYOS_SOSKND_TWO, BIYOS_SOSKND_ONE},
        .counts = {3, 1, 0, 0},
        .variables = {2, 0, 1, 1},
        .weights = {1.0, 2.0, 3.0, 1.0},
    };
    expect_special_ordered_sets(read_all(model), wanted);
}

TEST(CApiTest, NullSets)
{
    const Env env;
    const Model model(env);
    add_variables(model, 2);
    const int kind = BIYOS_SOSKND_ONE;
    const BIYOS_offset count = 2;
    const double weights[] = {1.0, 2.0};
    BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_addsos(held, 1, nullptr, &count, nullptr, weights, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'knd': is null");
    EXPECT_EQ(BIYOS_addsos(held, 1, &kind, &count, nullptr, weights, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'var': is null");
    EXPECT_EQ(BIYOS_addsos(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(n_special_ordered_sets(model), 0);
    EXPECT_EQ(BIYOS_getnsos(held, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
    EXPECT_EQ(BIYOS_getnsosvarrg(held, 0, 0, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
    EXPECT_EQ(BIYOS_getnsosvarsp(held, 1, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
}

TEST(CApiTest, RefusedSets)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    SpecialOrderedSets two{
        .kinds = {BIYOS_SOSKND_ONE, 0},
        .counts = {1, 1},
        .variables = {0, 1},
        .weights = {1.0, 2.0},
    };
    expect_refused(env, model, two, "set 4: kind 0 is unknown");
    two.kinds = {BIYOS_SOSKND_ONE, BIYOS_SOSKND_TWO};
    two.counts = {(std::numeric_limits<BIYOS_offset>::max)(), 1};
    expect_refused(env, model, two, "argument 'cnt': sum overflows at entry 1");
    SpecialOrderedSets three = two;
    three.kinds.push_back(BIYOS_SOSKND_ONE);
    three.counts = {1, 1, -1};
    expect_refused(env, model, three, "argument 'cnt': -1 is negative at entry 2");
    expect_refused(env,
                   model,
                   single(BIYOS_SOSKND_TWO, {4}, {1.0}),
                   "set 3: variable 4 is unknown");
    expect_refused(env,
                   model,
                   single(BIYOS_SOSKND_TWO, {-1}, {1.0}),
                   "set 3: variable -1 is unknown");
    const SpecialOrderedSets repeated = single(BIYOS_SOSKND_ONE, {1, 0, 1}, {1.0, 2.0, 3.0});
    expect_refused(env, model, repeated, "set 3: variable 1 is repeated");
    const SpecialOrderedSets equal = single(BIYOS_SOSKND_TWO, {0, 1, 2}, {2.0, 1.0, 2.0});
    expect_refused(env, model, equal, "set 3: weight 2 is repeated");
    const SpecialOrderedSets undefined = single(BIYOS_SOSKND_ONE, {0}, {nan});
    expect_refused(env, model, undefined, "set 3: weight of variable 0 is NaN");
    EXPECT_EQ(BIYOS_addsos(model.get(), -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    expect_special_ordered_sets(read_all(model), kSample);
}

TEST(CApiTest, ListedSets)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const SpecialOrderedSets wanted{
        .kinds = {BIYOS_SOSKND_ONE, BIYOS_SOSKND_TWO, BIYOS_SOSKND_ONE, BIYOS_SOSKND_ONE},
        .counts = {2, 3, 2, 0},
        .variables = {1, 0, 2, 0, 3, 1, 0},
        .weights = {0.5, 1.5, 3.0, -1.0, 2.5, 0.5, 1.5},
    };
    expect_special_ordered_sets(read_listed(model, {2, 0, 2, 1}), wanted);
    expect_special_ordered_sets(read_listed(model, {0, 1, 2}), read_all(model));
    expect_special_ordered_sets(read_listed(model, {}), kEmpty);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, SetCapacity)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    SpecialOrderedSets read = sized(3, 8);
    const SpecialOrderedSets untouched = read;
    EXPECT_EQ(read_range(model, 0, 4, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 4 of the 5 members");
    EXPECT_EQ(read_range(model, 0, -1, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': -1 is negative");
    EXPECT_EQ(read_list(model, {0, 1, 2}, 4, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 4 of the 5 members");
    expect_special_ordered_sets(read, untouched);
    ASSERT_EQ(read_range(model, 0, 8, read), BIYOS_OK);
    EXPECT_EQ(read.counts, kSample.counts);
    EXPECT_EQ(read.variables, std::vector<BIYOS_index>({2, 0, 3, 1, 0, -1, -1, -1}));
}

TEST(CApiTest, SetIndices)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    SpecialOrderedSets read = sized(2, 5);
    EXPECT_EQ(read_range(model, 2, 5, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: set 3: unknown");
    EXPECT_EQ(read_list(model, {1, 3}, 5, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: set 3: unknown");
    const BIYOS_index unknown = -1;
    BIYOS_offset nnz = 7;
    EXPECT_EQ(BIYOS_getnsosvarsp(model.get(), 1, &unknown, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: set -1: unknown");
    EXPECT_EQ(BIYOS_getnsosvarrg(model.get(), -1, 0, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: set -1: unknown");
    EXPECT_EQ(nnz, 7);
    EXPECT_EQ(n_members(model, 3, 0), 0);
    expect_special_ordered_sets(read, sized(2, 5));
}

TEST(CApiTest, SetOutputs)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_getsosrg(held, 0, 3, 5, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    SpecialOrderedSets read = sized(3, 5);
    const SpecialOrderedSets untouched = read;
    EXPECT_EQ(BIYOS_getsosrg(held, 0, 3, 5, nullptr, read.counts.data(), nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(read.counts, kSample.counts);
    EXPECT_EQ(BIYOS_getsosrg(held, 0, 3, 5, read.kinds.data(), nullptr, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(read.kinds, kSample.kinds);
    const BIYOS_index indices[] = {2};
    EXPECT_EQ(BIYOS_getsossp(held, 1, indices, 2, nullptr, nullptr, nullptr, read.weights.data()),
              BIYOS_OK);
    expect_doubles(read.weights, {0.5, 1.5, 9.0, 9.0, 9.0});
    EXPECT_EQ(read.variables, untouched.variables);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(BIYOS_getsosrg(held, 0, 3, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 0 of the 5 members");
    EXPECT_EQ(BIYOS_getsosrg(held, 0, 3, 3, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 3 of the 5 members");
}

TEST(CApiTest, LockedSets)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    add_variables(model, 1);
    const int kind = BIYOS_SOSKND_TWO;
    ASSERT_EQ(BIYOS_addsos(model.get(), 1, &kind, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status> statuses{refused, BIYOS_OK, BIYOS_OK, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(n_special_ordered_sets(model), 1);
}
