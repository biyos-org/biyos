#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "counts.hpp"
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
using biyos::test::expanded;
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
    std::vector<BIYOS_offset> counts;
    Indices others;
    std::vector<double> values;
};

struct Listed
{
    Indices records;
    Compressed terms;
};

const Listed kScattered{
    .records = {0, 7, 9},
    .terms =
        {
            .counts = {3, 5, 2},
            .others = {0, 1, 2, 0, 1, 2, 3, 4, 3, 4},
            .values = {1.0, -2.0, 0.5, 4.0, 0.0, -0.0, 3.0, -1.5, 2.0, 8.0},
        },
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
        const std::size_t before = found.others.size();
        for (std::size_t k = 0; k < records.size(); ++k)
        {
            if (records[k] == record)
            {
                found.others.push_back(others[k]);
                found.values.push_back(added.values[k]);
            }
        }
        found.counts.push_back(static_cast<BIYOS_offset>(found.others.size() - before));
    }
    return found;
}

void push_part(Listed &listed,
               BIYOS_index record,
               const Compressed &whole,
               std::size_t begin,
               std::size_t end)
{
    listed.records.push_back(record);
    listed.terms.counts.push_back(static_cast<BIYOS_offset>(end - begin));
    for (std::size_t k = begin; k < end; ++k)
    {
        listed.terms.others.push_back(whole.others[k]);
        listed.terms.values.push_back(whole.values[k]);
    }
}

Listed split(const Added &added, int count, bool by_row)
{
    const Compressed whole = compressed(added, count, by_row);
    Listed found;
    std::size_t end = whole.others.size();
    for (int record = count - 1; record >= 0; --record)
    {
        const auto size = static_cast<std::size_t>(whole.counts[static_cast<std::size_t>(record)]);
        const std::size_t begin = end - size;
        push_part(found, record, whole, begin, begin + size / 2);
        push_part(found, record, whole, begin + size / 2, end);
        end = begin;
    }
    return found;
}

Added coordinates_of(const Listed &listed, bool by_row)
{
    const Indices owners = expanded(listed.terms.counts, listed.records);
    return Added{
        .functions = by_row ? owners : listed.terms.others,
        .variables = by_row ? listed.terms.others : owners,
        .values = listed.terms.values,
    };
}

BIYOS_status add_list(const Model &model, const Listed &listed, bool by_row)
{
    const auto add = by_row ? BIYOS_addrlintermsp : BIYOS_addclintermsp;
    return add(model.get(),
               static_cast<BIYOS_index>(listed.records.size()),
               listed.records.data(),
               listed.terms.counts.data(),
               listed.terms.others.data(),
               listed.terms.values.data());
}

BIYOS_status add_row_split(const Model &model, const Added &added)
{
    return add_list(model, split(added, n_functions(model), true), true);
}

BIYOS_status add_column_split(const Model &model, const Added &added)
{
    return add_list(model, split(added, n_variables(model), false), false);
}

Listed read_list(const Model &model, const Indices &records, bool by_row)
{
    const auto n = static_cast<BIYOS_index>(records.size());
    const auto count = by_row ? BIYOS_getnrlintermsp : BIYOS_getnclintermsp;
    const auto read = by_row ? BIYOS_getrlintermsp : BIYOS_getclintermsp;
    BIYOS_offset nnz = -1;
    EXPECT_EQ(count(model.get(), n, records.data(), &nnz), BIYOS_OK);
    const auto size = static_cast<std::size_t>(nnz);
    Listed found{
        .records = records,
        .terms =
            {
                .counts = std::vector<BIYOS_offset>(records.size(), -1),
                .others = Indices(size, -1),
                .values = std::vector<double>(size),
            },
    };
    EXPECT_EQ(read(model.get(),
                   n,
                   records.data(),
                   nnz,
                   found.terms.counts.data(),
                   found.terms.others.data(),
                   found.terms.values.data()),
              BIYOS_OK);
    return found;
}

BIYOS_status add_listed(const Model &model, const Added &added)
{
    return BIYOS_addlinterm(model.get(),
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
                               rows.counts.data(),
                               variables,
                               rows.values.data());
}

