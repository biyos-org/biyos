#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"
#include "model/compare/compare.hpp"
#include "sample.hpp"
#include "utils/counts.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <string>
#include <tuple>
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
using Key = std::tuple<BIYOS_index, BIYOS_index, BIYOS_index>;
using Held = std::map<Key, std::uint64_t>;

struct Added
{
    Indices functions;
    Indices rows;
    Indices columns;
    std::vector<double> values;
};

struct Compressed
{
    std::vector<BIYOS_offset> counts;
    Indices rows;
    Indices columns;
    std::vector<double> values;
};

struct Listed
{
    Indices functions;
    Compressed terms;
};

const Listed kScattered{
    .functions = {0, 7, 9},
    .terms =
        {
            .counts = {3, 5, 2},
            .rows = {0, 0, 1, 0, 0, 1, 2, 3, 1, 4},
            .columns = {0, 2, 1, 1, 4, 3, 2, 4, 1, 4},
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

void push(Added &added, BIYOS_index function, BIYOS_index row, BIYOS_index column, double value)
{
    added.functions.push_back(function);
    added.rows.push_back(row);
    added.columns.push_back(column);
    added.values.push_back(value);
}

Added drawn(Engine &engine, int functions, int variables)
{
    Added added;
    for (int function = 0; function < functions; ++function)
    {
        for (int row = 0; row < variables; ++row)
        {
            for (int column = row; column < variables; ++column)
            {
                if (below(engine, 4) == 0)
                {
                    push(added, function, row, column, (below(engine, 17) - 8) / 4.0);
                }
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
        const bool swap = below(engine, 2) == 0;
        const BIYOS_index row = swap ? added.columns[k] : added.rows[k];
        const BIYOS_index column = swap ? added.rows[k] : added.columns[k];
        push(found, added.functions[k], row, column, added.values[k]);
    }
    return found;
}

Compressed compressed(const Added &added, int count)
{
    Compressed found;
    for (int function = 0; function < count; ++function)
    {
        const std::size_t before = found.rows.size();
        for (std::size_t k = 0; k < added.functions.size(); ++k)
        {
            if (added.functions[k] == function)
            {
                found.rows.push_back(added.rows[k]);
                found.columns.push_back(added.columns[k]);
                found.values.push_back(added.values[k]);
            }
        }
        found.counts.push_back(static_cast<BIYOS_offset>(found.rows.size() - before));
    }
    return found;
}

void push_part(Listed &listed,
               BIYOS_index function,
               const Compressed &whole,
               std::size_t begin,
               std::size_t end)
{
    listed.functions.push_back(function);
    listed.terms.counts.push_back(static_cast<BIYOS_offset>(end - begin));
    for (std::size_t k = begin; k < end; ++k)
    {
        listed.terms.rows.push_back(whole.rows[k]);
        listed.terms.columns.push_back(whole.columns[k]);
        listed.terms.values.push_back(whole.values[k]);
    }
}

Listed split(const Added &added, int count)
{
    const Compressed whole = compressed(added, count);
    Listed found;
    std::size_t end = whole.rows.size();
    for (int function = count - 1; function >= 0; --function)
    {
        const auto size =
            static_cast<std::size_t>(whole.counts[static_cast<std::size_t>(function)]);
        const std::size_t begin = end - size;
        push_part(found, function, whole, begin, begin + size / 2);
        push_part(found, function, whole, begin + size / 2, end);
        end = begin;
    }
    return found;
}

Added coordinates_of(const Listed &listed)
{
    return Added{
        .functions = expanded(listed.terms.counts, listed.functions),
        .rows = listed.terms.rows,
        .columns = listed.terms.columns,
        .values = listed.terms.values,
    };
}

BIYOS_status add_list(const Model &model, const Listed &listed)
{
    return BIYOS_addquadtermsp(model.get(),
                               static_cast<BIYOS_index>(listed.functions.size()),
                               listed.functions.data(),
                               listed.terms.counts.data(),
                               listed.terms.rows.data(),
                               listed.terms.columns.data(),
                               listed.terms.values.data());
}

BIYOS_status add_split(const Model &model, const Added &added)
{
    return add_list(model, split(added, n_functions(model)));
}

Listed read_list(const Model &model, const Indices &functions)
{
    const auto n = static_cast<BIYOS_index>(functions.size());
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnquadtermsp(model.get(), n, functions.data(), &nnz), BIYOS_OK);
    const auto size = static_cast<std::size_t>(nnz);
    Listed found{
        .functions = functions,
        .terms =
            {
                .counts = std::vector<BIYOS_offset>(functions.size(), -1),
                .rows = Indices(size, -1),
                .columns = Indices(size, -1),
                .values = std::vector<double>(size),
            },
    };
    EXPECT_EQ(BIYOS_getquadtermsp(model.get(),
                                  n,
                                  functions.data(),
                                  nnz,
                                  found.terms.counts.data(),
                                  found.terms.rows.data(),
                                  found.terms.columns.data(),
                                  found.terms.values.data()),
              BIYOS_OK);
    return found;
}

BIYOS_status add_listed(const Model &model, const Added &added)
{
    return BIYOS_addquadterm(model.get(),
                             static_cast<BIYOS_offset>(added.values.size()),
                             added.functions.data(),
                             added.rows.data(),
                             added.columns.data(),
                             added.values.data());
}

BIYOS_status add_ranged(const Model &model, const Added &added)
{
    const int n = n_functions(model);
    const Compressed found = compressed(added, n);
    return BIYOS_addquadtermrg(model.get(),
                               0,
                               n,
                               found.counts.data(),
                               found.rows.data(),
                               found.columns.data(),
                               found.values.data());
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
    EXPECT_EQ(BIYOS_getnquadtermsp(model.get(), n, every.data(), &nnz), BIYOS_OK);
    const auto size = static_cast<std::size_t>(nnz);
    std::vector<BIYOS_offset> counts(every.size());
    Indices rows(size);
    Indices columns(size);
    std::vector<double> values(size);
    EXPECT_EQ(BIYOS_getquadtermsp(model.get(),
                                  n,
                                  every.data(),
                                  nnz,
                                  counts.data(),
                                  rows.data(),
                                  columns.data(),
                                  values.data()),
              BIYOS_OK);
    const Indices functions = expanded(counts, every);
    EXPECT_EQ(functions.size(), size);
    Held held;
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        held[{functions[k], rows[k], columns[k]}] = std::bit_cast<std::uint64_t>(values[k]);
    }
    return held;
}

Held summed(Held held, const Added &added)
{
    for (std::size_t k = 0; k < added.values.size(); ++k)
    {
        const Key key{added.functions[k], added.rows[k], added.columns[k]};
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

TEST(ProductTest, Agree)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        const Added added = drawn(engine, n_functions(model), n_variables(model));
        const Model listed(env, model);
        const Model ranged(env, model);
        const Model parts(env, model);
        ASSERT_EQ(add_listed(listed, shuffled(added, engine)), BIYOS_OK);
        ASSERT_EQ(add_ranged(ranged, added), BIYOS_OK);
        ASSERT_EQ(add_split(parts, added), BIYOS_OK);
        EXPECT_EQ(difference(listed, ranged), "");
        EXPECT_EQ(difference(listed, parts), "");
    }
}

TEST(ProductTest, Sums)
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

TEST(ProductTest, Refusals)
{
    const Env env;
    const Model model(env);
    Engine engine(kSeeds[0]);
    build_sample(model.get(), engine);
    const int functions = n_functions(model);
    const int variables = n_variables(model);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::string twice = "invalid argument: function 0: pair (0, 1) is repeated";
    const Added repeated{
        .functions = {0, 0},
        .rows = {0, 1},
        .columns = {1, 0},
        .values = {1.0, 2.0},
    };
    expect_kept(env, model, add_listed, repeated, twice);
    expect_kept(env, model, add_ranged, repeated, twice);
    expect_kept(env, model, add_split, repeated, twice);
    const Added stranger{
        .functions = {0, functions},
        .rows = {0, 0},
        .columns = {0, 0},
        .values = {1.0, 1.0},
    };
    const std::string unknown = "invalid argument: function " + std::to_string(functions);
    expect_kept(env, model, add_listed, stranger, unknown + ": unknown");
    const Added outside{.functions = {0}, .rows = {0}, .columns = {variables}, .values = {1.0}};
    const std::string missing =
        "invalid argument: function 0: variable " + std::to_string(variables) + " is unknown";
    expect_kept(env, model, add_listed, outside, missing);
    expect_kept(env, model, add_ranged, outside, missing);
    expect_kept(env, model, add_split, outside, missing);
    const Added swapped{.functions = {0}, .rows = {variables}, .columns = {0}, .values = {1.0}};
    expect_kept(env, model, add_listed, swapped, missing);
    const Added negative{.functions = {-1}, .rows = {0}, .columns = {0}, .values = {1.0}};
    expect_kept(env, model, add_listed, negative, "invalid argument: function -1: unknown");
    const Added wrong{.functions = {0}, .rows = {0}, .columns = {0}, .values = {nan}};
    expect_kept(env,
                model,
                add_listed,
                wrong,
                "invalid argument: function 0: coefficient of pair (0, 0) is NaN");
    const Model sums(env, model);
    const Added first{.functions = {0}, .rows = {0}, .columns = {0}, .values = {kLargest}};
    ASSERT_EQ(add_listed(sums, first), BIYOS_OK);
    expect_kept(env,
                sums,
                add_ranged,
                first,
                "invalid argument: function 0: sum of coefficients of pair (0, 0) is infinity");
}

TEST(ProductTest, Arguments)
{
    const Env env;
    const Model model(env);
    Engine engine(kSeeds[0]);
    build_sample(model.get(), engine);
    const int functions = n_functions(model);
    BIYOS_model *held = model.get();
    const std::array<BIYOS_index, 1> zero{0};
    const std::array<double, 1> one{1.0};
    const std::array<BIYOS_offset, 3> negative{1, 0, -1};
    const std::array<BIYOS_offset, 2> overflowing{(std::numeric_limits<BIYOS_offset>::max)(), 1};
    EXPECT_EQ(BIYOS_addquadterm(held, -1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': -1 is negative");
    EXPECT_EQ(BIYOS_addquadterm(held, 1, nullptr, zero.data(), zero.data(), one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'fun': is null");
    EXPECT_EQ(BIYOS_addquadterm(held, 1, zero.data(), zero.data(), nullptr, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'var2': is null");
    EXPECT_EQ(BIYOS_addquadterm(held, 1, zero.data(), nullptr, zero.data(), one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'var1': is null");
    EXPECT_EQ(BIYOS_addquadterm(held, 1, zero.data(), zero.data(), zero.data(), nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'coef': is null");
    EXPECT_EQ(BIYOS_addquadtermrg(held, 0, -1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'end': -1 is below 'beg' 0");
    EXPECT_EQ(BIYOS_addquadtermrg(held, 0, functions + 1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: function " + std::to_string(functions) + ": unknown");
    EXPECT_EQ(
        BIYOS_addquadtermrg(held, 0, 3, negative.data(), zero.data(), zero.data(), one.data()),
        BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': -1 is negative at entry 2");
    EXPECT_EQ(
        BIYOS_addquadtermrg(held, 0, 2, overflowing.data(), zero.data(), zero.data(), one.data()),
        BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': sum overflows at entry 1");
    EXPECT_EQ(BIYOS_addquadterm(held, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addquadtermrg(held, 0, functions, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
}

TEST(ProductTest, Scattered)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvar(model.get(), 10, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 10, nullptr), BIYOS_OK);
    const Model listed(env, model);
    ASSERT_EQ(add_list(listed, kScattered), BIYOS_OK);
    ASSERT_EQ(add_listed(model, coordinates_of(kScattered)), BIYOS_OK);
    EXPECT_EQ(difference(listed, model), "");
    const Listed read = read_list(listed, kScattered.functions);
    EXPECT_EQ(read.terms.counts, kScattered.terms.counts);
    EXPECT_EQ(read.terms.rows, kScattered.terms.rows);
    EXPECT_EQ(read.terms.columns, kScattered.terms.columns);
}

TEST(ProductTest, Lists)
{
    const Env env;
    const Model model(env);
    Engine engine(kSeeds[0]);
    build_sample(model.get(), engine);
    const int functions = n_functions(model);
    BIYOS_model *held = model.get();
    const std::array<BIYOS_index, 3> zeros{0, 0, 0};
    const std::array<double, 1> one{1.0};
    const std::array<BIYOS_offset, 3> negative{1, 0, -1};
    const std::array<BIYOS_offset, 2> overflowing{(std::numeric_limits<BIYOS_offset>::max)(), 1};
    const std::array<BIYOS_offset, 1> empty{0};
    const BIYOS_index *zero = zeros.data();
    const BIYOS_index minus = -1;
    EXPECT_EQ(BIYOS_addquadtermsp(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_addquadtermsp(held, 1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_addquadtermsp(held, 1, &minus, empty.data(), nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: function -1: unknown");
    EXPECT_EQ(BIYOS_addquadtermsp(held, 1, &functions, empty.data(), nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: function " + std::to_string(functions) + ": unknown");
    EXPECT_EQ(BIYOS_addquadtermsp(held, 3, zero, negative.data(), zero, zero, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': -1 is negative at entry 2");
    EXPECT_EQ(BIYOS_addquadtermsp(held, 2, zero, overflowing.data(), zero, zero, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': sum overflows at entry 1");
    EXPECT_EQ(BIYOS_addquadtermsp(held, 1, zero, negative.data(), zero, nullptr, one.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'var2': is null");
    const Model before(env, model);
    EXPECT_EQ(BIYOS_addquadtermsp(held, 0, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addquadtermsp(held, 2, zero, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(difference(before, model), "");
}
