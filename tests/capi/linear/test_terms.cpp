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
using Counter = BIYOS_status (*)(const BIYOS_model *, BIYOS_index *);
using CountListed = BIYOS_status (*)(const BIYOS_model *,
                                     BIYOS_index,
                                     const BIYOS_index *,
                                     BIYOS_offset *);
using CountRanged = BIYOS_status (*)(const BIYOS_model *, BIYOS_index, BIYOS_index, BIYOS_offset *);
using ReadListed = BIYOS_status (*)(const BIYOS_model *,
                                    BIYOS_index,
                                    const BIYOS_index *,
                                    BIYOS_offset,
                                    BIYOS_offset *,
                                    BIYOS_index *,
                                    BIYOS_index *,
                                    BIYOS_real *);
using ReadRanged = BIYOS_status (*)(const BIYOS_model *,
                                    BIYOS_index,
                                    BIYOS_index,
                                    BIYOS_offset,
                                    BIYOS_offset *,
                                    BIYOS_index *,
                                    BIYOS_real *);

struct Reader
{
    Counter records;
    CountListed n_listed;
    CountRanged n_ranged;
    ReadListed listed;
    ReadRanged ranged;
    bool by_row;
    std::string family;
};

const Reader kRows{
    .records = BIYOS_getnfun,
    .n_listed = BIYOS_getnrlintermsp,
    .n_ranged = BIYOS_getnrlintermrg,
    .listed = BIYOS_getrlintermsp,
    .ranged = BIYOS_getrlintermrg,
    .by_row = true,
    .family = "function",
};

const Reader kColumns{
    .records = BIYOS_getnvar,
    .n_listed = BIYOS_getnclintermsp,
    .n_ranged = BIYOS_getnclintermrg,
    .listed = BIYOS_getclintermsp,
    .ranged = BIYOS_getclintermrg,
    .by_row = false,
    .family = "variable",
};

struct Span
{
    BIYOS_index start;
    BIYOS_index size;
};

