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
#include <numeric>
#include <string>
#include <tuple>
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

struct Span
{
    BIYOS_index start;
    BIYOS_index size;
};

struct Held
{
    const Env &env;
    const Model &model;
};

struct Names
{
    std::vector<std::string> texts;
    std::vector<const char *> pointers;
};

struct NameCalls
{
    Counter count;
    decltype(&BIYOS_setvarnamesp) set_listed;
    decltype(&BIYOS_setvarnamerg) set_ranged;
    decltype(&BIYOS_getvarnamesp) get_listed;
    decltype(&BIYOS_getvarnamerg) get_ranged;
    decltype(&BIYOS_delvarnamesp) delete_listed;
    decltype(&BIYOS_delvarnamerg) delete_ranged;
};

const std::array<NameCalls, 4> kNameCalls{{
    {BIYOS_getnvar,
     BIYOS_setvarnamesp,
     BIYOS_setvarnamerg,
     BIYOS_getvarnamesp,
     BIYOS_getvarnamerg,
     BIYOS_delvarnamesp,
     BIYOS_delvarnamerg},
    {BIYOS_getnfun,
     BIYOS_setfunnamesp,
     BIYOS_setfunnamerg,
     BIYOS_getfunnamesp,
     BIYOS_getfunnamerg,
     BIYOS_delfunnamesp,
     BIYOS_delfunnamerg},
    {BIYOS_getncon,
     BIYOS_setconnamesp,
     BIYOS_setconnamerg,
     BIYOS_getconnamesp,
     BIYOS_getconnamerg,
     BIYOS_delconnamesp,
     BIYOS_delconnamerg},
    {BIYOS_getnobj,
     BIYOS_setobjnamesp,
     BIYOS_setobjnamerg,
     BIYOS_getobjnamesp,
     BIYOS_getobjnamerg,
     BIYOS_delobjnamesp,
     BIYOS_delobjnamerg},
}};

