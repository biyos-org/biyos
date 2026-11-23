#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "handles.hpp"
#include "model/compare.hpp"
#include "sample.hpp"
#include "tolerance.hpp"

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
using biyos::test::ToleranceCalls;

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

const std::array<NameCalls, 6> kNameCalls{{
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
    {BIYOS_getnsos,
     BIYOS_setsosnamesp,
     BIYOS_setsosnamerg,
     BIYOS_getsosnamesp,
     BIYOS_getsosnamerg,
     BIYOS_delsosnamesp,
     BIYOS_delsosnamerg},
    {BIYOS_getncone,
     BIYOS_setconenamesp,
     BIYOS_setconenamerg,
     BIYOS_getconenamesp,
     BIYOS_getconenamerg,
     BIYOS_delconenamesp,
     BIYOS_delconenamerg},
}};

const NameCalls kIndicatorNames{BIYOS_getnind,
                                BIYOS_setindnamesp,
                                BIYOS_setindnamerg,
                                BIYOS_getindnamesp,
                                BIYOS_getindnamerg,
                                BIYOS_delindnamesp,
                                BIYOS_delindnamerg};

const NameCalls kComplementNames{BIYOS_getnccon,
                                 BIYOS_setcconnamesp,
                                 BIYOS_setcconnamerg,
                                 BIYOS_getcconnamesp,
                                 BIYOS_getcconnamerg,
                                 BIYOS_delcconnamesp,
                                 BIYOS_delcconnamerg};

const std::array<ToleranceCalls, 3> kToleranceCalls{
    biyos::test::kVariableTolerances,
    biyos::test::kConstraintTolerances,
    biyos::test::kConeTolerances,
};

std::uint64_t key(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

int key(int value)
{
    return value;
}

std::int64_t key(std::int64_t value)
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

void write_functions(const Held &held, Engine &engine)
{
    const Span span = random_span(engine, count_of(held.model, BIYOS_getnfun));
    const std::vector<double> constants =
        drawn(engine, static_cast<std::size_t>(span.size), weight);
    expect_writes(held, span, BIYOS_setcstsp, BIYOS_setcstrg, constants);
    expect_writes(held, span, BIYOS_addcstsp, BIYOS_addcstrg, constants);
    expect_writes(held, span, BIYOS_clrfunsp, BIYOS_clrfunrg);
}

BIYOS_offset add_expression(const Model &model)
{
    BIYOS_offset x = -1;
    BIYOS_offset sine = -1;
    BIYOS_offset sum = -1;
    EXPECT_EQ(BIYOS_addvarnode(model.get(), 0, &x), BIYOS_OK);
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_SIN, 1, &x, nullptr, &sine), BIYOS_OK);
    const std::array<BIYOS_offset, 2> children{x, sine};
    const std::array<double, 3> values{0.5, 2.0, 1.0};
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_SUM, 2, children.data(), values.data(), &sum),
              BIYOS_OK);
    return sum;
}

