#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "handles.hpp"
#include "model/compare.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;
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
constexpr std::array<BIYOS_offset, 4> kStarts{0, 1, 2, 3};
constexpr std::array<const char *, 3> kNames{"r0", "r1", "r2"};

void add_parts(const Model &model)
{
    const std::array<BIYOS_int, 3> kinds{BIYOS_VAR_BIN, BIYOS_VAR_BIN, BIYOS_VAR_BIN};
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, kinds.data(), nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 3, nullptr), BIYOS_OK);
}

void add_records(const Model &model)
{
    BIYOS_model *held = model.get();
    const BIYOS_index *three = kThree.data();
    const std::array<BIYOS_int, 3> values{1, 0, 1};
    const std::array<BIYOS_int, 3> sets{BIYOS_SOSKND_SOS1, BIYOS_SOSKND_SOS1, BIYOS_SOSKND_SOS1};
    const std::array<BIYOS_int, 3> cones{BIYOS_CONE_NONNEG, BIYOS_CONE_NONNEG, BIYOS_CONE_NONNEG};
    const std::array<BIYOS_index, 3> partners{1, 2, 0};
    ASSERT_EQ(BIYOS_addcon(held, 3, three, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addobj(held, 3, three, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addind(held, 3, three, values.data(), three, nullptr, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_addsos(held, 3, sets.data(), kStarts.data(), three, nullptr, nullptr),
              BIYOS_OK);
    ASSERT_EQ(
        BIYOS_addcone(held, 3, cones.data(), kStarts.data(), three, nullptr, nullptr, nullptr),
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
    EXPECT_EQ(biyos::difference(tried.get()->core, model.get()->core), "");
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
    EXPECT_EQ(calls.ranged(model.get(), 1, 2, map.data()), BIYOS_OK);
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
    EXPECT_EQ(calls.ranged(model.get(), 2, 0, nullptr), BIYOS_OK);
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
                           BIYOS_index pos,
                           BIYOS_index n)
{
    const Model tried(env, model);
    Map map(3, 9);
    EXPECT_EQ(calls.ranged(tried.get(), pos, n, map.data()), BIYOS_ERR_BADARG);
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
    EXPECT_EQ(ranged_refusal(env, model, calls, 2, 2), refused(calls, "3: unknown"));
    EXPECT_EQ(ranged_refusal(env, model, calls, -1, 1), refused(calls, "-1: unknown"));
    EXPECT_EQ(ranged_refusal(env, model, calls, 0, -1),
              "invalid argument: argument 'n': -1 is negative");
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

std::string in_use(const Env &env, const Model &model, Indices indices)
{
    const Model tried(env, model);
    Map map(4, 9);
    const auto n = static_cast<BIYOS_index>(indices.size());
    EXPECT_EQ(BIYOS_delfunsp(tried.get(), n, indices.data(), map.data()), BIYOS_ERR_INUSE);
    EXPECT_EQ(map, Map(4, 9));
    expect_unchanged(tried, model);
    return env.message();
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
    EXPECT_EQ(in_use(env, model, {3}), used);
    EXPECT_EQ(in_use(env, model, {1, 3}), used);
    const Model tried(env, model);
    Map map(4, 9);
    EXPECT_EQ(BIYOS_delfunrg(tried.get(), 2, 2, map.data()), BIYOS_ERR_INUSE);
    EXPECT_EQ(env.message(), used);
    EXPECT_EQ(map, Map(4, 9));
    expect_unchanged(tried, model);
    EXPECT_EQ(BIYOS_delfunrg(tried.get(), 3, 2, map.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refused(kFunctions, "4: unknown"));
}