struct Coords
{
    Indices functions;
    Indices variables;
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

void expect_same(const Coords &found, const Coords &wanted)
{
    EXPECT_EQ(found.functions, wanted.functions);
    EXPECT_EQ(found.variables, wanted.variables);
    EXPECT_EQ(found.values, wanted.values);
}

void append(Coords &into, const Coords &more)
{
    into.functions.insert(into.functions.end(), more.functions.begin(), more.functions.end());
    into.variables.insert(into.variables.end(), more.variables.begin(), more.variables.end());
    into.values.insert(into.values.end(), more.values.begin(), more.values.end());
}

int count_of(const Model &model, const Reader &reader)
{
    BIYOS_index n = -1;
    EXPECT_EQ(reader.records(model.get(), &n), BIYOS_OK);
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

BIYOS_offset n_listed(const Model &model, const Reader &reader, const Indices &listed)
{
    BIYOS_offset nnz = -1;
    const auto n = static_cast<BIYOS_index>(listed.size());
    EXPECT_EQ(reader.n_listed(model.get(), n, listed.data(), &nnz), BIYOS_OK);
    return nnz;
}

BIYOS_offset n_ranged(const Model &model, const Reader &reader, Span span)
{
    BIYOS_offset nnz = -1;
    EXPECT_EQ(reader.n_ranged(model.get(), span.start, span.size, &nnz), BIYOS_OK);
    return nnz;
}

Coords read_listed(const Model &model, const Reader &reader, const Indices &listed)
{
    const BIYOS_offset count = n_listed(model, reader, listed);
    const auto size = static_cast<std::size_t>(count);
    Coords found{.functions = Indices(size, -1), .variables = Indices(size, -1), .values = {}};
    std::vector<double> values(size);
    BIYOS_offset nnz = -1;
    EXPECT_EQ(reader.listed(model.get(),
                            static_cast<BIYOS_index>(listed.size()),
                            listed.data(),
                            count,
                            &nnz,
                            found.functions.data(),
                            found.variables.data(),
                            values.data()),
              BIYOS_OK);
    EXPECT_EQ(nnz, count);
    found.values = bits(values);
    return found;
}

Indices expanded(const std::vector<BIYOS_offset> &starts, BIYOS_index first)
{
    Indices records;
    for (std::size_t j = 0; j + 1 < starts.size(); ++j)
    {
        records.insert(records.end(),
                       static_cast<std::size_t>(starts[j + 1] - starts[j]),
                       first + static_cast<BIYOS_index>(j));
    }
    return records;
}

Coords read_ranged(const Model &model, const Reader &reader, Span span)
{
    const BIYOS_offset count = n_ranged(model, reader, span);
    const auto size = static_cast<std::size_t>(count);
    std::vector<BIYOS_offset> starts(static_cast<std::size_t>(span.size) + 1, -1);
    Indices other(size, -1);
    std::vector<double> values(size);
    EXPECT_EQ(reader.ranged(model.get(),
                            span.start,
                            span.size,
                            count,
                            starts.data(),
                            other.data(),
                            values.data()),
              BIYOS_OK);
    EXPECT_EQ(starts.front(), 0);
    EXPECT_EQ(starts.back(), count);
    Indices records = expanded(starts, span.start);
    if (reader.by_row)
    {
        return {.functions = records, .variables = other, .values = bits(values)};
    }
    return {.functions = other, .variables = records, .values = bits(values)};
}

void expect_pairs(const Model &model, const Reader &reader, Engine &engine)
{
    const int count = count_of(model, reader);
    const Span span = random_span(engine, count);
    expect_same(read_listed(model, reader, indices_of(span)), read_ranged(model, reader, span));
    const Indices listed = random_list(engine, count);
    Coords wanted;
    for (const BIYOS_index index : listed)
    {
        append(wanted, read_ranged(model, reader, {.start = index, .size = 1}));
    }
    expect_same(read_listed(model, reader, listed), wanted);
}

void expect_kept(const Model &model, const Reader &reader, const Indices &listed, BIYOS_offset cap)
{
    const auto n = static_cast<BIYOS_index>(listed.size());
    BIYOS_offset nnz = -1;
    const auto size = static_cast<std::size_t>(cap) + 1;
    Indices functions(size, -1);
    Indices variables(size, -1);
    std::vector<double> values(size, -1.0);
    EXPECT_EQ(reader.listed(model.get(),
                            n,
                            listed.data(),
                            cap,
                            &nnz,
                            functions.data(),
                            variables.data(),
                            values.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(nnz, -1);
    EXPECT_EQ(functions, Indices(size, -1));
    EXPECT_EQ(variables, Indices(size, -1));
    EXPECT_EQ(bits(values), bits(std::vector<double>(size, -1.0)));
}

void expect_refused(const Env &env, const Reader &reader)
{
    Engine engine(kSeeds[0]);
    const Model model(env);
    build_sample(model.get(), engine);
    const int count = count_of(model, reader);
    const std::string unknown = reader.family + " " + std::to_string(count) + ": unknown";
    expect_kept(model, reader, {0, count}, 4);
    EXPECT_EQ(env.message(), "invalid argument: " + unknown);
    const BIYOS_offset needed = n_listed(model, reader, indices_of({0, count}));
    ASSERT_GT(needed, 0);
    expect_kept(model, reader, indices_of({0, count}), needed - 1);
    const std::string held = std::to_string(needed - 1) + " of the " + std::to_string(needed);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds " + held + " terms");
    BIYOS_offset nnz = -1;
    EXPECT_EQ(reader.n_ranged(model.get(), 0, count + 1, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: " + unknown);
    EXPECT_EQ(nnz, -1);
    EXPECT_EQ(reader.n_ranged(model.get(), 0, count, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
}

}

TEST(TermTest, Rows)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        expect_pairs(model, kRows, engine);
    }
}

TEST(TermTest, Columns)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        expect_pairs(model, kColumns, engine);
    }
}

TEST(TermTest, Counts)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        const Span rows{.start = 0, .size = count_of(model, kRows)};
        const Span columns{.start = 0, .size = count_of(model, kColumns)};
        const BIYOS_offset total = n_ranged(model, kRows, rows);
        EXPECT_EQ(n_ranged(model, kColumns, columns), total);
        const Indices listed = random_list(engine, rows.size);
        EXPECT_EQ(static_cast<std::size_t>(n_listed(model, kRows, listed)),
                  read_listed(model, kRows, listed).values.size());
        const Span span = random_span(engine, columns.size);
        EXPECT_EQ(static_cast<std::size_t>(n_ranged(model, kColumns, span)),
                  read_ranged(model, kColumns, span).values.size());
    }
}

TEST(TermTest, Refusals)
{
    const Env env;
    expect_refused(env, kRows);
    expect_refused(env, kColumns);
}
