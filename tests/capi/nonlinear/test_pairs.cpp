#include "biyos/biyos.h"
#include "handles.hpp"
#include "sample.hpp"
#include "utils/counts.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
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

using Indices = std::vector<BIYOS_index>;

struct Span
{
    BIYOS_index start;
    BIYOS_index size;
};

struct Pairs
{
    Indices functions;
    Indices rows;
    Indices columns;
    std::vector<std::uint64_t> values;
};

std::vector<std::uint64_t> bits(const std::vector<double> &values)
{
    std::vector<std::uint64_t> found;
    for (const double value : values)
    {
        found.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return found;
}

void expect_same(const Pairs &found, const Pairs &wanted)
{
    EXPECT_EQ(found.functions, wanted.functions);
    EXPECT_EQ(found.rows, wanted.rows);
    EXPECT_EQ(found.columns, wanted.columns);
    EXPECT_EQ(found.values, wanted.values);
}

void append(Pairs &into, const Pairs &more)
{
    into.functions.insert(into.functions.end(), more.functions.begin(), more.functions.end());
    into.rows.insert(into.rows.end(), more.rows.begin(), more.rows.end());
    into.columns.insert(into.columns.end(), more.columns.begin(), more.columns.end());
    into.values.insert(into.values.end(), more.values.begin(), more.values.end());
}

int n_functions(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnfun(model.get(), &n), BIYOS_OK);
    return n;
}

Span random_span(Engine &engine, int count)
{
    const int start = below(engine, count + 1);
    return {.start = start, .size = below(engine, count - start + 1)};
}

Indices random_list(Engine &engine, int count)
{
    Indices listed(static_cast<std::size_t>(below(engine, 2 * count + 1)));
    for (BIYOS_index &index : listed)
    {
        index = below(engine, count);
    }
    return listed;
}

Indices indices_of(Span span)
{
    Indices listed(static_cast<std::size_t>(span.size));
    std::iota(listed.begin(), listed.end(), span.start);
    return listed;
}

BIYOS_offset n_listed(const Model &model, const Indices &listed)
{
    BIYOS_offset nnz = -1;
    const auto n = static_cast<BIYOS_index>(listed.size());
    EXPECT_EQ(BIYOS_getnquadtermsp(model.get(), n, listed.data(), &nnz), BIYOS_OK);
    return nnz;
}

BIYOS_offset n_ranged(const Model &model, Span span)
{
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnquadtermrg(model.get(), span.start, span.start + span.size, &nnz),
              BIYOS_OK);
    return nnz;
}

struct Read
{
    std::vector<BIYOS_offset> counts;
    Indices rows;
    Indices columns;
    std::vector<double> values;
};

Read sized(std::size_t n, BIYOS_offset count)
{
    const auto size = static_cast<std::size_t>(count);
    return {
        .counts = std::vector<BIYOS_offset>(n, -1),
        .rows = Indices(size, -1),
        .columns = Indices(size, -1),
        .values = std::vector<double>(size),
    };
}

Pairs arranged(const Indices &listed, const Read &read)
{
    const Indices functions = expanded(read.counts, listed);
    EXPECT_EQ(functions.size(), read.values.size());
    return {
        .functions = functions,
        .rows = read.rows,
        .columns = read.columns,
        .values = bits(read.values),
    };
}

Read fetched(const Model &model, const Indices &listed)
{
    const BIYOS_offset count = n_listed(model, listed);
    Read read = sized(listed.size(), count);
    EXPECT_EQ(BIYOS_getquadtermsp(model.get(),
                                  static_cast<BIYOS_index>(listed.size()),
                                  listed.data(),
                                  count,
                                  read.counts.data(),
                                  read.rows.data(),
                                  read.columns.data(),
                                  read.values.data()),
              BIYOS_OK);
    return read;
}

Pairs read_listed(const Model &model, const Indices &listed)
{
    return arranged(listed, fetched(model, listed));
}

Pairs read_ranged(const Model &model, Span span)
{
    const BIYOS_offset count = n_ranged(model, span);
    Read read = sized(static_cast<std::size_t>(span.size), count);
    EXPECT_EQ(BIYOS_getquadtermrg(model.get(),
                                  span.start,
                                  span.start + span.size,
                                  count,
                                  read.counts.data(),
                                  read.rows.data(),
                                  read.columns.data(),
                                  read.values.data()),
              BIYOS_OK);
    return arranged(indices_of(span), read);
}

