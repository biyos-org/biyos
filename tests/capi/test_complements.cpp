#include "biyos/biyos.h"
#include "doubles.hpp"
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
using biyos::test::expect_doubles;
using biyos::test::Model;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

struct Records
{
    std::vector<int> functions;
    std::vector<int> multipliers;
    std::vector<double> lower;
    std::vector<double> upper;
};

const Records kSample{
    .functions = {1, 0, 1},
    .multipliers = {0, 0, 2},
    .lower = {0.0, -kInfinity, -1.0},
    .upper = {kInfinity, kInfinity, 1.0},
};

void add_functions(const Model &model, int n)
{
    ASSERT_EQ(BIYOS_addfun(model.get(), n, nullptr), BIYOS_OK);
}

void add_sample(const Model &model)
{
    add_functions(model, 3);
    const BIYOS_status status = BIYOS_addccon(model.get(),
                                              3,
                                              kSample.functions.data(),
                                              kSample.multipliers.data(),
                                              kSample.lower.data(),
                                              kSample.upper.data(),
                                              nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

int n_complements(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnccon(model.get(), &n), BIYOS_OK);
    return n;
}

Records sized(std::size_t size)
{
    return Records{
        .functions = std::vector<int>(size, -1),
        .multipliers = std::vector<int>(size, -1),
        .lower = std::vector<double>(size, 9.0),
        .upper = std::vector<double>(size, 9.0),
    };
}

Records read_range(const Model &model, int start, int n)
{
    Records read = sized(static_cast<std::size_t>(n));
    const BIYOS_status status = BIYOS_getcconrg(model.get(),
                                                start,
                                                n,
                                                read.functions.data(),
                                                read.multipliers.data(),
                                                read.lower.data(),
                                                read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Records read_all(const Model &model)
{
    return read_range(model, 0, n_complements(model));
}

Records read_listed(const Model &model, const std::vector<BIYOS_index> &indices)
{
    const auto n = static_cast<BIYOS_index>(indices.size());
    Records read = sized(indices.size());
    const BIYOS_status status = BIYOS_getcconsp(model.get(),
                                                n,
                                                indices.data(),
                                                read.functions.data(),
                                                read.multipliers.data(),
                                                read.lower.data(),
                                                read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void expect_records(const Records &found, const Records &wanted)
{
    EXPECT_EQ(found.functions, wanted.functions);
    EXPECT_EQ(found.multipliers, wanted.multipliers);
    expect_doubles(found.lower, wanted.lower);
    expect_doubles(found.upper, wanted.upper);
}

struct Tolerances
{
    std::vector<double> absolute;
    std::vector<double> relative;
};

const Tolerances kUnset{
    .absolute = {kNan, kNan, kNan},
    .relative = {kNan, kNan, kNan},
};

Tolerances read_tolerances(const Model &model)
{
    const int n = n_complements(model);
    const auto size = static_cast<std::size_t>(n);
    Tolerances read{
        .absolute = std::vector<double>(size, 9.0),
        .relative = std::vector<double>(size, 9.0),
    };
    const BIYOS_status status =
        BIYOS_getccontolrg(model.get(), 0, n, read.absolute.data(), read.relative.data());
    EXPECT_EQ(status, BIYOS_OK);
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

void expect_tolerances(const Tolerances &found, const Tolerances &wanted)
{
    EXPECT_EQ(bits_of(found.absolute), bits_of(wanted.absolute));
    EXPECT_EQ(bits_of(found.relative), bits_of(wanted.relative));
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
    note(attempt, BIYOS_addccon(model, 1, &index, &index, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_setcconbndsp(model, 1, &index, &bound, &bound));
    note(attempt, BIYOS_getcconbndsp(model, 1, &index, nullptr, nullptr));
    note(attempt, BIYOS_getnccon(model, &n));
    note(attempt, BIYOS_getcconrg(model, 0, 1, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_setccontolsp(model, 1, &index, &bound, &bound));
    note(attempt, BIYOS_getccontolsp(model, 1, &index, nullptr, nullptr));
    note(attempt, BIYOS_delccontolsp(model, 1, &index));
}

}

TEST(CApiTest, AddedComplements)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    EXPECT_EQ(n_complements(model), 3);
    expect_records(read_all(model), kSample);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstComplement)
{
    const Env env;
    const Model model(env);
    add_functions(model, 2);
    const int functions[] = {0, 1};
    BIYOS_model *held = model.get();
    BIYOS_index start = -1;
    ASSERT_EQ(BIYOS_addccon(held, 0, nullptr, nullptr, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addccon(held, 2, functions, functions, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addccon(held, 1, functions, functions + 1, nullptr, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_addccon(held, 1, functions + 1, functions, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 3);
    const BIYOS_index unknown = 2;
    EXPECT_EQ(BIYOS_addccon(held, 1, functions, &unknown, nullptr, nullptr, &start),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(start, 3);
    EXPECT_EQ(n_complements(model), 4);
}

TEST(CApiTest, NullComplements)
{
    const Env env;
    const Model model(env);
    add_functions(model, 2);
    const int functions[] = {0, 1};
    const double bounds[] = {-0.5, 0.75};
    BIYOS_model *held = model.get();
    ASSERT_EQ(BIYOS_addccon(held, 2, functions, functions, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addccon(held, 2, functions, functions, bounds, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addccon(held, 2, functions, functions, nullptr, bounds, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addccon(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Records wanted{
        .functions = {0, 1, 0, 1, 0, 1},
        .multipliers = {0, 1, 0, 1, 0, 1},
        .lower = {0.0, 0.0, -0.5, 0.75, 0.0, 0.0},
        .upper = {kInfinity, kInfinity, kInfinity, kInfinity, -0.5, 0.75},
    };
    expect_records(read_all(model), wanted);
    EXPECT_EQ(BIYOS_addccon(held, 1, nullptr, functions, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx1': is null");
    EXPECT_EQ(BIYOS_addccon(held, 1, functions, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx2': is null");
    EXPECT_EQ(n_complements(model), 6);
}

TEST(CApiTest, ComplementRange)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Records ranged{
        .functions = {0, 1},
        .multipliers = {0, 2},
        .lower = {-kInfinity, -1.0},
        .upper = {kInfinity, 1.0},
    };
    expect_records(read_range(model, 1, 2), ranged);
    EXPECT_EQ(BIYOS_getcconrg(model.get(), 3, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Records listed{
        .functions = {1, 1, 0},
        .multipliers = {2, 0, 0},
        .lower = {-1.0, 0.0, -kInfinity},
        .upper = {1.0, kInfinity, kInfinity},
    };
    expect_records(read_listed(model, {2, 0, 1}), listed);
    expect_records(read_listed(model, {}), Records{});
}

TEST(CApiTest, ComplementOutputs)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_getcconrg(held, 0, 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    std::vector<int> multipliers(2, -1);
    std::vector<double> upper(2, 9.0);
    EXPECT_EQ(BIYOS_getcconrg(held, 1, 2, nullptr, multipliers.data(), nullptr, upper.data()),
              BIYOS_OK);
    EXPECT_EQ(multipliers, std::vector<int>({0, 2}));
    expect_doubles(upper, {kInfinity, 1.0});
    const BIYOS_index indices[] = {2};
    std::vector<int> functions(1, -1);
    EXPECT_EQ(BIYOS_getcconsp(held, 1, indices, functions.data(), nullptr, nullptr, nullptr),
              BIYOS_OK);
    EXPECT_EQ(functions, std::vector<int>({1}));
    EXPECT_EQ(BIYOS_getnccon(held, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
}

TEST(CApiTest, ComplementBounds)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const BIYOS_index indices[] = {2, 0};
    const double lower[] = {-2.0, -3.0};
    const double upper[] = {2.0, 3.0};
    ASSERT_EQ(BIYOS_setcconbndsp(held, 2, indices, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcconbndrg(held, 1, 1, nullptr, upper + 1), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcconbndrg(held, 0, 3, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcconbndsp(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    std::vector<double> low(3, 9.0);
    std::vector<double> high(3, 9.0);
    ASSERT_EQ(BIYOS_getcconbndrg(held, 0, 3, low.data(), high.data()), BIYOS_OK);
    expect_doubles(low, {-3.0, -kInfinity, -2.0});
    expect_doubles(high, {kInfinity, 3.0, 1.0});
    std::vector<double> listed(3, 9.0);
    const BIYOS_index repeated[] = {2, 0, 2};
    ASSERT_EQ(BIYOS_getcconbndsp(held, 3, repeated, nullptr, listed.data()), BIYOS_OK);
    expect_doubles(listed, {1.0, kInfinity, 1.0});
    EXPECT_EQ(read_all(model).multipliers, kSample.multipliers);
    EXPECT_EQ(BIYOS_setcconbndsp(held, 2, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
}

TEST(CApiTest, RefusedComplements)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const int functions[] = {0, 3};
    const BIYOS_real nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(BIYOS_addccon(held, 2, functions, functions, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 4: function 3 is unknown");
    EXPECT_EQ(BIYOS_addccon(held, 1, functions, functions + 1, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 3: function 3 is unknown");
    EXPECT_EQ(BIYOS_addccon(held, 1, functions, functions, &nan, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 3: lower bound is NaN");
    EXPECT_EQ(BIYOS_addccon(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    const BIYOS_index index = 3;
    const BIYOS_real bound = 0.0;
    EXPECT_EQ(BIYOS_setcconbndsp(held, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 3: unknown");
    const BIYOS_index first = 0;
    const BIYOS_real infinite = kInfinity;
    EXPECT_EQ(BIYOS_setcconbndsp(held, 1, &first, &infinite, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 0: lower bound is infinity");
    EXPECT_EQ(BIYOS_getcconbndrg(held, 2, 2, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 3: unknown");
    EXPECT_EQ(BIYOS_getcconsp(held, 1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    expect_records(read_all(model), kSample);
}

TEST(CApiTest, LockedComplements)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    add_functions(model, 1);
    const BIYOS_index function = 0;
    ASSERT_EQ(BIYOS_addccon(model.get(), 1, &function, &function, nullptr, nullptr, nullptr),
              BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status> statuses{
        refused,
        refused,
        BIYOS_OK,
        BIYOS_OK,
        BIYOS_OK,
        refused,
        BIYOS_OK,
        refused,
    };
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0], locked);
    EXPECT_EQ(attempt.messages[1], locked);
    EXPECT_EQ(attempt.messages[5], locked);
    EXPECT_EQ(attempt.messages[7], locked);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(n_complements(model), 1);
}

TEST(CApiTest, ComplementTolerances)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    expect_tolerances(read_tolerances(model), kUnset);
    BIYOS_model *held = model.get();
    const BIYOS_index indices[] = {2, 0};
    const double absolute[] = {0.5, 0.0};
    const double relative[] = {0.25, 0.125};
    ASSERT_EQ(BIYOS_setccontolsp(held, 2, indices, absolute, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setccontolrg(held, 1, 2, nullptr, relative), BIYOS_OK);
    ASSERT_EQ(BIYOS_setccontolrg(held, 0, 3, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setccontolsp(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    const Tolerances wanted{
        .absolute = {0.0, kNan, 0.5},
        .relative = {kNan, 0.25, 0.125},
    };
    expect_tolerances(read_tolerances(model), wanted);
    const BIYOS_index repeated[] = {1, 2, 1};
    std::vector<double> listed(3, 9.0);
    ASSERT_EQ(BIYOS_getccontolsp(held, 3, repeated, nullptr, listed.data()), BIYOS_OK);
    EXPECT_EQ(bits_of(listed), bits_of({0.25, 0.125, 0.25}));
    EXPECT_EQ(BIYOS_getccontolsp(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    expect_records(read_all(model), kSample);
}

TEST(CApiTest, RefusedTolerances)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const BIYOS_index index = 1;
    const double nan = kNan;
    const double infinite = kInfinity;
    const double negative = -0.5;
    const double small = 0.5;
    EXPECT_EQ(BIYOS_setccontolsp(held, 1, &index, &nan, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 1: absolute tolerance is NaN");
    EXPECT_EQ(BIYOS_setccontolsp(held, 1, &index, nullptr, &infinite), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 1: relative tolerance is infinity");
    EXPECT_EQ(BIYOS_setccontolrg(held, 1, 1, &negative, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 1: absolute tolerance -0.5 is negative");
    const BIYOS_index unknown = 3;
    EXPECT_EQ(BIYOS_setccontolsp(held, 1, &unknown, &small, &small), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 3: unknown");
    EXPECT_EQ(BIYOS_setccontolsp(held, 1, nullptr, &small, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    const BIYOS_index indices[] = {0, 2};
    const double absolute[] = {0.5, kNan};
    EXPECT_EQ(BIYOS_setccontolsp(held, 2, indices, absolute, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 2: absolute tolerance is NaN");
    std::vector<double> kept(2, 9.0);
    EXPECT_EQ(BIYOS_getccontolrg(held, 2, 2, kept.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 3: unknown");
    EXPECT_EQ(bits_of(kept), bits_of({9.0, 9.0}));
    expect_tolerances(read_tolerances(model), kUnset);
}

TEST(CApiTest, DeletedTolerances)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const double absolute[] = {0.5, 0.25, 0.125};
    const double relative[] = {0.75, 0.375, 0.0};
    ASSERT_EQ(BIYOS_setccontolrg(held, 0, 3, absolute, relative), BIYOS_OK);
    const BIYOS_index indices[] = {2, 2};
    ASSERT_EQ(BIYOS_delccontolsp(held, 2, indices), BIYOS_OK);
    ASSERT_EQ(BIYOS_delccontolsp(held, 0, nullptr), BIYOS_OK);
    const Tolerances wanted{
        .absolute = {0.5, 0.25, kNan},
        .relative = {0.75, 0.375, kNan},
    };
    expect_tolerances(read_tolerances(model), wanted);
    ASSERT_EQ(BIYOS_delccontolrg(held, 0, 2), BIYOS_OK);
    ASSERT_EQ(BIYOS_delccontolrg(held, 1, 2), BIYOS_OK);
    expect_tolerances(read_tolerances(model), kUnset);
    expect_records(read_all(model), kSample);
}

TEST(CApiTest, RefusedDeletion)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    BIYOS_model *held = model.get();
    const double absolute[] = {0.5, 0.25, 0.125};
    ASSERT_EQ(BIYOS_setccontolrg(held, 0, 3, absolute, nullptr), BIYOS_OK);
    const Tolerances before = read_tolerances(model);
    const BIYOS_index indices[] = {0, 3};
    EXPECT_EQ(BIYOS_delccontolsp(held, 2, indices), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 3: unknown");
    EXPECT_EQ(BIYOS_delccontolsp(held, 1, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_delccontolrg(held, 2, 2), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: complement 3: unknown");
    expect_tolerances(read_tolerances(model), before);
}
