#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "handles.hpp"
#include "model/compare.hpp"
#include "sample.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{

using biyos::test::below;
using biyos::test::build_sample;
using biyos::test::Engine;
using biyos::test::Env;
using biyos::test::Model;

constexpr std::array<std::uint32_t, 8> kSeeds{1, 2, 3, 5, 8, 13, 21, 34};
constexpr double kLargest = (std::numeric_limits<double>::max)();

struct Entries
{
    std::vector<BIYOS_index> functions;
    std::vector<double> values;
};

int n_functions(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnfun(model.get(), &n), BIYOS_OK);
    return n;
}

std::vector<double> constants_of(const Model &model)
{
    std::vector<double> constants(static_cast<std::size_t>(n_functions(model)));
    const auto n = static_cast<BIYOS_index>(constants.size());
    EXPECT_EQ(BIYOS_getcstrg(model.get(), 0, n, constants.data()), BIYOS_OK);
    return constants;
}

std::vector<std::uint64_t> bits(const std::vector<double> &values)
{
    std::vector<std::uint64_t> found;
    for (const double value : values)
    {
        found.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return found;
}

Entries drawn(Engine &engine, int functions)
{
    Entries entries;
    const int n = below(engine, 2 * functions + 1);
    for (int k = 0; k < n; ++k)
    {
        entries.functions.push_back(below(engine, functions));
        entries.values.push_back((below(engine, 17) - 8) / 4.0);
    }
    return entries;
}

std::vector<double> summed(std::vector<double> constants, const Entries &entries)
{
    for (std::size_t k = 0; k < entries.functions.size(); ++k)
    {
        constants[static_cast<std::size_t>(entries.functions[k])] += entries.values[k];
    }
    return constants;
}

BIYOS_status add(const Model &model, const Entries &entries)
{
    const auto n = static_cast<BIYOS_index>(entries.functions.size());
    return BIYOS_addcstsp(model.get(), n, entries.functions.data(), entries.values.data());
}

BIYOS_status set(const Model &model, const Entries &entries)
{
    const auto n = static_cast<BIYOS_index>(entries.functions.size());
    return BIYOS_setcstsp(model.get(), n, entries.functions.data(), entries.values.data());
}

void expect_refused(const Env &env, const Model &model, const Entries &entries)
{
    const Model tried(env, model);
    EXPECT_EQ(add(tried, entries), BIYOS_ERR_BADARG);
    EXPECT_EQ(biyos::difference(tried.get()->core, model.get()->core), "");
}

}

TEST(ConstantTest, Sums)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        const Entries entries = drawn(engine, n_functions(model));
        const std::vector<double> wanted = summed(constants_of(model), entries);
        ASSERT_EQ(add(model, entries), BIYOS_OK);
        EXPECT_EQ(bits(constants_of(model)), bits(wanted));
    }
}

TEST(ConstantTest, LastWins)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addfun(model.get(), 2, nullptr), BIYOS_OK);
    ASSERT_EQ(set(model, {.functions = {0, 1, 0}, .values = {1.0, 2.0, 3.0}}), BIYOS_OK);
    EXPECT_EQ(bits(constants_of(model)), bits({3.0, 2.0}));
    ASSERT_EQ(add(model, {.functions = {1, 1}, .values = {0.5, 0.25}}), BIYOS_OK);
    EXPECT_EQ(bits(constants_of(model)), bits({3.0, 2.75}));
}

TEST(ConstantTest, Refusals)
{
    const Env env;
    const Model model(env);
    const std::vector<double> held{kLargest, -1.0};
    ASSERT_EQ(BIYOS_addfun(model.get(), 2, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcstrg(model.get(), 0, 2, held.data()), BIYOS_OK);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    expect_refused(env, model, {.functions = {1, 0}, .values = {1.0, nan}});
    EXPECT_EQ(env.message(), "invalid argument: function 0: constant is NaN");
    expect_refused(env, model, {.functions = {1, 0, 0}, .values = {1.0, -1.0, kLargest}});
    EXPECT_EQ(env.message(), "invalid argument: function 0: constant sum is infinity");
    expect_refused(env, model, {.functions = {1, 2}, .values = {1.0, 1.0}});
    EXPECT_EQ(env.message(), "invalid argument: function 2: unknown");
    const Model tried(env, model);
    EXPECT_EQ(set(tried, {.functions = {0, 1}, .values = {1.0, -nan}}), BIYOS_ERR_BADARG);
    EXPECT_EQ(biyos::difference(tried.get()->core, model.get()->core), "");
}

TEST(ConstantTest, NullValues)
{
    const Env env;
    const Model model(env);
    const std::vector<double> held{1.5, -1.0};
    ASSERT_EQ(BIYOS_addfun(model.get(), 2, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcstrg(model.get(), 0, 2, held.data()), BIYOS_OK);
    const std::array<BIYOS_index, 2> listed{1, 0};
    EXPECT_EQ(BIYOS_addcstsp(model.get(), 2, listed.data(), nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_setcstrg(model.get(), 0, 2, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getcstrg(model.get(), 0, 2, nullptr), BIYOS_OK);
    EXPECT_EQ(bits(constants_of(model)), bits(held));
    EXPECT_EQ(BIYOS_addcstrg(model.get(), 1, 2, nullptr), BIYOS_ERR_BADARG);
    const Model tried(env, model);
    const std::array<BIYOS_index, 2> unknown{0, 2};
    EXPECT_EQ(BIYOS_addcstsp(tried.get(), 2, unknown.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 2: unknown");
    EXPECT_EQ(BIYOS_setcstsp(tried.get(), 2, unknown.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function 2: unknown");
    EXPECT_EQ(biyos::difference(tried.get()->core, model.get()->core), "");
}
