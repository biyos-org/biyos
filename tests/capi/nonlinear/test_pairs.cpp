#include "biyos/biyos.h"
#include "handles.hpp"
#include "sample.hpp"

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

Pairs read_listed(const Model &model, const Indices &listed)
{
    const BIYOS_offset count = n_listed(model, listed);
    const auto size = static_cast<std::size_t>(count);
    Pairs found{
        .functions = Indices(size, -1),
        .rows = Indices(size, -1),
        .columns = Indices(size, -1),
        .values = {},
    };
    std::vector<double> values(size);
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getquadtermsp(model.get(),
                                  static_cast<BIYOS_index>(listed.size()),
                                  listed.data(),
                                  count,
                                  &nnz,
                                  found.functions.data(),
                                  found.rows.data(),
                                  found.columns.data(),
                                  values.data()),
              BIYOS_OK);
    EXPECT_EQ(nnz, count);
    found.values = bits(values);
    return found;
}

Indices expanded(const std::vector<BIYOS_offset> &counts, BIYOS_index first)
{
    Indices records;
    for (std::size_t j = 0; j < counts.size(); ++j)
    {
        records.insert(records.end(),
                       static_cast<std::size_t>(counts[j]),
                       first + static_cast<BIYOS_index>(j));
    }
    return records;
}

Pairs read_ranged(const Model &model, Span span)
{
    const BIYOS_offset count = n_ranged(model, span);
    const auto size = static_cast<std::size_t>(count);
    std::vector<BIYOS_offset> counts(static_cast<std::size_t>(span.size), -1);
    Indices rows(size, -1);
    Indices columns(size, -1);
    std::vector<double> values(size);
    EXPECT_EQ(BIYOS_getquadtermrg(model.get(),
                                  span.start,
                                  span.start + span.size,
                                  count,
                                  counts.data(),
                                  rows.data(),
                                  columns.data(),
                                  values.data()),
              BIYOS_OK);
    EXPECT_EQ(std::accumulate(counts.begin(), counts.end(), BIYOS_offset{0}), count);
    return {
        .functions = expanded(counts, span.start),
        .rows = rows,
        .columns = columns,
        .values = bits(values),
    };
}

void expect_kept(const Model &model, const Indices &listed, BIYOS_offset cap)
{
    const auto n = static_cast<BIYOS_index>(listed.size());
    BIYOS_offset nnz = -1;
    const auto size = static_cast<std::size_t>(cap) + 1;
    Indices functions(size, -1);
    Indices rows(size, -1);
    std::vector<double> values(size, -1.0);
    EXPECT_EQ(BIYOS_getquadtermsp(model.get(),
                                  n,
                                  listed.data(),
                                  cap,
                                  &nnz,
                                  functions.data(),
                                  rows.data(),
                                  nullptr,
                                  values.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(nnz, -1);
    EXPECT_EQ(functions, Indices(size, -1));
    EXPECT_EQ(rows, Indices(size, -1));
    EXPECT_EQ(bits(values), bits(std::vector<double>(size, -1.0)));
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
