#include "core/error.hpp"
#include "core/index.hpp"
#include "model/compare.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"
#include "model/renumber.hpp"
#include "refusal.hpp"
#include "sample.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using biyos::ConeKind;
using biyos::Family;
using biyos::Index;
using biyos::kDeleted;
using biyos::Model;
using biyos::Offset;
using biyos::Sense;
using biyos::SosKind;
using biyos::test::built;
using biyos::test::Naming;
using biyos::test::Parts;
using biyos::test::refusal;
using Map = std::vector<Index>;
using Indices = std::vector<Index>;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Case
{
    Family family;
    Map (Model::*call)(std::span<const Index>);
    void (*keep)(Parts &, const Map &);
    Naming Parts::*names;
};

void add_records(Parts &parts)
{
    parts.objectives.push_back({
        .function = 1,
        .sense = Sense::Minimize,
        .priority = 2,
        .weight = 0.5,
        .absolute = 0.0,
        .relative = 1e-3,
    });
    parts.indicators.push_back(
        {.variable = 1, .value = 1, .function = 0, .lower = 0.0, .upper = 2.0});
    parts.sos.kinds.push_back(SosKind::Sos1);
    parts.sos.starts.push_back(7);
    parts.sos.members.insert(parts.sos.members.end(),
                             {{.variable = 2, .weight = 1.0}, {.variable = 0, .weight = 2.0}});
    parts.cones.kinds.push_back(ConeKind::DualPower);
    parts.cones.starts.push_back(8);
    parts.cones.functions.insert(parts.cones.functions.end(), {1, 0, 2});
    parts.cones.parameter_starts.push_back(3);
    parts.cones.parameters.push_back(0.5);
    parts.complements.push_back(
        {.function = 2, .lower = -kInfinity, .upper = 0.0, .multiplier = 1});
}

void add_names(Parts &parts)
{
    parts.constraint_names = {.indices = {0, 1, 2}, .names = {"a", "c", "a"}};
    parts.objective_names = {.indices = {0}, .names = {"cost"}};
    parts.indicator_names = {.indices = {1, 2}, .names = {"off", "on"}};
    parts.sos_names = {.indices = {2, 1}, .names = {"t", "s"}};
    parts.cone_names = {.indices = {1, 2}, .names = {"k", "k"}};
    parts.complement_names = {.indices = {2, 1}, .names = {"pair", "pair"}};
}

Parts three()
{
    Parts parts = biyos::test::sample();
    add_records(parts);
    add_names(parts);
    return parts;
}

template <typename T> std::vector<T> kept(const std::vector<T> &items, const Map &map)
{
    std::vector<T> found;
    for (std::size_t k = 0; k < items.size(); ++k)
    {
        if (map[k] != kDeleted)
        {
            found.push_back(items[k]);
        }
    }
    return found;
}

template <typename T>
void kept_parts(std::vector<Offset> &starts, std::vector<T> &items, const Map &map)
{
    std::vector<Offset> kept_starts{0};
    std::vector<T> kept_items;
    for (std::size_t k = 0; k < map.size(); ++k)
    {
        if (map[k] != kDeleted)
        {
            const std::span<const T> part = biyos::slice(std::span<const T>(items), starts, k);
            kept_items.insert(kept_items.end(), part.begin(), part.end());
            kept_starts.push_back(static_cast<Offset>(kept_items.size()));
        }
    }
    starts = kept_starts;
    items = kept_items;
}

Naming renamed(const Naming &naming, const Map &map)
{
    Naming found;
    for (std::size_t k = 0; k < naming.indices.size(); ++k)
    {
        const Index to = map[static_cast<std::size_t>(naming.indices[k])];
        if (to != kDeleted)
        {
            found.indices.push_back(to);
            found.names.push_back(naming.names[k]);
        }
    }
    return found;
}

void keep_constraints(Parts &parts, const Map &map)
{
    parts.constraints = kept(parts.constraints, map);
    parts.constraint_names = renamed(parts.constraint_names, map);
}

void keep_objectives(Parts &parts, const Map &map)
{
    parts.objectives = kept(parts.objectives, map);
    parts.objective_names = renamed(parts.objective_names, map);
}

void keep_indicators(Parts &parts, const Map &map)
{
    parts.indicators = kept(parts.indicators, map);
    parts.indicator_names = renamed(parts.indicator_names, map);
}

void keep_sos(Parts &parts, const Map &map)
{
    parts.sos.kinds = kept(parts.sos.kinds, map);
    kept_parts(parts.sos.starts, parts.sos.members, map);
    parts.sos_names = renamed(parts.sos_names, map);
}

void keep_cones(Parts &parts, const Map &map)
{
    parts.cones.kinds = kept(parts.cones.kinds, map);
    kept_parts(parts.cones.starts, parts.cones.functions, map);
    kept_parts(parts.cones.parameter_starts, parts.cones.parameters, map);
    parts.cone_names = renamed(parts.cone_names, map);
}

void keep_complements(Parts &parts, const Map &map)
{
    parts.complements = kept(parts.complements, map);
    parts.complement_names = renamed(parts.complement_names, map);
}

const Case kConstraints{
    .family = Family::Constraint,
    .call = &Model::delete_constraints,
    .keep = keep_constraints,
    .names = &Parts::constraint_names,
};

const Case kObjectives{
    .family = Family::Objective,
    .call = &Model::delete_objectives,
    .keep = keep_objectives,
    .names = &Parts::objective_names,
};

const Case kIndicators{
    .family = Family::Indicator,
    .call = &Model::delete_indicators,
    .keep = keep_indicators,
    .names = &Parts::indicator_names,
};

