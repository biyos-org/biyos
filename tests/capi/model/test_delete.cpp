#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"
#include "model/compare/compare.hpp"
#include "tolerance.hpp"

#include <gtest/gtest.h>

#include <array>
#include <numeric>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;
using biyos::test::ToleranceCalls;
using Map = std::vector<BIYOS_index>;
using Indices = std::vector<BIYOS_index>;
using Counter = BIYOS_status (*)(const BIYOS_model *, BIYOS_index *);
using Listed = BIYOS_status (*)(BIYOS_model *, BIYOS_index, const BIYOS_index *, BIYOS_index *);
using Ranged = BIYOS_status (*)(BIYOS_model *, BIYOS_index, BIYOS_index, BIYOS_index *);
using Namer = BIYOS_status (*)(BIYOS_model *, BIYOS_index, BIYOS_index, const char *const *);
using Finder = BIYOS_status (*)(const BIYOS_model *,
                                BIYOS_index,
                                const char *const *,
                                BIYOS_index *);

struct Calls
{
    std::string noun;
    Counter count;
    Listed listed;
    Ranged ranged;
    Namer name;
    Finder find;
};

const std::array<Calls, 6> kCalls{{
    {
        .noun = "constraint",
        .count = BIYOS_getncon,
        .listed = BIYOS_delconsp,
        .ranged = BIYOS_delconrg,
        .name = BIYOS_setconnamerg,
        .find = BIYOS_getconidx,
    },
    {
        .noun = "objective",
        .count = BIYOS_getnobj,
        .listed = BIYOS_delobjsp,
        .ranged = BIYOS_delobjrg,
        .name = BIYOS_setobjnamerg,
        .find = BIYOS_getobjidx,
    },
    {
        .noun = "indicator",
        .count = BIYOS_getnind,
        .listed = BIYOS_delindsp,
        .ranged = BIYOS_delindrg,
        .name = BIYOS_setindnamerg,
        .find = BIYOS_getindidx,
    },
    {
        .noun = "set",
        .count = BIYOS_getnsos,
        .listed = BIYOS_delsossp,
        .ranged = BIYOS_delsosrg,
        .name = BIYOS_setsosnamerg,
        .find = BIYOS_getsosidx,
    },
    {
        .noun = "cone",
        .count = BIYOS_getncone,
        .listed = BIYOS_delconesp,
        .ranged = BIYOS_delconerg,
        .name = BIYOS_setconenamerg,
        .find = BIYOS_getconeidx,
    },
    {
        .noun = "complement",
        .count = BIYOS_getnccon,
        .listed = BIYOS_delcconsp,
        .ranged = BIYOS_delcconrg,
        .name = BIYOS_setcconnamerg,
        .find = BIYOS_getcconidx,
    },
}};

constexpr std::array<BIYOS_index, 3> kThree{0, 1, 2};
constexpr std::array<BIYOS_offset, 3> kCounts{1, 1, 1};
constexpr std::array<const char *, 3> kNames{"r0", "r1", "r2"};

void add_parts(const Model &model)
{
    const std::array<BIYOS_int, 3> kinds{BIYOS_VARKND_BIN, BIYOS_VARKND_BIN, BIYOS_VARKND_BIN};
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, kinds.data(), nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 3, nullptr), BIYOS_OK);
}