void write_expressions(const Held &held, Engine &engine)
{
    const Model noded(held.env, held.model);
    const BIYOS_offset sum = add_expression(noded);
    const Span span = random_span(engine, count_of(noded, BIYOS_getnfun));
    const std::vector<BIYOS_offset> nodes(static_cast<std::size_t>(span.size), sum);
    const Held changed{.env = held.env, .model = noded};
    expect_writes(changed, span, BIYOS_attachexprsp, BIYOS_attachexprrg, nodes);
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

void write_tolerances(const Held &held, Engine &engine, const ToleranceCalls &calls)
{
    const Span span = random_span(engine, count_of(held.model, calls.count));
    const auto n = static_cast<std::size_t>(span.size);
    const std::vector<double> absolute = drawn(engine, n, low);
    const std::vector<double> relative = drawn(engine, n, high);
    expect_writes(held, span, calls.set_listed, calls.set_ranged, absolute, relative);
    const Model tolerated(held.env, held.model);
    const BIYOS_status status =
        calls.set_ranged(tolerated.get(), span.start, span.size, absolute.data(), relative.data());
    EXPECT_EQ(status, BIYOS_OK);
    const Held changed{.env = held.env, .model = tolerated};
    const Span deleted = random_span(engine, count_of(tolerated, calls.count));
    expect_writes(changed, deleted, calls.delete_listed, calls.delete_ranged);
}

struct Switches
{
    int start;
    int size;
};

Switches add_switches(const Model &model, Engine &engine)
{
    const Switches switches{.start = count_of(model, BIYOS_getnvar), .size = 1 + below(engine, 4)};
    const std::vector<int> kinds(static_cast<std::size_t>(switches.size), BIYOS_VAR_BIN);
    EXPECT_EQ(BIYOS_addvar(model.get(), switches.size, kinds.data(), nullptr, nullptr, nullptr),
              BIYOS_OK);
    return switches;
}

std::vector<int> drawn_switches(Engine &engine, std::size_t n, Switches switches)
{
    std::vector<int> variables;
    for (std::size_t k = 0; k < n; ++k)
    {
        variables.push_back(switches.start + below(engine, switches.size));
    }
    return variables;
}

int value(Engine &engine)
{
    return below(engine, 2);
}

void add_indicators(const Model &model, Engine &engine, Switches switches)
{
    const int functions = count_of(model, BIYOS_getnfun);
    const auto n = static_cast<std::size_t>(1 + below(engine, 6));
    const std::vector<int> variables = drawn_switches(engine, n, switches);
    const std::vector<int> values = drawn(engine, n, value);
    std::vector<int> owners;
    for (std::size_t k = 0; k < n; ++k)
    {
        owners.push_back(below(engine, functions));
    }
    const std::vector<double> lower = drawn(engine, n, low);
    const std::vector<double> upper = drawn(engine, n, high);
    const BIYOS_status status = BIYOS_addind(model.get(),
                                             static_cast<BIYOS_index>(n),
                                             variables.data(),
                                             values.data(),
                                             owners.data(),
                                             lower.data(),
                                             upper.data(),
                                             nullptr);
    EXPECT_EQ(status, BIYOS_OK);
}

void write_indicators(const Held &held, Engine &engine)
{
    const Model switched(held.env, held.model);
    const Switches switches = add_switches(switched, engine);
    add_indicators(switched, engine, switches);
    const Held changed{.env = held.env, .model = switched};
    const Span span = random_span(engine, count_of(switched, BIYOS_getnind));
    const auto n = static_cast<std::size_t>(span.size);
    const std::vector<double> lower = drawn(engine, n, low);
    const std::vector<double> upper = drawn(engine, n, high);
    const std::vector<int> variables = drawn_switches(engine, n, switches);
    const std::vector<int> values = drawn(engine, n, value);
    expect_writes(changed, span, BIYOS_setindbndsp, BIYOS_setindbndrg, lower, upper);
    expect_writes(changed, span, BIYOS_setindvarsp, BIYOS_setindvarrg, variables, values);
    write_names(changed, engine, kIndicatorNames);
}

void add_complements(const Model &model, Engine &engine)
{
    const int functions = count_of(model, BIYOS_getnfun);
    const auto n = static_cast<std::size_t>(1 + below(engine, 6));
    std::vector<int> first;
    std::vector<int> second;
    for (std::size_t k = 0; k < n; ++k)
    {
        first.push_back(below(engine, functions));
        second.push_back(below(engine, functions));
    }
    const std::vector<double> lower = drawn(engine, n, low);
    const std::vector<double> upper = drawn(engine, n, high);
    BIYOS_index start = -1;
    const BIYOS_status status = BIYOS_addccon(model.get(),
                                              static_cast<BIYOS_index>(n),
                                              first.data(),
                                              second.data(),
                                              lower.data(),
                                              upper.data(),
                                              &start);
    EXPECT_EQ(status, BIYOS_OK);
    const auto half = static_cast<BIYOS_index>(n / 2);
    EXPECT_EQ(BIYOS_setccontolrg(model.get(), start, half, lower.data(), upper.data()), BIYOS_OK);
}

void write_complements(const Held &held, Engine &engine)
{
    const Model paired(held.env, held.model);
    add_complements(paired, engine);
    const Held changed{.env = held.env, .model = paired};
    const Span span = random_span(engine, count_of(paired, BIYOS_getnccon));
    const auto n = static_cast<std::size_t>(span.size);
    const std::vector<double> lower = drawn(engine, n, low);
    const std::vector<double> upper = drawn(engine, n, high);
    expect_writes(changed, span, BIYOS_setcconbndsp, BIYOS_setcconbndrg, lower, upper);
    expect_writes(changed, span, BIYOS_setccontolsp, BIYOS_setccontolrg, lower, upper);
    expect_writes(changed, span, BIYOS_delccontolsp, BIYOS_delccontolrg);
    write_names(changed, engine, kComplementNames);
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
    expect_reads(model, span, BIYOS_getvarsp, BIYOS_getvarrg, whole, reals, reals);
}

void read_functions(const Model &model, Engine &engine)
{
    const Span span = random_span(engine, count_of(model, BIYOS_getnfun));
    const std::vector<double> reals = zeros<double>(span);
    expect_reads(model, span, BIYOS_getcstsp, BIYOS_getcstrg, reals);
}

void read_constraints(const Model &model, Engine &engine)
{
    const Span span = random_span(engine, count_of(model, BIYOS_getncon));
    const std::vector<double> reals = zeros<double>(span);
    const std::vector<int> whole = zeros<int>(span);
    expect_reads(model, span, BIYOS_getconbndsp, BIYOS_getconbndrg, reals, reals);
    expect_reads(model, span, BIYOS_getconsp, BIYOS_getconrg, whole, reals, reals);
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
    expect_reads(model, span, BIYOS_getobjsp, BIYOS_getobjrg, whole, whole, whole, reals);
}

void read_indicators(const Held &held, Engine &engine)
{
    const Model switched(held.env, held.model);
    add_indicators(switched, engine, add_switches(switched, engine));
    const Span span = random_span(engine, count_of(switched, BIYOS_getnind));
    const std::vector<double> reals = zeros<double>(span);
    const std::vector<int> whole = zeros<int>(span);
    const std::vector<const char *> names = zeros<const char *>(span);
    expect_reads(switched, span, BIYOS_getindbndsp, BIYOS_getindbndrg, reals, reals);
    expect_reads(switched, span, BIYOS_getindvarsp, BIYOS_getindvarrg, whole, whole);
    expect_reads(switched, span, BIYOS_getindsp, BIYOS_getindrg, whole, whole, whole, reals, reals);
    expect_reads(switched, span, kIndicatorNames.get_listed, kIndicatorNames.get_ranged, names);
}

void read_complements(const Held &held, Engine &engine)
{
    const Model paired(held.env, held.model);
    add_complements(paired, engine);
    const Span span = random_span(engine, count_of(paired, BIYOS_getnccon));
    const std::vector<double> reals = zeros<double>(span);
    const std::vector<int> whole = zeros<int>(span);
    const std::vector<const char *> names = zeros<const char *>(span);
    expect_reads(paired, span, BIYOS_getcconbndsp, BIYOS_getcconbndrg, reals, reals);
    expect_reads(paired, span, BIYOS_getcconsp, BIYOS_getcconrg, whole, whole, reals, reals);
    expect_reads(paired, span, BIYOS_getccontolsp, BIYOS_getccontolrg, reals, reals);
    expect_reads(paired, span, kComplementNames.get_listed, kComplementNames.get_ranged, names);
}

void read_all(const Model &model, Engine &engine)
{
    read_variables(model, engine);
    read_functions(model, engine);
    read_constraints(model, engine);
    read_objectives(model, engine);
    for (const NameCalls &calls : kNameCalls)
    {
        const Span span = random_span(engine, count_of(model, calls.count));
        const std::vector<const char *> names = zeros<const char *>(span);
        expect_reads(model, span, calls.get_listed, calls.get_ranged, names);
    }
    for (const ToleranceCalls &calls : kToleranceCalls)
    {
        const Span span = random_span(engine, count_of(model, calls.count));
        const std::vector<double> reals = zeros<double>(span);
        expect_reads(model, span, calls.get_listed, calls.get_ranged, reals, reals);
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
    expect_kept(held, span, wanted, BIYOS_getvarrg, kinds, bounds, bounds);
}

void keep_functions(const Held &held, Span span, BIYOS_status wanted)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, BIYOS_getnfun) + 1);
    const std::vector<double> constants(n, 0.5);
    expect_kept(held, span, wanted, BIYOS_setcstrg, constants);
    expect_kept(held, span, wanted, BIYOS_addcstrg, constants);
    expect_kept(held, span, wanted, BIYOS_getcstrg, constants);
    const std::vector<BIYOS_offset> nodes(n, 0);
    expect_kept(held, span, wanted, BIYOS_attachexprrg, nodes);
    expect_kept(held, span, wanted, BIYOS_clrfunrg);
}

