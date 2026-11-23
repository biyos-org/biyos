#include "biyos/biyos.h"
#include "handles.hpp"
#include "tolerance.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

using Indices = std::vector<BIYOS_index>;
using Offsets = std::vector<BIYOS_offset>;

struct Cones
{
    std::vector<BIYOS_int> kinds;
    Offsets starts;
    Indices functions;
    Offsets parameter_starts;
    std::vector<double> parameters;
};

const Cones kSample{
    .kinds = {BIYOS_CONE_SOC, BIYOS_CONE_POW, BIYOS_CONE_PSD},
    .starts = {0, 2, 5, 8},
    .functions = {3, 0, 0, 1, 2, 1, 2, 3},
    .parameter_starts = {0, 0, 2, 2},
    .parameters = {0.25, 0.75},
};

BIYOS_status add_cones(const Model &model, const Cones &cones, BIYOS_index *start = nullptr)
{
    return BIYOS_addcone(model.get(),
                         static_cast<BIYOS_index>(cones.kinds.size()),
                         cones.kinds.data(),
                         cones.starts.data(),
                         cones.functions.data(),
                         cones.parameter_starts.data(),
                         cones.parameters.data(),
                         start);
}

void add_sample(const Model &model)
{
    ASSERT_EQ(BIYOS_addfun(model.get(), 4, nullptr), BIYOS_OK);
    ASSERT_EQ(add_cones(model, kSample), BIYOS_OK);
}

BIYOS_index n_cones(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getncone(model.get(), &n), BIYOS_OK);
    return n;
}

Cones sized(std::size_t n, BIYOS_offset members, BIYOS_offset parameters)
{
    return Cones{
        .kinds = std::vector<BIYOS_int>(n, -9),
        .starts = Offsets(n + 1, -9),
        .functions = Indices(static_cast<std::size_t>(members), -9),
        .parameter_starts = Offsets(n + 1, -9),
        .parameters = std::vector<double>(static_cast<std::size_t>(parameters), -9.0),
    };
}

Cones read_range(const Model &model, BIYOS_index start, BIYOS_index size)
{
    BIYOS_offset members = -1;
    BIYOS_offset parameters = -1;
    EXPECT_EQ(BIYOS_getnconefunrg(model.get(), start, size, &members), BIYOS_OK);
    EXPECT_EQ(BIYOS_getnconeprmrg(model.get(), start, size, &parameters), BIYOS_OK);
    Cones read = sized(static_cast<std::size_t>(size), members, parameters);
    const BIYOS_status cones = BIYOS_getconerg(model.get(),
                                               start,
                                               size,
                                               members,
                                               read.kinds.data(),
                                               read.starts.data(),
                                               read.functions.data());
    EXPECT_EQ(cones, BIYOS_OK);
    const BIYOS_status weights = BIYOS_getconeprmrg(model.get(),
                                                    start,
                                                    size,
                                                    parameters,
                                                    read.parameter_starts.data(),
                                                    read.parameters.data());
    EXPECT_EQ(weights, BIYOS_OK);
    return read;
}

Cones read_listed(const Model &model, const Indices &indices)
{
    const auto n = static_cast<BIYOS_index>(indices.size());
    const BIYOS_index *listed = indices.data();
    BIYOS_offset members = -1;
    BIYOS_offset parameters = -1;
    EXPECT_EQ(BIYOS_getnconefunsp(model.get(), n, listed, &members), BIYOS_OK);
    EXPECT_EQ(BIYOS_getnconeprmsp(model.get(), n, listed, &parameters), BIYOS_OK);
    Cones read = sized(indices.size(), members, parameters);
    const BIYOS_status cones = BIYOS_getconesp(model.get(),
                                               n,
                                               listed,
                                               members,
                                               read.kinds.data(),
                                               read.starts.data(),
                                               read.functions.data());
    EXPECT_EQ(cones, BIYOS_OK);
    const BIYOS_status weights = BIYOS_getconeprmsp(model.get(),
                                                    n,
                                                    listed,
                                                    parameters,
                                                    read.parameter_starts.data(),
                                                    read.parameters.data());
    EXPECT_EQ(weights, BIYOS_OK);
    return read;
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

void expect_cones(const Cones &found, const Cones &wanted)
{
    EXPECT_EQ(found.kinds, wanted.kinds);
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.functions, wanted.functions);
    EXPECT_EQ(found.parameter_starts, wanted.parameter_starts);
    EXPECT_EQ(bits_of(found.parameters), bits_of(wanted.parameters));
}

