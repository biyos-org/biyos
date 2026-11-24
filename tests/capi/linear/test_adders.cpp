#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "handles.hpp"
#include "model/compare/compare.hpp"
#include "sample.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <string>
#include <utility>
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

using Indices = std::vector<BIYOS_index>;
using Key = std::pair<BIYOS_index, BIYOS_index>;
using Held = std::map<Key, std::uint64_t>;

struct Added
{
    Indices functions;
    Indices variables;
    std::vector<double> values;
};

struct Compressed
{
    std::vector<BIYOS_offset> starts{0};
    Indices others;
    std::vector<double> values;
};

int count_of(const Model &model, BIYOS_status (*count)(const BIYOS_model *, BIYOS_index *))
{
    BIYOS_index n = -1;
    EXPECT_EQ(count(model.get(), &n), BIYOS_OK);
    return n;
}

int n_functions(const Model &model)
{
    return count_of(model, BIYOS_getnfun);
}

int n_variables(const Model &model)
{
    return count_of(model, BIYOS_getnvar);
}

Added drawn(Engine &engine, int functions, int variables)
{
    Added added;
    for (int function = 0; function < functions; ++function)
    {
        for (int variable = 0; variable < variables; ++variable)
        {
            if (below(engine, 3) == 0)
            {
                added.functions.push_back(function);
                added.variables.push_back(variable);
                added.values.push_back((below(engine, 17) - 8) / 4.0);
            }
        }
    }
    return added;
}

Added shuffled(const Added &added, Engine &engine)
{
    std::vector<std::size_t> order(added.values.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    for (std::size_t k = order.size(); k > 1; --k)
    {
        std::swap(order[k - 1],
                  order[static_cast<std::size_t>(below(engine, static_cast<int>(k)))]);
    }
    Added found;
    for (const std::size_t k : order)
    {
        found.functions.push_back(added.functions[k]);
        found.variables.push_back(added.variables[k]);
        found.values.push_back(added.values[k]);
    }
    return found;
}

Compressed compressed(const Added &added, int count, bool by_row)
{
    const Indices &records = by_row ? added.functions : added.variables;
    const Indices &others = by_row ? added.variables : added.functions;
    Compressed found;
    for (int record = 0; record < count; ++record)
    {
        for (std::size_t k = 0; k < records.size(); ++k)
        {
            if (records[k] == record)
            {
                found.others.push_back(others[k]);
                found.values.push_back(added.values[k]);
            }
        }
        found.starts.push_back(static_cast<BIYOS_offset>(found.others.size()));
    }
    return found;
}

BIYOS_status add_listed(const Model &model, const Added &added)
{
    return BIYOS_addlintermsp(model.get(),
                              static_cast<BIYOS_offset>(added.values.size()),
                              added.functions.data(),
                              added.variables.data(),
                              added.values.data());
}

BIYOS_status add_rows(const Model &model, const Added &added)
{
    const int n = n_functions(model);
    const Compressed rows = compressed(added, n, true);
    const BIYOS_index *variables = rows.others.data();
    return BIYOS_addrlintermrg(model.get(),
                               0,
                               n,
                               rows.starts.data(),
                               variables,
                               rows.values.data());
}

BIYOS_status add_columns(const Model &model, const Added &added)
{
    const int n = n_variables(model);
    const Compressed columns = compressed(added, n, false);
    const BIYOS_index *functions = columns.others.data();
    const double *values = columns.values.data();
    return BIYOS_addclintermrg(model.get(), 0, n, columns.starts.data(), functions, values);
}

Indices every_function(const Model &model)
{
    Indices listed(static_cast<std::size_t>(n_functions(model)));
    std::iota(listed.begin(), listed.end(), 0);
    return listed;
}

Held held_of(const Model &model)
{
    const Indices every = every_function(model);
    const auto n = static_cast<BIYOS_index>(every.size());
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnrlintermsp(model.get(), n, every.data(), &nnz), BIYOS_OK);
    const auto size = static_cast<std::size_t>(nnz);
    Indices functions(size);
    Indices variables(size);
    std::vector<double> values(size);
    EXPECT_EQ(BIYOS_getrlintermsp(model.get(),
                                  n,
                                  every.data(),
                                  nnz,
                                  nullptr,
                                  functions.data(),
                                  variables.data(),
                                  values.data()),
              BIYOS_OK);
    Held held;
    for (std::size_t k = 0; k < size; ++k)
    {
        held[{functions[k], variables[k]}] = std::bit_cast<std::uint64_t>(values[k]);
    }
    return held;
}

Held summed(Held held, const Added &added)
{
    for (std::size_t k = 0; k < added.values.size(); ++k)
    {
        const Key key{added.functions[k], added.variables[k]};
        const auto found = held.find(key);
        const double before = found == held.end() ? 0.0 : std::bit_cast<double>(found->second);
        const double after = found == held.end() ? added.values[k] : before + added.values[k];
        held[key] = std::bit_cast<std::uint64_t>(after);
    }
    return held;
}

std::string difference(const Model &left, const Model &right)
{
    return biyos::difference(left.get()->core, right.get()->core);
}