const Case kSos{
    .family = Family::Sos,
    .call = &Model::delete_sos,
    .keep = keep_sos,
    .names = &Parts::sos_names,
};

const Case kCones{
    .family = Family::Cone,
    .call = &Model::delete_cones,
    .keep = keep_cones,
    .names = &Parts::cone_names,
};

const Case kComplements{
    .family = Family::Complement,
    .call = &Model::delete_complements,
    .keep = keep_complements,
    .names = &Parts::complement_names,
};

const std::array kCases{kConstraints, kObjectives, kIndicators, kSos, kCones, kComplements};

std::string lookup(const Model &model, Family family, std::string_view name)
{
    try
    {
        return std::to_string(model.index_of(family, name));
    }
    catch (const biyos::Error &error)
    {
        return error.what();
    }
}

void expect_lookups(const Model &model, const Model &wanted, const Case &family)
{
    const Naming &naming = three().*family.names;
    for (const std::string_view name : naming.names)
    {
        SCOPED_TRACE(name);
        EXPECT_EQ(lookup(model, family.family, name), lookup(wanted, family.family, name));
    }
}

void expect_deleted(const Case &family, const Indices &indices, const Map &map)
{
    SCOPED_TRACE(testing::PrintToString(indices));
    Model model = built(three());
    EXPECT_EQ((model.*family.call)(indices), map);
    Parts parts = three();
    family.keep(parts, map);
    const Model wanted = built(parts);
    EXPECT_EQ(biyos::difference(model, wanted), "");
    EXPECT_EQ(model.n_of(family.family), wanted.n_of(family.family));
    expect_lookups(model, wanted, family);
}

void expect_each(const Case &family)
{
    expect_deleted(family, {0}, {kDeleted, 0, 1});
    expect_deleted(family, {1}, {0, kDeleted, 1});
    expect_deleted(family, {2}, {0, 1, kDeleted});
    expect_deleted(family, {2, 0}, {kDeleted, 0, kDeleted});
    expect_deleted(family, {1, 2, 0}, {kDeleted, kDeleted, kDeleted});
}

std::string prefixed(const Case &family, Index index, const std::string &reason)
{
    const std::string noun(biyos::noun_of(family.family));
    return "invalid argument: " + noun + " " + std::to_string(index) + ": " + reason;
}

std::string repeated(const Case &family, Index index)
{
    const std::string noun(biyos::noun_of(family.family));
    return "invalid argument: argument 'idx': " + noun + " " + std::to_string(index) +
           " is repeated";
}

std::string refused(const Case &family, const Indices &indices)
{
    Model model = built(three());
    const Model before = model;
    const std::string message = refusal([&] { (model.*family.call)(indices); });
    EXPECT_EQ(biyos::difference(model, before), "");
    expect_lookups(model, before, family);
    return message;
}

void expect_refused(const Case &family)
{
    EXPECT_EQ(refused(family, {0, 3}), prefixed(family, 3, "unknown"));
    EXPECT_EQ(refused(family, {-1}), prefixed(family, -1, "unknown"));
    EXPECT_EQ(refused(family, {1, 0, 1}), repeated(family, 1));
    EXPECT_EQ(refused(family, {2, 2, 3}), prefixed(family, 3, "unknown"));
}

void expect_empty(const Case &family)
{
    Model model = built(three());
    const Model before = model;
    EXPECT_EQ((model.*family.call)(Indices{}), (Map{0, 1, 2}));
    EXPECT_EQ(biyos::difference(model, before), "");
    expect_lookups(model, before, family);
}

void set_tolerances(Model &model, const Indices &indices)
{
    const std::vector<double> absolute{1e-6, 1e-7};
    const std::vector<double> relative{1e-3, 1e-4};
    const std::size_t n = indices.size();
    model.set_complement_tolerances(indices,
                                    std::span(absolute).first(n),
                                    std::span(relative).first(n));
}

}

TEST(DeleteTest, Constraints)
{
    expect_each(kConstraints);
}

TEST(DeleteTest, Objectives)
{
    expect_each(kObjectives);
}

TEST(DeleteTest, Indicators)
{
    expect_each(kIndicators);
}

TEST(DeleteTest, Sets)
{
    expect_each(kSos);
}

TEST(DeleteTest, Cones)
{
    expect_each(kCones);
}

TEST(DeleteTest, Complements)
{
    expect_each(kComplements);
}

TEST(DeleteTest, Refused)
{
    for (const Case &family : kCases)
    {
        SCOPED_TRACE(biyos::noun_of(family.family));
        expect_refused(family);
    }
}

TEST(DeleteTest, Empty)
{
    for (const Case &family : kCases)
    {
        SCOPED_TRACE(biyos::noun_of(family.family));
        expect_empty(family);
    }
}

TEST(DeleteTest, Tolerances)
{
    Model model = built(three());
    set_tolerances(model, {2, 0});
    EXPECT_EQ(model.delete_complements(Indices{1}), (Map{0, kDeleted, 1}));
    Parts parts = three();
    keep_complements(parts, {0, kDeleted, 1});
    Model wanted = built(parts);
    set_tolerances(wanted, {1, 0});
    EXPECT_EQ(biyos::difference(model, wanted), "");
    EXPECT_EQ(model.delete_complements(Indices{0}), (Map{kDeleted, 0}));
    keep_complements(parts, {kDeleted, 0});
    wanted = built(parts);
    set_tolerances(wanted, {0});
    EXPECT_EQ(biyos::difference(model, wanted), "");
}