BIYOS_status add_columns(const Model &model, const Added &added)
{
    const int n = n_variables(model);
    const Compressed columns = compressed(added, n, false);
    const BIYOS_index *functions = columns.others.data();
    const double *values = columns.values.data();
    return BIYOS_addclintermrg(model.get(), 0, n, columns.counts.data(), functions, values);
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
    std::vector<BIYOS_offset> counts(every.size());
    Indices variables(size);
    std::vector<double> values(size);
    EXPECT_EQ(BIYOS_getrlintermsp(model.get(),
                                  n,
                                  every.data(),
                                  nnz,
                                  counts.data(),
                                  variables.data(),
                                  values.data()),
              BIYOS_OK);
    const Indices functions = expanded(counts, every);
    EXPECT_EQ(functions.size(), size);
    Held held;
    for (std::size_t k = 0; k < functions.size(); ++k)
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
    return biyos::difference(left.get()->model(), right.get()->model());
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
        const Model row_list(env, model);
        const Model column_list(env, model);
        ASSERT_EQ(add_listed(listed, shuffled(added, engine)), BIYOS_OK);
        ASSERT_EQ(add_rows(rows, added), BIYOS_OK);
        ASSERT_EQ(add_columns(columns, added), BIYOS_OK);
        ASSERT_EQ(add_row_split(row_list, added), BIYOS_OK);
        ASSERT_EQ(add_column_split(column_list, added), BIYOS_OK);
        EXPECT_EQ(difference(listed, rows), "");
        EXPECT_EQ(difference(listed, columns), "");
        EXPECT_EQ(difference(listed, row_list), "");
        EXPECT_EQ(difference(listed, column_list), "");
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
    expect_kept(env, model, add_row_split, repeated, twice);
    expect_kept(env, model, add_column_split, repeated, twice);
    const Added stranger{.functions = {0, functions}, .variables = {0, 0}, .values = {1.0, 1.0}};
    expect_kept(env, model, add_listed, stranger, unknown + ": unknown");
    expect_kept(env, model, add_columns, stranger, unknown + ": unknown");
    expect_kept(env, model, add_column_split, stranger, unknown + ": unknown");
    const Added negative{.functions = {-1}, .variables = {0}, .values = {1.0}};
    expect_kept(env, model, add_listed, negative, "invalid argument: function -1: unknown");
    const Added outside{.functions = {0}, .variables = {variables}, .values = {1.0}};
    const std::string missing =
        "invalid argument: function 0: variable " + std::to_string(variables) + " is unknown";
    expect_kept(env, model, add_listed, outside, missing);
    expect_kept(env, model, add_rows, outside, missing);
    expect_kept(env, model, add_row_split, outside, missing);
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
    const std::array<BIYOS_offset, 3> negative{1, 0, -1};
    const std::array<BIYOS_offset, 2> overflowing{(std::numeric_limits<BIYOS_offset>::max)(), 1};
    EXPECT_EQ(BIYOS_addlinterm(held, -1, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': -1 is negative");
    EXPECT_EQ(BIYOS_addlinterm(held, 1, nullptr, zero.data(), one.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'fun': is null");
    EXPECT_EQ(BIYOS_addlinterm(held, 1, zero.data(), nullptr, one.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'var': is null");
    EXPECT_EQ(BIYOS_addlinterm(held, 1, zero.data(), zero.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'coef': is null");
    EXPECT_EQ(BIYOS_addrlintermrg(held, 0, functions + 1, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: function " + std::to_string(functions) + ": unknown");
    EXPECT_EQ(BIYOS_addclintermrg(held, variables, variables + 1, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: variable " + std::to_string(variables) + ": unknown");
    EXPECT_EQ(BIYOS_addclintermrg(held, 0, 3, negative.data(), zero.data(), one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': -1 is negative at entry 2");
    EXPECT_EQ(BIYOS_addrlintermrg(held, 0, 2, overflowing.data(), zero.data(), one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': sum overflows at entry 1");
    EXPECT_EQ(BIYOS_addclintermrg(held, 0, 1, negative.data(), nullptr, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'fun': is null");
    EXPECT_EQ(BIYOS_addlinterm(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addrlintermrg(held, 0, functions, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addclintermrg(held, variables, variables, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(AdderTest, Scattered)
{
    for (const bool by_row : {true, false})
    {
        const Env env;
        const Model model(env);
        ASSERT_EQ(BIYOS_addvar(model.get(), 10, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
        ASSERT_EQ(BIYOS_addfun(model.get(), 10, nullptr), BIYOS_OK);
        const Model listed(env, model);
        ASSERT_EQ(add_list(listed, kScattered, by_row), BIYOS_OK);
        ASSERT_EQ(add_listed(model, coordinates_of(kScattered, by_row)), BIYOS_OK);
        EXPECT_EQ(difference(listed, model), "");
        const Listed read = read_list(listed, kScattered.records, by_row);
        EXPECT_EQ(read.terms.counts, kScattered.terms.counts);
        EXPECT_EQ(read.terms.others, kScattered.terms.others);
    }
}

TEST(AdderTest, Lists)
{
    const Env env;
    const Model model(env);
    Engine engine(kSeeds[0]);
    build_sample(model.get(), engine);
    const int functions = n_functions(model);
    const int variables = n_variables(model);
    BIYOS_model *held = model.get();
    const std::array<BIYOS_index, 3> zeros{0, 0, 0};
    const std::array<double, 1> one{1.0};
    const std::array<BIYOS_offset, 3> negative{1, 0, -1};
    const std::array<BIYOS_offset, 2> overflowing{(std::numeric_limits<BIYOS_offset>::max)(), 1};
    const std::array<BIYOS_offset, 1> empty{0};
    const BIYOS_index *zero = zeros.data();
    const BIYOS_index minus = -1;
    EXPECT_EQ(BIYOS_addrlintermsp(held, -1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_addclintermsp(held, 1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_addrlintermsp(held, 1, &minus, empty.data(), nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function -1: unknown");
    EXPECT_EQ(BIYOS_addclintermsp(held, 1, &minus, empty.data(), nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_addrlintermsp(held, 1, &functions, empty.data(), nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: function " + std::to_string(functions) + ": unknown");
    EXPECT_EQ(BIYOS_addclintermsp(held, 1, &variables, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: variable " + std::to_string(variables) + ": unknown");
    EXPECT_EQ(BIYOS_addrlintermsp(held, 3, zero, negative.data(), zero, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': -1 is negative at entry 2");
    EXPECT_EQ(BIYOS_addclintermsp(held, 2, zero, overflowing.data(), zero, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': sum overflows at entry 1");
    EXPECT_EQ(BIYOS_addclintermsp(held, 1, zero, negative.data(), nullptr, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'fun': is null");
    EXPECT_EQ(BIYOS_addrlintermsp(held, 1, zero, negative.data(), zero, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'coef': is null");
    const Model before(env, model);
    EXPECT_EQ(BIYOS_addrlintermsp(held, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addclintermsp(held, 2, zero, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(difference(before, model), "");
}