void keep_constraints(const Held &held, Span span, BIYOS_status wanted)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, BIYOS_getncon) + 1);
    const std::vector<double> bounds(n, 0.5);
    const std::vector<int> functions(n, 0);
    expect_kept(held, span, wanted, BIYOS_setconbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_getconbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_getconrg, functions, bounds, bounds);
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
    expect_kept(held, span, wanted, BIYOS_getobjrg, senses, senses, senses, values);
}

BIYOS_status read_sos(const BIYOS_model *model,
                      BIYOS_index pos,
                      BIYOS_index n,
                      int *kinds,
                      BIYOS_offset *starts,
                      BIYOS_index *variables,
                      double *weights)
{
    return BIYOS_getsosrg(model, pos, n, 0, kinds, starts, variables, weights);
}

void keep_sos(const Held &held, Span span, BIYOS_status wanted)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, BIYOS_getnsos) + 1);
    const std::vector<int> kinds(n, BIYOS_SOSKND_SOS1);
    const std::vector<BIYOS_offset> starts(n, 0);
    const std::vector<double> weights(n, 0.5);
    expect_kept(held, span, wanted, BIYOS_getnsosvarrg, starts);
    expect_kept(held, span, wanted, read_sos, kinds, starts, kinds, weights);
}

void keep_names(const Held &held, Span span, BIYOS_status wanted, const NameCalls &calls)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, calls.count) + 1);
    const std::vector<const char *> names(n, "kept");
    expect_kept(held, span, wanted, calls.set_ranged, names);
    expect_kept(held, span, wanted, calls.get_ranged, names);
    expect_kept(held, span, wanted, calls.delete_ranged);
}