void add_records(const Model &model)
{
    BIYOS_model *held = model.get();
    const BIYOS_index *three = kThree.data();
    const std::array<BIYOS_int, 3> values{1, 0, 1};
    const std::array<BIYOS_int, 3> sets{BIYOS_SOSKND_ONE, BIYOS_SOSKND_ONE, BIYOS_SOSKND_ONE};
    const std::array<BIYOS_int, 3> cones{BIYOS_CONEKND_NONNEG,
                                         BIYOS_CONEKND_NONNEG,
                                         BIYOS_CONEKND_NONNEG};
    const std::array<BIYOS_index, 3> partners{1, 2, 0};
    ASSERT_EQ(BIYOS_addcon(held, 3, three, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addobj(held, 3, three, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addind(held, 3, three, values.data(), three, nullptr, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_addsos(held, 3, sets.data(), kCounts.data(), three, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(
        BIYOS_addcone(held, 3, cones.data(), kCounts.data(), three, nullptr, nullptr, nullptr),
        BIYOS_OK);
    ASSERT_EQ(BIYOS_addccon(held, 3, three, partners.data(), nullptr, nullptr, nullptr), BIYOS_OK);
}

void add_names(const Model &model)
{
    for (const Calls &calls : kCalls)
    {
        ASSERT_EQ(calls.name(model.get(), 0, 3, kNames.data()), BIYOS_OK);
    }
}

void build(const Model &model)
{
    add_parts(model);
    add_records(model);
    add_names(model);
}

int count_of(const Model &model, const Calls &calls)
{
    BIYOS_index n = -1;
    EXPECT_EQ(calls.count(model.get(), &n), BIYOS_OK);
    return n;
}

std::string find(const Env &env, const Model &model, const Calls &calls, const char *name)
{
    BIYOS_index index = -1;
    if (calls.find(model.get(), 1, &name, &index) != BIYOS_OK)
    {
        return env.message();
    }
    return std::to_string(index);
}

std::string unknown(const Calls &calls, const std::string &name)
{
    return "invalid argument: " + calls.noun + " '" + name + "': unknown";
}

std::string refused(const Calls &calls, const std::string &reason)
{
    return "invalid argument: " + calls.noun + " " + reason;
}

void expect_unchanged(const Model &tried, const Model &model)
{
    EXPECT_EQ(biyos::difference(tried.get()->model(), model.get()->model()), "");
}

void expect_listed(const Calls &calls)
{
    const Env env;
    const Model model(env);
    build(model);
    const Indices indices{2, 0};
    Map map(3, 9);
    EXPECT_EQ(calls.listed(model.get(), 2, indices.data(), map.data()), BIYOS_OK);
    EXPECT_EQ(map, (Map{-1, 0, -1}));
    EXPECT_EQ(count_of(model, calls), 1);
    EXPECT_EQ(find(env, model, calls, "r1"), "0");
    EXPECT_EQ(find(env, model, calls, "r0"), unknown(calls, "r0"));
    EXPECT_EQ(find(env, model, calls, "r2"), unknown(calls, "r2"));
}

void expect_ranged(const Calls &calls)
{
    const Env env;
    const Model model(env);
    build(model);
    Map map(3, 9);
    EXPECT_EQ(calls.ranged(model.get(), 1, 3, map.data()), BIYOS_OK);
    EXPECT_EQ(map, (Map{0, -1, -1}));
    EXPECT_EQ(count_of(model, calls), 1);
    EXPECT_EQ(find(env, model, calls, "r0"), "0");
    EXPECT_EQ(find(env, model, calls, "r1"), unknown(calls, "r1"));
}

void expect_unmapped(const Calls &calls)
{
    const Env env;
    const Model model(env);
    build(model);
    const BIYOS_index middle = 1;
    EXPECT_EQ(calls.listed(model.get(), 1, &middle, nullptr), BIYOS_OK);
    EXPECT_EQ(count_of(model, calls), 2);
    EXPECT_EQ(find(env, model, calls, "r2"), "1");
    EXPECT_EQ(calls.ranged(model.get(), 2, 2, nullptr), BIYOS_OK);
    EXPECT_EQ(calls.ranged(model.get(), 0, 2, nullptr), BIYOS_OK);
    EXPECT_EQ(count_of(model, calls), 0);
    EXPECT_EQ(calls.listed(model.get(), 0, nullptr, nullptr), BIYOS_OK);
}

std::string listed_refusal(const Env &env, const Model &model, const Calls &calls, Indices indices)
{
    const Model tried(env, model);
    Map map(3, 9);
    const auto n = static_cast<BIYOS_index>(indices.size());
    EXPECT_EQ(calls.listed(tried.get(), n, indices.data(), map.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(map, Map(3, 9));
    expect_unchanged(tried, model);
    return env.message();
}

std::string ranged_refusal(const Env &env,
                           const Model &model,
                           const Calls &calls,
                           BIYOS_index beg,
                           BIYOS_index end)
{
    const Model tried(env, model);
    Map map(3, 9);
    EXPECT_EQ(calls.ranged(tried.get(), beg, end, map.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(map, Map(3, 9));
    expect_unchanged(tried, model);
    return env.message();
}

void expect_refused(const Calls &calls)
{
    const Env env;
    const Model model(env);
    build(model);
    const std::string repeated =
        "invalid argument: argument 'idx': " + calls.noun + " 1 is repeated";
    EXPECT_EQ(listed_refusal(env, model, calls, {1, 0, 1}), repeated);
    EXPECT_EQ(listed_refusal(env, model, calls, {0, 3}), refused(calls, "3: unknown"));
    EXPECT_EQ(listed_refusal(env, model, calls, {-1}), refused(calls, "-1: unknown"));
    EXPECT_EQ(ranged_refusal(env, model, calls, 2, 4), refused(calls, "3: unknown"));
    EXPECT_EQ(ranged_refusal(env, model, calls, -1, 0), refused(calls, "-1: unknown"));
    EXPECT_EQ(ranged_refusal(env, model, calls, 1, 0),
              "invalid argument: argument 'end': 0 is below 'beg' 1");
    const Model tried(env, model);
    EXPECT_EQ(calls.listed(tried.get(), 1, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(calls.listed(tried.get(), -1, kThree.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(calls.listed(nullptr, 0, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(calls.ranged(nullptr, 0, 0, nullptr), BIYOS_ERR_BADARG);
    expect_unchanged(tried, model);
}

const Calls kFunctions{
    .noun = "function",
    .count = BIYOS_getnfun,
    .listed = BIYOS_delfunsp,
    .ranged = BIYOS_delfunrg,
    .name = BIYOS_setfunnamerg,
    .find = BIYOS_getfunidx,
};

void build_functions(const Model &model)
{
    const BIYOS_index used = 3;
    ASSERT_EQ(BIYOS_addfun(model.get(), 4, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setfunnamerg(model.get(), 0, 3, kNames.data()), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(model.get(), 1, &used, nullptr, nullptr, nullptr), BIYOS_OK);
}

BIYOS_index function_of(const Model &model)
{
    BIYOS_index function = -1;
    EXPECT_EQ(BIYOS_getconrg(model.get(), 0, 1, &function, nullptr, nullptr), BIYOS_OK);
    return function;
}

std::string in_use(const Env &env, const Model &model, Listed listed, Indices indices)
{
    const Model tried(env, model);
    Map map(4, 9);
    const auto n = static_cast<BIYOS_index>(indices.size());
    EXPECT_EQ(listed(tried.get(), n, indices.data(), map.data()), BIYOS_ERR_INUSE);
    EXPECT_EQ(map, Map(4, 9));
    expect_unchanged(tried, model);
    return env.message();
}

struct Spread
{
    std::vector<BIYOS_real> lower;
    std::vector<BIYOS_real> absolute;
    std::vector<BIYOS_real> coefficients;
    std::vector<const char *> names;
    BIYOS_index member;
};

const Spread kFour{
    .lower = {0.0, 1.0, 2.0, 3.0},
    .absolute = {0.5, 0.25, 0.125, 0.0625},
    .coefficients = {1.0, 2.0, 3.0, 4.0},
    .names = {"r0", "r1", "r2"},
    .member = 3,
};

const Spread kTwo{
    .lower = {1.0, 3.0},
    .absolute = {0.25, 0.0625},
    .coefficients = {2.0, 4.0},
    .names = {"r1"},
    .member = 1,
};

const Spread kOne{
    .lower = {3.0},
    .absolute = {0.0625},
    .coefficients = {4.0},
    .names = {},
    .member = 0,
};

void build_variables(const Model &model, const Spread &spread)
{
    BIYOS_model *held = model.get();
    const auto n = static_cast<BIYOS_index>(spread.lower.size());
    const auto named = static_cast<BIYOS_index>(spread.names.size());
    const std::vector<BIYOS_index> functions(spread.lower.size(), 0);
    std::vector<BIYOS_index> variables(spread.lower.size());
    std::iota(variables.begin(), variables.end(), 0);
    const BIYOS_int kind = BIYOS_SOSKND_ONE;
    const BIYOS_offset count = 1;
    const BIYOS_real *coefficients = spread.coefficients.data();
    ASSERT_EQ(BIYOS_addvar(held, n, nullptr, spread.lower.data(), nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarnamerg(held, 0, named, spread.names.data()), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvartolrg(held, 0, n, spread.absolute.data(), nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(held, 1, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addlinterm(held, n, functions.data(), variables.data(), coefficients),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_addsos(held, 1, &kind, &count, &spread.member, nullptr, nullptr), BIYOS_OK);
}

void expect_built(const Env &env, const Model &model, const Spread &spread)
{
    const Model wanted(env);
    build_variables(wanted, spread);
    EXPECT_EQ(biyos::difference(model.get()->model(), wanted.get()->model()), "");
}

void expect_moved(const ToleranceCalls &tolerances, Listed listed)
{
    SCOPED_TRACE(tolerances.noun);
    const Env env;
    const Model model(env);
    build(model);
    const std::array<double, 3> absolute{0.5, 0.25, 0.125};
    const BIYOS_index last = 2;
    const double relative = 0.75;
    ASSERT_EQ(tolerances.set_ranged(model.get(), 0, 3, absolute.data(), nullptr), BIYOS_OK);
    ASSERT_EQ(tolerances.set_listed(model.get(), 1, &last, nullptr, &relative), BIYOS_OK);
    const BIYOS_index first = 0;
    ASSERT_EQ(listed(model.get(), 1, &first, nullptr), BIYOS_OK);
    const biyos::test::Tolerances wanted{
        .absolute = {0.25, 0.125},
        .relative = {biyos::test::kUnknown, 0.75},
    };
    biyos::test::expect_tolerances(biyos::test::read_tolerances(model, tolerances), wanted);
}

}

TEST(CApiTest, DeleteListed)
{
    for (const Calls &calls : kCalls)
    {
        SCOPED_TRACE(calls.noun);
        expect_listed(calls);
    }
}

TEST(CApiTest, DeleteRanged)
{
    for (const Calls &calls : kCalls)
    {
        SCOPED_TRACE(calls.noun);
        expect_ranged(calls);
    }
}

TEST(CApiTest, NullMap)
{
    for (const Calls &calls : kCalls)
    {
        SCOPED_TRACE(calls.noun);
        expect_unmapped(calls);
    }
}

TEST(CApiTest, DeleteRefused)
{
    for (const Calls &calls : kCalls)
    {
        SCOPED_TRACE(calls.noun);
        expect_refused(calls);
    }
}

TEST(CApiTest, DeleteFunctions)
{
    const Env env;
    const Model model(env);
    build_functions(model);
    const Indices indices{2, 0};
    Map map(4, 9);
    EXPECT_EQ(BIYOS_delfunsp(model.get(), 2, indices.data(), map.data()), BIYOS_OK);
    EXPECT_EQ(map, (Map{-1, 0, -1, 1}));
    EXPECT_EQ(count_of(model, kFunctions), 2);
    EXPECT_EQ(function_of(model), 1);
    EXPECT_EQ(find(env, model, kFunctions, "r1"), "0");
    EXPECT_EQ(find(env, model, kFunctions, "r2"), unknown(kFunctions, "r2"));
    EXPECT_EQ(BIYOS_delfunrg(model.get(), 0, 1, nullptr), BIYOS_OK);
    EXPECT_EQ(count_of(model, kFunctions), 1);
    EXPECT_EQ(function_of(model), 0);
    EXPECT_EQ(find(env, model, kFunctions, "r1"), unknown(kFunctions, "r1"));
}

TEST(CApiTest, UsedFunction)
{
    const Env env;
    const Model model(env);
    build_functions(model);
    const std::string used = "in use: function 3: used by constraint 0";
    EXPECT_EQ(in_use(env, model, BIYOS_delfunsp, {3}), used);
    EXPECT_EQ(in_use(env, model, BIYOS_delfunsp, {1, 3}), used);
    const Model tried(env, model);
    Map map(4, 9);
    EXPECT_EQ(BIYOS_delfunrg(tried.get(), 2, 4, map.data()), BIYOS_ERR_INUSE);
    EXPECT_EQ(env.message(), used);
    EXPECT_EQ(map, Map(4, 9));
    expect_unchanged(tried, model);
    EXPECT_EQ(BIYOS_delfunrg(tried.get(), 3, 5, map.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused(kFunctions, "4: unknown"));
}

TEST(CApiTest, DeleteVariables)
{
    const Env env;
    const Model model(env);
    build_variables(model, kFour);
    const Indices indices{2, 0};
    Map map(4, 9);
    EXPECT_EQ(BIYOS_delvarsp(model.get(), 2, indices.data(), map.data()), BIYOS_OK);
    EXPECT_EQ(map, (Map{-1, 0, -1, 1}));
    expect_built(env, model, kTwo);
    EXPECT_EQ(BIYOS_delvarrg(model.get(), 0, 1, nullptr), BIYOS_OK);
    expect_built(env, model, kOne);
}

TEST(CApiTest, UsedVariable)
{
    const Env env;
    const Model model(env);
    build_variables(model, kFour);
    const std::string used = "in use: variable 3: used by set 0";
    EXPECT_EQ(in_use(env, model, BIYOS_delvarsp, {3}), used);
    EXPECT_EQ(in_use(env, model, BIYOS_delvarsp, {1, 3}), used);
    const Model tried(env, model);
    Map map(4, 9);
    EXPECT_EQ(BIYOS_delvarrg(tried.get(), 2, 4, map.data()), BIYOS_ERR_INUSE);
    EXPECT_EQ(env.message(), used);
    EXPECT_EQ(map, Map(4, 9));
    expect_unchanged(tried, model);
    EXPECT_EQ(BIYOS_delvarrg(tried.get(), 3, 5, map.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 4: unknown");
    EXPECT_EQ(BIYOS_delvarsp(tried.get(), 1, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delvarsp(tried.get(), -1, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delvarrg(nullptr, 0, 1, nullptr), BIYOS_ERR_BADARG);
    const BIYOS_index unknown = -1;
    EXPECT_EQ(BIYOS_delvarsp(tried.get(), 1, &unknown, nullptr), BIYOS_ERR_BADARG);
    expect_unchanged(tried, model);
    ASSERT_EQ(BIYOS_addvarnode(model.get(), 1, nullptr), BIYOS_OK);
    EXPECT_EQ(in_use(env, model, BIYOS_delvarsp, {1}), "in use: variable 1: used by node 0");
    const BIYOS_index function = 0;
    ASSERT_EQ(BIYOS_addcb(model.get(), 1, &function, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(in_use(env, model, BIYOS_delvarsp, {0}), "in use: variable 0: used by callback 0");
}

TEST(CApiTest, MovedTolerances)
{
    expect_moved(biyos::test::kConstraintTolerances, BIYOS_delconsp);
    expect_moved(biyos::test::kConeTolerances, BIYOS_delconesp);
}