std::uint64_t key(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

int key(int value)
{
    return value;
}

std::string key(const char *value)
{
    return value == nullptr ? "(null)" : value;
}

template <typename Value> auto keys(const std::vector<Value> &values)
{
    std::vector<decltype(key(Value{}))> found;
    for (const Value value : values)
    {
        found.push_back(key(value));
    }
    return found;
}

int count_of(const Model &model, Counter count)
{
    BIYOS_index n = -1;
    EXPECT_EQ(count(model.get(), &n), BIYOS_OK);
    return n;
}

Span random_span(Engine &engine, int count)
{
    const int start = below(engine, count + 1);
    return {.start = start, .size = below(engine, count - start + 1)};
}

Indices indices_of(Span span)
{
    Indices indices(static_cast<std::size_t>(span.size));
    std::iota(indices.begin(), indices.end(), span.start);
    return indices;
}

double low(Engine &engine)
{
    return below(engine, 4) / 8.0;
}

double high(Engine &engine)
{
    return 0.5 + below(engine, 4) / 8.0;
}

double weight(Engine &engine)
{
    return (1 + below(engine, 4)) / 2.0;
}

int kind(Engine &engine)
{
    return 1 + below(engine, 5);
}

int sense(Engine &engine)
{
    return 1 + below(engine, 2);
}

int priority(Engine &engine)
{
    return below(engine, 7) - 3;
}

template <typename Value>
std::vector<Value> drawn(Engine &engine, std::size_t n, Value (*draw)(Engine &))
{
    std::vector<Value> values;
    for (std::size_t k = 0; k < n; ++k)
    {
        values.push_back(draw(engine));
    }
    return values;
}

Names drawn_names(Engine &engine, std::size_t n)
{
    Names names;
    for (std::size_t k = 0; k < n; ++k)
    {
        names.texts.push_back("r" + std::to_string(below(engine, 8)));
    }
    for (const std::string &text : names.texts)
    {
        names.pointers.push_back(text.c_str());
    }
    return names;
}

template <typename Listed, typename Ranged, typename... Values>
void expect_writes(const Held &held,
                   Span span,
                   Listed listed,
                   Ranged ranged,
                   const std::vector<Values> &...values)
{
    const Model by_list(held.env, held.model);
    const Model by_range(held.env, held.model);
    const Indices indices = indices_of(span);
    EXPECT_EQ(listed(by_list.get(), span.size, indices.data(), values.data()...), BIYOS_OK);
    EXPECT_EQ(ranged(by_range.get(), span.start, span.size, values.data()...), BIYOS_OK);
    EXPECT_EQ(biyos::difference(by_list.get()->core, by_range.get()->core), "");
}

template <typename Listed, typename... Values>
auto read_listed(const Model &model, Span span, Listed listed, std::vector<Values>... outputs)
{
    const Indices indices = indices_of(span);
    EXPECT_EQ(listed(model.get(), span.size, indices.data(), outputs.data()...), BIYOS_OK);
    return std::make_tuple(keys(outputs)...);
}

template <typename Ranged, typename... Values>
auto read_ranged(const Model &model, Span span, Ranged ranged, std::vector<Values>... outputs)
{
    EXPECT_EQ(ranged(model.get(), span.start, span.size, outputs.data()...), BIYOS_OK);
    return std::make_tuple(keys(outputs)...);
}

template <typename Listed, typename Ranged, typename... Values>
void expect_reads(const Model &model,
                  Span span,
                  Listed listed,
                  Ranged ranged,
                  const std::vector<Values> &...outputs)
{
    EXPECT_EQ(read_listed(model, span, listed, outputs...),
              read_ranged(model, span, ranged, outputs...));
}

template <typename Ranged, typename... Values>
void expect_kept(const Held &held,
                 Span span,
                 BIYOS_status wanted,
                 Ranged ranged,
                 std::vector<Values>... values)
{
    const Model tried(held.env, held.model);
    const auto before = std::make_tuple(keys(values)...);
    EXPECT_EQ(ranged(tried.get(), span.start, span.size, values.data()...), wanted);
    EXPECT_EQ(std::make_tuple(keys(values)...), before);
    EXPECT_EQ(biyos::difference(tried.get()->core, held.model.get()->core), "");
}

void write_variables(const Held &held, Engine &engine)
{
    const Span span = random_span(engine, count_of(held.model, BIYOS_getnvar));
    const auto n = static_cast<std::size_t>(span.size);
    const std::vector<double> lower = drawn(engine, n, low);
    const std::vector<double> upper = drawn(engine, n, high);
    const std::vector<int> kinds = drawn(engine, n, kind);
    expect_writes(held, span, BIYOS_setvarbndsp, BIYOS_setvarbndrg, lower, upper);
    expect_writes(held, span, BIYOS_setvarkndsp, BIYOS_setvarkndrg, kinds);
}

void write_constraints(const Held &held, Engine &engine)
{
    const Span span = random_span(engine, count_of(held.model, BIYOS_getncon));
    const auto n = static_cast<std::size_t>(span.size);
    const std::vector<double> lower = drawn(engine, n, low);
    const std::vector<double> upper = drawn(engine, n, high);
    expect_writes(held, span, BIYOS_setconbndsp, BIYOS_setconbndrg, lower, upper);
}

void write_objectives(const Held &held, Engine &engine)
{
    const Span span = random_span(engine, count_of(held.model, BIYOS_getnobj));
    const auto n = static_cast<std::size_t>(span.size);
    const std::vector<int> senses = drawn(engine, n, sense);
    const std::vector<int> priorities = drawn(engine, n, priority);
    const std::vector<double> weights = drawn(engine, n, weight);
    const std::vector<double> absolute = drawn(engine, n, low);
    const std::vector<double> relative = drawn(engine, n, high);
    expect_writes(held, span, BIYOS_setobjsensesp, BIYOS_setobjsenserg, senses);
    expect_writes(held, span, BIYOS_setobjpriosp, BIYOS_setobjpriorg, priorities);
    expect_writes(held, span, BIYOS_setobjcoefsp, BIYOS_setobjcoefrg, weights);
    expect_writes(held, span, BIYOS_setobjtolsp, BIYOS_setobjtolrg, absolute, relative);
}

void write_names(const Held &held, Engine &engine, const NameCalls &calls)
{
    const Span span = random_span(engine, count_of(held.model, calls.count));
    const Names names = drawn_names(engine, static_cast<std::size_t>(span.size));
    expect_writes(held, span, calls.set_listed, calls.set_ranged, names.pointers);
    expect_writes(held, span, calls.delete_listed, calls.delete_ranged);
}

template <typename Value> std::vector<Value> zeros(Span span)
{
    return std::vector<Value>(static_cast<std::size_t>(span.size));
}

void read_variables(const Model &model, Engine &engine)
{
    const Span span = random_span(engine, count_of(model, BIYOS_getnvar));
    const std::vector<double> reals = zeros<double>(span);
    const std::vector<int> whole = zeros<int>(span);
    expect_reads(model, span, BIYOS_getvarbndsp, BIYOS_getvarbndrg, reals, reals);
    expect_reads(model, span, BIYOS_getvarkndsp, BIYOS_getvarkndrg, whole);
}

void read_constraints(const Model &model, Engine &engine)
{
    const Span span = random_span(engine, count_of(model, BIYOS_getncon));
    const std::vector<double> reals = zeros<double>(span);
    expect_reads(model, span, BIYOS_getconbndsp, BIYOS_getconbndrg, reals, reals);
}

void read_objectives(const Model &model, Engine &engine)
{
    const Span span = random_span(engine, count_of(model, BIYOS_getnobj));
    const std::vector<double> reals = zeros<double>(span);
    const std::vector<int> whole = zeros<int>(span);
    expect_reads(model, span, BIYOS_getobjsensesp, BIYOS_getobjsenserg, whole);
    expect_reads(model, span, BIYOS_getobjpriosp, BIYOS_getobjpriorg, whole);
    expect_reads(model, span, BIYOS_getobjcoefsp, BIYOS_getobjcoefrg, reals);
    expect_reads(model, span, BIYOS_getobjtolsp, BIYOS_getobjtolrg, reals, reals);
}

void read_all(const Model &model, Engine &engine)
{
    read_variables(model, engine);
    read_constraints(model, engine);
    read_objectives(model, engine);
    for (const NameCalls &calls : kNameCalls)
    {
        const Span span = random_span(engine, count_of(model, calls.count));
        const std::vector<const char *> names = zeros<const char *>(span);
        expect_reads(model, span, calls.get_listed, calls.get_ranged, names);
    }
}

std::vector<Span> bad_spans(int count)
{
    return {{-1, 0}, {-1, 1}, {0, -1}, {count, 1}, {count + 1, 0}, {0, count + 1}, {1, count}};
}

void keep_variables(const Held &held, Span span, BIYOS_status wanted)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, BIYOS_getnvar) + 1);
    const std::vector<double> bounds(n, 0.5);
    const std::vector<int> kinds(n, BIYOS_VAR_INT);
    expect_kept(held, span, wanted, BIYOS_setvarbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_getvarbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_setvarkndrg, kinds);
    expect_kept(held, span, wanted, BIYOS_getvarkndrg, kinds);
}