void expect_kept(const Model &model, const Indices &listed, BIYOS_offset cap)
{
    const auto n = static_cast<BIYOS_index>(listed.size());
    const auto size = static_cast<std::size_t>(cap) + 1;
    std::vector<BIYOS_offset> counts(listed.size(), -1);
    Indices rows(size, -1);
    std::vector<double> values(size, -1.0);
    EXPECT_EQ(BIYOS_getquadtermsp(model.get(),
                                  n,
                                  listed.data(),
                                  cap,
                                  counts.data(),
                                  rows.data(),
                                  nullptr,
                                  values.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(counts, std::vector<BIYOS_offset>(listed.size(), -1));
    EXPECT_EQ(rows, Indices(size, -1));
    EXPECT_EQ(bits(values), bits(std::vector<double>(size, -1.0)));
}

void build_small(const Model &model)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 2, nullptr), BIYOS_OK);
    const Indices functions{0, 0, 1};
    const Indices firsts{0, 1, 2};
    const Indices seconds{0, 2, 2};
    const std::vector<double> values{1.0, 2.0, 3.0};
    ASSERT_EQ(BIYOS_addquadterm(model.get(),
                                3,
                                functions.data(),
                                firsts.data(),
                                seconds.data(),
                                values.data()),
              BIYOS_OK);
}

void expect_uncounted(const Model &model, const Indices &listed)
{
    const Read wanted = fetched(model, listed);
    const BIYOS_offset count = n_listed(model, listed);
    Read read = sized(0, count);
    EXPECT_EQ(BIYOS_getquadtermsp(model.get(),
                                  static_cast<BIYOS_index>(listed.size()),
                                  listed.data(),
                                  count,
                                  nullptr,
                                  read.rows.data(),
                                  read.columns.data(),
                                  read.values.data()),
              BIYOS_OK);
    EXPECT_EQ(read.rows, wanted.rows);
    EXPECT_EQ(read.columns, wanted.columns);
    EXPECT_EQ(bits(read.values), bits(wanted.values));
}

}

TEST(PairTest, Agree)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        const int count = n_functions(model);
        const Span span = random_span(engine, count);
        expect_same(read_listed(model, indices_of(span)), read_ranged(model, span));
        const Indices listed = random_list(engine, count);
        Pairs wanted;
        for (const BIYOS_index index : listed)
        {
            append(wanted, read_ranged(model, {.start = index, .size = 1}));
        }
        expect_same(read_listed(model, listed), wanted);
    }
}

TEST(PairTest, Repeats)
{
    const Env env;
    const Model model(env);
    build_small(model);
    const Read read = fetched(model, {0, 1, 0});
    EXPECT_EQ(read.counts, (std::vector<BIYOS_offset>{2, 1, 2}));
    EXPECT_EQ(read.rows, (Indices{0, 1, 2, 0, 1}));
    EXPECT_EQ(read.columns, (Indices{0, 2, 2, 0, 2}));
    EXPECT_EQ(bits(read.values), bits({1.0, 2.0, 3.0, 1.0, 2.0}));
}

TEST(PairTest, Uncounted)
{
    const Env env;
    const Model model(env);
    build_small(model);
    expect_uncounted(model, {0, 1, 0});
}

TEST(PairTest, Counts)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        const int count = n_functions(model);
        const Indices listed = random_list(engine, count);
        EXPECT_EQ(static_cast<std::size_t>(n_listed(model, listed)),
                  read_listed(model, listed).values.size());
        const Span span = random_span(engine, count);
        EXPECT_EQ(static_cast<std::size_t>(n_ranged(model, span)),
                  read_ranged(model, span).values.size());
        EXPECT_EQ(n_listed(model, indices_of({0, count})), n_ranged(model, {0, count}));
    }
}

TEST(PairTest, Refusals)
{
    const Env env;
    Engine engine(kSeeds[0]);
    const Model model(env);
    build_sample(model.get(), engine);
    const int count = n_functions(model);
    const std::string unknown = "invalid argument: function " + std::to_string(count) + ": unknown";
    expect_kept(model, {0, count}, 4);
    EXPECT_EQ(env.message(), unknown);
    const BIYOS_offset needed = n_listed(model, indices_of({0, count}));
    ASSERT_GT(needed, 0);
    expect_kept(model, indices_of({0, count}), needed - 1);
    const std::string held = std::to_string(needed - 1) + " of the " + std::to_string(needed);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds " + held + " terms");
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnquadtermrg(model.get(), 0, count + 1, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), unknown);
    EXPECT_EQ(nnz, -1);
    EXPECT_EQ(BIYOS_getnquadtermsp(model.get(), 1, nullptr, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(nnz, -1);
}