void expect_kept(const Env &env,
                 const Model &model,
                 BIYOS_status (*add)(const Model &, const Added &),
                 const Added &added,
                 const std::string &message)
{
    const Model tried(env, model);
    EXPECT_EQ(add(tried, added), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), message);
    EXPECT_EQ(difference(tried, model), "");
}

}

TEST(AdderTest, Agree)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        const Added added = drawn(engine, n_functions(model), n_variables(model));
        const Model listed(env, model);
        const Model rows(env, model);
        const Model columns(env, model);
        ASSERT_EQ(add_listed(listed, shuffled(added, engine)), BIYOS_OK);
        ASSERT_EQ(add_rows(rows, added), BIYOS_OK);
        ASSERT_EQ(add_columns(columns, added), BIYOS_OK);
        EXPECT_EQ(difference(listed, rows), "");
        EXPECT_EQ(difference(listed, columns), "");
    }
}

TEST(AdderTest, Sums)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        const Added added = drawn(engine, n_functions(model), n_variables(model));
        const Held after = summed(held_of(model), added);
        ASSERT_EQ(add_listed(model, shuffled(added, engine)), BIYOS_OK);
        EXPECT_EQ(held_of(model), after);
    }
}

TEST(AdderTest, Refusals)
{
    const Env env;
    const Model model(env);
    Engine engine(kSeeds[0]);
    build_sample(model.get(), engine);
    const int functions = n_functions(model);
    const int variables = n_variables(model);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::string unknown = "invalid argument: function " + std::to_string(functions);
    const std::string twice = "invalid argument: function 0: variable 0 is repeated";
    const Added repeated{.functions = {0, 0}, .variables = {0, 0}, .values = {1.0, 2.0}};
    expect_kept(env, model, add_listed, repeated, twice);
    expect_kept(env, model, add_rows, repeated, twice);
    expect_kept(env, model, add_columns, repeated, twice);
    const Added stranger{.functions = {0, functions}, .variables = {0, 0}, .values = {1.0, 1.0}};
    expect_kept(env, model, add_listed, stranger, unknown + ": unknown");
    expect_kept(env, model, add_columns, stranger, unknown + ": unknown");
    const Added negative{.functions = {-1}, .variables = {0}, .values = {1.0}};
    expect_kept(env, model, add_listed, negative, "invalid argument: function -1: unknown");
    const Added outside{.functions = {0}, .variables = {variables}, .values = {1.0}};
    const std::string missing =
        "invalid argument: function 0: variable " + std::to_string(variables) + " is unknown";
    expect_kept(env, model, add_listed, outside, missing);
    expect_kept(env, model, add_rows, outside, missing);
    const Added wrong{.functions = {0}, .variables = {0}, .values = {nan}};
    expect_kept(env,
                model,
                add_listed,
                wrong,
                "invalid argument: function 0: coefficient of variable 0 is NaN");
    const Model sums(env, model);
    const Added first{.functions = {0}, .variables = {0}, .values = {kLargest}};
    ASSERT_EQ(add_listed(sums, first), BIYOS_OK);
    expect_kept(env,
                sums,
                add_columns,
                first,
                "invalid argument: function 0: sum of coefficients of variable 0 is infinity");
}

TEST(AdderTest, Arguments)
{
    const Env env;
    const Model model(env);
    Engine engine(kSeeds[0]);
    build_sample(model.get(), engine);
    const int functions = n_functions(model);
    const int variables = n_variables(model);
    BIYOS_model *held = model.get();
    const std::array<BIYOS_index, 1> zero{0};
    const std::array<double, 1> one{1.0};
    const std::array<BIYOS_offset, 3> backwards{0, 1, 0};
    const std::array<BIYOS_offset, 2> late{1, 1};
    EXPECT_EQ(BIYOS_addlintermsp(held, -1, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': -1 is negative");
    EXPECT_EQ(BIYOS_addlintermsp(held, 1, nullptr, zero.data(), one.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': is null");
    EXPECT_EQ(BIYOS_addlintermsp(held, 1, zero.data(), nullptr, one.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'varidx': is null");
    EXPECT_EQ(BIYOS_addlintermsp(held, 1, zero.data(), zero.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'coef': is null");
    EXPECT_EQ(BIYOS_addrlintermrg(held, 0, functions + 1, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: function " + std::to_string(functions) + ": unknown");
    EXPECT_EQ(BIYOS_addclintermrg(held, variables, 1, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: variable " + std::to_string(variables) + ": unknown");
    EXPECT_EQ(BIYOS_addclintermrg(held, 0, 2, backwards.data(), zero.data(), one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': decreases from 1 to 0 at entry 2");
    EXPECT_EQ(BIYOS_addrlintermrg(held, 0, 1, late.data(), zero.data(), one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'off': begins at 1, not 0");
    EXPECT_EQ(BIYOS_addclintermrg(held, 0, 1, backwards.data(), nullptr, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'funidx': is null");
    EXPECT_EQ(BIYOS_addlintermsp(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addrlintermrg(held, 0, functions, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addclintermrg(held, variables, 0, nullptr, nullptr, nullptr), BIYOS_OK);
}