void keep_constraints(const Held &held, Span span, BIYOS_status wanted)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, BIYOS_getncon) + 1);
    const std::vector<double> bounds(n, 0.5);
    expect_kept(held, span, wanted, BIYOS_setconbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_getconbndrg, bounds, bounds);
}

void keep_objectives(const Held &held, Span span, BIYOS_status wanted)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, BIYOS_getnobj) + 1);
    const std::vector<int> senses(n, BIYOS_MAXIMIZE);
    const std::vector<double> values(n, 0.5);
    expect_kept(held, span, wanted, BIYOS_setobjsenserg, senses);
    expect_kept(held, span, wanted, BIYOS_getobjsenserg, senses);
    expect_kept(held, span, wanted, BIYOS_setobjpriorg, senses);
    expect_kept(held, span, wanted, BIYOS_getobjpriorg, senses);
    expect_kept(held, span, wanted, BIYOS_setobjcoefrg, values);
    expect_kept(held, span, wanted, BIYOS_getobjcoefrg, values);
    expect_kept(held, span, wanted, BIYOS_setobjtolrg, values, values);
    expect_kept(held, span, wanted, BIYOS_getobjtolrg, values, values);
}

void keep_names(const Held &held, Span span, BIYOS_status wanted, const NameCalls &calls)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, calls.count) + 1);
    const std::vector<const char *> names(n, "kept");
    expect_kept(held, span, wanted, calls.set_ranged, names);
    expect_kept(held, span, wanted, calls.get_ranged, names);
    expect_kept(held, span, wanted, calls.delete_ranged);
}

void keep_all(const Held &held, Counter count, Span span, BIYOS_status wanted)
{
    if (count == BIYOS_getnvar)
    {
        keep_variables(held, span, wanted);
    }
    if (count == BIYOS_getncon)
    {
        keep_constraints(held, span, wanted);
    }
    if (count == BIYOS_getnobj)
    {
        keep_objectives(held, span, wanted);
    }
    for (const NameCalls &calls : kNameCalls)
    {
        if (calls.count == count)
        {
            keep_names(held, span, wanted, calls);
        }
    }
}

}

TEST(RangeTest, Writes)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        const Held held{.env = env, .model = model};
        write_variables(held, engine);
        write_constraints(held, engine);
        write_objectives(held, engine);
        for (const NameCalls &calls : kNameCalls)
        {
            write_names(held, engine, calls);
        }
    }
}

TEST(RangeTest, Reads)
{
    for (const std::uint32_t seed : kSeeds)
    {
        Engine engine(seed);
        const Env env;
        const Model model(env);
        build_sample(model.get(), engine);
        read_all(model, engine);
    }
}

TEST(RangeTest, Refusals)
{
    Engine engine(kSeeds[0]);
    const Env env;
    const Model model(env);
    build_sample(model.get(), engine);
    const Held held{.env = env, .model = model};
    for (const NameCalls &calls : kNameCalls)
    {
        const int count = count_of(model, calls.count);
        for (const Span span : bad_spans(count))
        {
            keep_all(held, calls.count, span, BIYOS_ERR_BADARG);
        }
        keep_all(held, calls.count, {count, 0}, BIYOS_OK);
    }
}

TEST(RangeTest, Atomic)
{
    Engine engine(kSeeds[1]);
    const Env env;
    const Model model(env);
    build_sample(model.get(), engine);
    const Model tried(env, model);
    const auto n = static_cast<std::size_t>(count_of(model, BIYOS_getnvar));
    std::vector<double> lower(n, 0.0);
    lower.back() = std::numeric_limits<double>::quiet_NaN();
    const BIYOS_index size = count_of(model, BIYOS_getnvar);
    EXPECT_EQ(BIYOS_setvarbndrg(tried.get(), 0, size, lower.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(biyos::difference(tried.get()->core, model.get()->core), "");
}

TEST(RangeTest, Messages)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    double bound = 0.0;
    EXPECT_EQ(BIYOS_setvarbndrg(model.get(), 0, -1, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_getvarbndrg(model.get(), 4, 0, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 4: unknown");
    EXPECT_EQ(BIYOS_delvarnamerg(model.get(), 2, 2), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
}