Cones one(BIYOS_int kind, Indices functions, std::vector<double> parameters = {})
{
    const auto members = static_cast<BIYOS_offset>(functions.size());
    const auto weights = static_cast<BIYOS_offset>(parameters.size());
    return Cones{
        .kinds = {kind},
        .starts = {0, members},
        .functions = std::move(functions),
        .parameter_starts = {0, weights},
        .parameters = std::move(parameters),
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
    attempt.messages.emplace_back(biyos::test::last_message(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    const BIYOS_int kind = BIYOS_CONE_NONNEG;
    const BIYOS_offset starts[] = {0, 1};
    const BIYOS_index index = 0;
    BIYOS_index n = 0;
    BIYOS_offset nnz = 0;
    note(attempt, BIYOS_addcone(model, 1, &kind, starts, &index, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getncone(model, &n));
    note(attempt, BIYOS_getnconefunsp(model, 1, &index, &nnz));
    note(attempt, BIYOS_getnconeprmrg(model, 0, 1, &nnz));
    note(attempt, BIYOS_getconesp(model, 1, &index, 2, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_getconeprmrg(model, 1, 1, 2, nullptr, nullptr));
}

}

TEST(CApiTest, AddedCones)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addfun(model.get(), 4, nullptr), BIYOS_OK);
    BIYOS_index start = -1;
    ASSERT_EQ(add_cones(model, kSample, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    EXPECT_EQ(n_cones(model), 3);
    expect_cones(read_range(model, 0, 3), kSample);
    ASSERT_EQ(add_cones(model, one(BIYOS_CONE_DEXP, {2, 0, 1}), &start), BIYOS_OK);
    EXPECT_EQ(start, 3);
    EXPECT_EQ(n_cones(model), 4);
    expect_cones(read_range(model, 3, 1), one(BIYOS_CONE_DEXP, {2, 0, 1}));
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ConeRange)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Cones wanted{
        .kinds = {BIYOS_CONE_POW, BIYOS_CONE_PSD},
        .starts = {0, 3, 6},
        .functions = {0, 1, 2, 1, 2, 3},
        .parameter_starts = {0, 2, 2},
        .parameters = {0.25, 0.75},
    };
    expect_cones(read_range(model, 1, 2), wanted);
    const Cones none{
        .kinds = {},
        .starts = {0},
        .functions = {},
        .parameter_starts = {0},
        .parameters = {},
    };
    expect_cones(read_range(model, 3, 0), none);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ListedCones)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Cones wanted{
        .kinds = {BIYOS_CONE_PSD, BIYOS_CONE_POW, BIYOS_CONE_SOC, BIYOS_CONE_POW},
        .starts = {0, 3, 6, 8, 11},
        .functions = {1, 2, 3, 0, 1, 2, 3, 0, 0, 1, 2},
        .parameter_starts = {0, 0, 2, 2, 4},
        .parameters = {0.25, 0.75, 0.25, 0.75},
    };
    expect_cones(read_listed(model, {2, 1, 0, 1}), wanted);
    expect_cones(read_listed(model, {}), read_range(model, 0, 0));
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, ConeCapacity)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Cones read = sized(3, 8, 2);
    const BIYOS_index listed[] = {0, 1, 2};
    BIYOS_int *kinds = read.kinds.data();
    EXPECT_EQ(BIYOS_getconerg(model.get(), 0, 3, 7, kinds, read.starts.data(), nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 7 of the 8 members");
    EXPECT_EQ(BIYOS_getconesp(model.get(), 3, listed, -1, kinds, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': -1 is negative");
    double *weights = read.parameters.data();
    EXPECT_EQ(BIYOS_getconeprmsp(model.get(), 3, listed, 1, nullptr, weights), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 1 of the 2 parameters");
    expect_cones(read, sized(3, 8, 2));
    EXPECT_EQ(BIYOS_getconerg(model.get(), 0, 3, 8, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getconeprmrg(model.get(), 1, 1, 9, nullptr, weights), BIYOS_OK);
    EXPECT_EQ(bits_of(read.parameters), bits_of(kSample.parameters));
}

TEST(CApiTest, NullCones)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addfun(model.get(), 3, nullptr), BIYOS_OK);
    const BIYOS_int kind = BIYOS_CONE_SOC;
    const BIYOS_offset starts[] = {0, 2};
    const BIYOS_index functions[] = {1, 2};
    BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_addcone(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_addcone(held, 1, &kind, starts, functions, nullptr, nullptr, nullptr),
              BIYOS_OK);
    expect_cones(read_range(model, 0, 1), one(BIYOS_CONE_SOC, {1, 2}));
    EXPECT_EQ(BIYOS_addcone(held, 1, nullptr, starts, functions, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'knd': is null");
    EXPECT_EQ(BIYOS_addcone(held, 1, &kind, nullptr, functions, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': is null");
    EXPECT_EQ(BIYOS_addcone(held, 1, &kind, starts, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': is null");
    const BIYOS_int power = BIYOS_CONE_POW;
    const BIYOS_offset weights[] = {0, 1};
    EXPECT_EQ(BIYOS_addcone(held, 1, &power, starts, functions, weights, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'prm': is null");
    BIYOS_offset nnz = 0;
    EXPECT_EQ(BIYOS_getnconefunrg(held, 0, 1, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
    EXPECT_EQ(BIYOS_getnconeprmsp(held, 1, nullptr, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_getncone(held, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
    EXPECT_EQ(n_cones(model), 1);
}

TEST(CApiTest, RefusedCones)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Cones shifted = kSample;
    shifted.starts[0] = 1;
    EXPECT_EQ(add_cones(model, shifted), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': begins at 1, not 0");
    Cones falling = kSample;
    falling.parameter_starts[2] = -1;
    EXPECT_EQ(add_cones(model, falling), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: argument 'prmoff': decreases from 0 to -1 at entry 2");
    EXPECT_EQ(add_cones(model, one(9, {0})), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: cone 3: kind 9 is unknown");
    EXPECT_EQ(add_cones(model, one(BIYOS_CONE_EXP, {0, 1})), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: cone 3: dimension 2 is not 3");
    EXPECT_EQ(add_cones(model, one(BIYOS_CONE_SOC, {0, 4})), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: cone 3: function 4 is unknown");
    EXPECT_EQ(add_cones(model, one(BIYOS_CONE_DPOW, {0, 1}, {0.5, 0.0})), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: cone 3: parameter 1 is 0, not positive");
    const BIYOS_index unknown[] = {0, 3};
    BIYOS_offset nnz = 0;
    EXPECT_EQ(BIYOS_getnconefunsp(model.get(), 2, unknown, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: cone 3: unknown");
    EXPECT_EQ(BIYOS_getconerg(model.get(), 2, 2, 9, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: cone 3: unknown");
    EXPECT_EQ(BIYOS_getconeprmrg(model.get(), -1, 1, 9, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: cone -1: unknown");
    EXPECT_EQ(BIYOS_addcone(model.get(), -1, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(n_cones(model), 3);
    expect_cones(read_range(model, 0, 3), kSample);
}

TEST(CApiTest, LockedCones)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG};
    statuses.resize(6, BIYOS_OK);
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, std::vector<std::string>(6, locked));
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(n_cones(model), 3);
}

TEST(CApiTest, ConeTolerances)
{
    biyos::test::check_tolerances(biyos::test::kConeTolerances, add_sample);
}