void keep_indicators(const Held &held, Span span, BIYOS_status wanted, Switches switches)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, BIYOS_getnind) + 1);
    const std::vector<double> bounds(n, 0.5);
    const std::vector<int> variables(n, switches.start);
    const std::vector<int> values(n, 1);
    expect_kept(held, span, wanted, BIYOS_setindbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_getindbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_setindvarrg, variables, values);
    expect_kept(held, span, wanted, BIYOS_getindvarrg, variables, values);
    expect_kept(held, span, wanted, BIYOS_getindrg, variables, values, variables, bounds, bounds);
    keep_names(held, span, wanted, kIndicatorNames);
}

void keep_complements(const Held &held, Span span, BIYOS_status wanted)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, BIYOS_getnccon) + 1);
    const std::vector<double> bounds(n, 0.5);
    const std::vector<int> functions(n, 0);
    expect_kept(held, span, wanted, BIYOS_setcconbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_getcconbndrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_getcconrg, functions, functions, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_setccontolrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_getccontolrg, bounds, bounds);
    expect_kept(held, span, wanted, BIYOS_delccontolrg);
    keep_names(held, span, wanted, kComplementNames);
}

void keep_tolerances(const Held &held, Span span, BIYOS_status wanted, const ToleranceCalls &calls)
{
    const auto n = static_cast<std::size_t>(count_of(held.model, calls.count) + 1);
    const std::vector<double> values(n, 0.5);
    expect_kept(held, span, wanted, calls.set_ranged, values, values);
    expect_kept(held, span, wanted, calls.get_ranged, values, values);
    expect_kept(held, span, wanted, calls.delete_ranged);
}

void keep_all(const Held &held, Counter count, Span span, BIYOS_status wanted)
{
    if (count == BIYOS_getnvar)
    {
        keep_variables(held, span, wanted);
    }
    if (count == BIYOS_getnfun)
    {
        keep_functions(held, span, wanted);
    }
    if (count == BIYOS_getncon)
    {
        keep_constraints(held, span, wanted);
    }
    if (count == BIYOS_getnobj)
    {
        keep_objectives(held, span, wanted);
    }
    if (count == BIYOS_getnsos)
    {
        keep_sos(held, span, wanted);
    }
    for (const NameCalls &calls : kNameCalls)
    {
        if (calls.count == count)
        {
            keep_names(held, span, wanted, calls);
        }
    }
    for (const ToleranceCalls &calls : kToleranceCalls)
    {
        if (calls.count == count)
        {
            keep_tolerances(held, span, wanted, calls);
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
        write_functions(held, engine);
        write_expressions(held, engine);
        write_constraints(held, engine);
        write_objectives(held, engine);
        for (const NameCalls &calls : kNameCalls)
        {
            write_names(held, engine, calls);
        }
        for (const ToleranceCalls &calls : kToleranceCalls)
        {
            write_tolerances(held, engine, calls);
        }
        write_indicators(held, engine);
        write_complements(held, engine);
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
        read_indicators(Held{.env = env, .model = model}, engine);
        read_complements(Held{.env = env, .model = model}, engine);
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

TEST(RangeTest, IndicatorRefusals)
{
    Engine engine(kSeeds[0]);
    const Env env;
    const Model model(env);
    build_sample(model.get(), engine);
    const Switches switches = add_switches(model, engine);
    add_indicators(model, engine, switches);
    const Held held{.env = env, .model = model};
    const int count = count_of(model, BIYOS_getnind);
    for (const Span span : bad_spans(count))
    {
        keep_indicators(held, span, BIYOS_ERR_BADARG, switches);
    }
    keep_indicators(held, {count, 0}, BIYOS_OK, switches);
}

TEST(RangeTest, ComplementRefusals)
{
    Engine engine(kSeeds[0]);
    const Env env;
    const Model model(env);
    build_sample(model.get(), engine);
    add_complements(model, engine);
    const Held held{.env = env, .model = model};
    const int count = count_of(model, BIYOS_getnccon);
    for (const Span span : bad_spans(count))
    {
        keep_complements(held, span, BIYOS_ERR_BADARG);
    }
    keep_complements(held, {count, 0}, BIYOS_OK);
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
