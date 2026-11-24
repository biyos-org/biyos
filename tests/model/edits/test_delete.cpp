#include "biyos/biyos.h"
#include "core/error.hpp"
#include "core/index.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "model/compare/compare.hpp"
#include "model/compressed.hpp"
#include "model/expressions/callbacks.hpp"
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

using biyos::Complement;
using biyos::ConeKind;
using biyos::Constraint;
using biyos::ErrorKind;
using biyos::Family;
using biyos::Graph;
using biyos::Index;
using biyos::Indicator;
using biyos::kDeleted;
using biyos::Member;
using biyos::Model;
using biyos::Objective;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::Operator;
using biyos::SpecialOrderedSetKind;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::built;
using biyos::test::Expressions;
using biyos::test::failure;
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
        .sense = ObjectiveSense::Minimize,
        .priority = 2,
        .weight = 0.5,
        .absolute = 0.0,
        .relative = 1e-3,
    });
    parts.indicators.push_back(
        {.variable = 1, .value = 1, .function = 0, .lower = 0.0, .upper = 2.0});
    parts.special_ordered_sets.kinds.push_back(SpecialOrderedSetKind::One);
    parts.special_ordered_sets.starts.push_back(7);
    parts.special_ordered_sets.members.insert(
        parts.special_ordered_sets.members.end(),
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
    parts.special_ordered_set_names = {.indices = {2, 1}, .names = {"t", "s"}};
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

void keep_special_ordered_sets(Parts &parts, const Map &map)
{
    parts.special_ordered_sets.kinds = kept(parts.special_ordered_sets.kinds, map);
    kept_parts(parts.special_ordered_sets.starts, parts.special_ordered_sets.members, map);
    parts.special_ordered_set_names = renamed(parts.special_ordered_set_names, map);
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

const Case kSpecialOrderedSets{
    .family = Family::SpecialOrderedSet,
    .call = &Model::delete_special_ordered_sets,
    .keep = keep_special_ordered_sets,
    .names = &Parts::special_ordered_set_names,
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

const std::array kCases{kConstraints,
                        kObjectives,
                        kIndicators,
                        kSpecialOrderedSets,
                        kCones,
                        kComplements};

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

constexpr std::array<Index, 3> kSpread{0, 2, 4};

template <typename Record> void spread(std::vector<Record> &records, Index Record::*field)
{
    for (Record &record : records)
    {
        record.*field = kSpread[static_cast<std::size_t>(record.*field)];
    }
}

void spread_records(Parts &parts)
{
    spread(parts.constraints, &Constraint::function);
    spread(parts.objectives, &Objective::function);
    spread(parts.indicators, &Indicator::function);
    spread(parts.complements, &Complement::function);
    spread(parts.complements, &Complement::multiplier);
    for (Index &function : parts.cones.functions)
    {
        function = kSpread[static_cast<std::size_t>(function)];
    }
}

Parts five()
{
    Parts parts = three();
    parts.constants = {1.5, 4.0, 0.0, -2.0, 0.1};
    parts.starts = {0, 2, 3, 4, 5, 5};
    parts.terms = {
        {.variable = 0, .coefficient = 2.0},
        {.variable = 2, .coefficient = -1.0},
        {.variable = 2, .coefficient = 5.0},
        {.variable = 1, .coefficient = 0.1},
        {.variable = 0, .coefficient = -3.0},
    };
    parts.quadratic.functions = {4, 0, 3};
    parts.quadratic.starts = {0, 1, 3, 4};
    parts.quadratic.terms.push_back({.row = 1, .column = 0, .coefficient = 4.0});
    parts.expressions.functions = {0, 4, 2, 1};
    parts.expressions.roots.push_back(2);
    parts.function_names = {.indices = {2, 1, 3}, .names = {"f", "u", "v"}};
    spread_records(parts);
    return parts;
}

void expect_unused(const Indices &indices)
{
    SCOPED_TRACE(testing::PrintToString(indices));
    Model model = built(five());
    const Offset nodes = model.graph().size();
    EXPECT_EQ(model.delete_functions(indices), (Map{0, kDeleted, 1, kDeleted, 2}));
    const Model wanted = built(three());
    EXPECT_EQ(biyos::difference(model, wanted), "");
    EXPECT_EQ(model.graph().size(), nodes);
    for (const std::string_view name : five().function_names.names)
    {
        SCOPED_TRACE(name);
        EXPECT_EQ(lookup(model, Family::Function, name), lookup(wanted, Family::Function, name));
    }
}

Model covered()
{
    Model model;
    model.add_functions(std::array{0.0, 1.0, 2.0, 3.0}, std::array<Offset, 5>{}, {});
    model.add_callback(std::array<Index, 1>{2}, nullptr);
    model.add_callback(std::array<Index, 2>{1, 2}, nullptr);
    model.add_callback(std::array<Index, 2>{3, 1}, nullptr);
    return model;
}

struct Use
{
    void (*add)(Model &, Index);
    std::string_view user;
};

void add_constraint(Model &model, Index function)
{
    model.add_constraints(std::array{Constraint{.function = function, .lower = 0.0, .upper = 1.0}});
}

void add_objective(Model &model, Index function)
{
    model.add_objectives(std::array{biyos::default_objective(function, 0)});
}

void add_indicator(Model &model, Index function)
{
    const Indicator indicator{
        .variable = 0,
        .value = 1,
        .function = function,
        .lower = 0.0,
        .upper = 1.0,
    };
    model.add_indicators(std::array{indicator});
}

void add_cone(Model &model, Index function)
{
    const std::array<Offset, 2> starts{0, 2};
    const std::array<Index, 2> functions{3, function};
    const std::array<Offset, 2> parameter_starts{0, 0};
    model.add_cones(std::array{ConeKind::Nonnegative}, starts, functions, parameter_starts, {});
}

void add_complement(Model &model, Index function)
{
    const Complement complement{.function = function, .lower = 0.0, .upper = 1.0, .multiplier = 3};
    model.add_complements(std::array{complement});
}

void add_multiplier(Model &model, Index function)
{
    const Complement complement{.function = 3, .lower = 0.0, .upper = 1.0, .multiplier = function};
    model.add_complements(std::array{complement});
}

void add_callback(Model &model, Index function)
{
    model.add_callback(std::array{function}, nullptr);
}

const std::array kUses{
    Use{.add = add_constraint, .user = "constraint"},
    Use{.add = add_objective, .user = "objective"},
    Use{.add = add_indicator, .user = "indicator"},
    Use{.add = add_cone, .user = "cone"},
    Use{.add = add_complement, .user = "complement"},
    Use{.add = add_multiplier, .user = "complement"},
    Use{.add = add_callback, .user = "callback"},
};

Model used(const Use &use)
{
    Model model;
    model.add_variables(std::array{biyos::widest(biyos::VariableKind::Binary)});
    model.add_functions(std::array{0.0, 1.0, 2.0, 3.0}, std::array<Offset, 5>{}, {});
    use.add(model, 0);
    use.add(model, 2);
    return model;
}

std::string in_use(Model &model, const Indices &indices)
{
    const Model before = model;
    const std::string message = failure([&] { model.delete_functions(indices); }, ErrorKind::InUse);
    EXPECT_EQ(biyos::difference(model, before), "");
    return message;
}

void expect_used(const Use &use)
{
    SCOPED_TRACE(use.user);
    Model model = used(use);
    const std::string wanted = "in use: function 2: used by " + std::string(use.user) + " 1";
    EXPECT_EQ(in_use(model, {2}), wanted);
    EXPECT_EQ(in_use(model, {1, 2}), wanted);
}

void set_tolerances(Model &model, Family family, const Indices &indices)
{
    const std::vector<double> absolute{1e-6, 1e-7};
    const std::vector<double> relative{1e-3, 1e-4};
    const std::size_t n = indices.size();
    model.set_tolerances(family,
                         indices,
                         std::span(absolute).first(n),
                         std::span(relative).first(n));
}

void expect_tolerances(const Case &family)
{
    SCOPED_TRACE(biyos::noun_of(family.family));
    Model model = built(three());
    set_tolerances(model, family.family, {2, 0});
    EXPECT_EQ((model.*family.call)(Indices{1}), (Map{0, kDeleted, 1}));
    Parts parts = three();
    family.keep(parts, {0, kDeleted, 1});
    Model wanted = built(parts);
    set_tolerances(wanted, family.family, {1, 0});
    EXPECT_EQ(biyos::difference(model, wanted), "");
    EXPECT_EQ((model.*family.call)(Indices{0}), (Map{kDeleted, 0}));
    family.keep(parts, {kDeleted, 0});
    wanted = built(parts);
    set_tolerances(wanted, family.family, {0});
    EXPECT_EQ(biyos::difference(model, wanted), "");
}

const Map kDropped{0, kDeleted, 1, kDeleted, 2};

Expressions product(Index first, Index second)
{
    return {
        .nodes =
            {
                {.code = BIYOS_OP_VAR, .variable = first},
                {.code = BIYOS_OP_VAR, .variable = second},
                {.code = BIYOS_OP_EXP, .children = {0}},
                {.code = BIYOS_OP_PROD, .children = {2, 1}, .values = {0.5}},
            },
        .functions = {2},
        .roots = {3},
    };
}

Parts spaced()
{
    Parts parts;
    parts.variables = {
        {.kind = VariableKind::Continuous, .lower = -1.0, .upper = 1.0},
        {.kind = VariableKind::Integer, .lower = 0.0, .upper = 4.0},
        {.kind = VariableKind::Binary, .lower = 0.0, .upper = 1.0},
        {.kind = VariableKind::Continuous, .lower = -kInfinity, .upper = 0.0},
        {.kind = VariableKind::Integer, .lower = -2.0, .upper = 2.0},
    };
    parts.constants = {1.0, -2.0, 0.5};
    parts.starts = {0, 4, 5, 5};
    parts.terms = {
        {.variable = 0, .coefficient = 1.0},
        {.variable = 1, .coefficient = 2.0},
        {.variable = 3, .coefficient = 3.0},
        {.variable = 4, .coefficient = 4.0},
        {.variable = 1, .coefficient = 5.0},
    };
    parts.quadratic = {
        .functions = {0},
        .starts = {0, 5},
        .terms =
            {
                {.row = 0, .column = 1, .coefficient = 1.0},
                {.row = 0, .column = 4, .coefficient = 6.0},
                {.row = 1, .column = 3, .coefficient = 2.0},
                {.row = 2, .column = 4, .coefficient = 5.0},
                {.row = 3, .column = 3, .coefficient = 3.0},
            },
    };
    parts.expressions = product(4, 2);
    parts.constraints = {{.function = 1, .lower = -1.0, .upper = 1.0}};
    parts.indicators = {
        {.variable = 2, .value = 1, .function = 0, .lower = -kInfinity, .upper = 1.0},
    };
    parts.special_ordered_sets = {
        .kinds = {SpecialOrderedSetKind::One},
        .starts = {0, 3},
        .members =
            {
                {.variable = 4, .weight = 1.0},
                {.variable = 0, .weight = 2.0},
                {.variable = 2, .weight = 3.0},
            },
    };
    parts.cones.starts = {0};
    parts.cones.parameter_starts = {0};
    parts.variable_names = {.indices = {1, 4, 0}, .names = {"y", "w", "x"}};
    return parts;
}

Parts compacted()
{
    Parts parts = spaced();
    parts.variables = kept(parts.variables, kDropped);
    parts.starts = {0, 2, 2, 2};
    parts.terms = {{.variable = 0, .coefficient = 1.0}, {.variable = 2, .coefficient = 4.0}};
    parts.quadratic.starts = {0, 2};
    parts.quadratic.terms = {
        {.row = 0, .column = 2, .coefficient = 6.0},
        {.row = 1, .column = 2, .coefficient = 5.0},
    };
    parts.expressions = product(2, 1);
    parts.indicators[0].variable = 1;
    parts.special_ordered_sets.members[0].variable = 2;
    parts.special_ordered_sets.members[2].variable = 1;
    parts.variable_names = renamed(parts.variable_names, kDropped);
    return parts;
}

std::vector<Index> payloads(const Graph &graph)
{
    std::vector<Index> found;
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (graph.op(node) == Operator::Variable)
        {
            found.push_back(graph.variable(node));
        }
    }
    return found;
}

std::string held_by(Model &model, const Indices &indices)
{
    const Model before = model;
    const std::string message = failure([&] { model.delete_variables(indices); }, ErrorKind::InUse);
    EXPECT_EQ(biyos::difference(model, before), "");
    EXPECT_EQ(payloads(model.graph()), payloads(before.graph()));
    return message;
}

void add_node(Model &model, Index variable)
{
    model.add_variable_node(variable);
}

void add_member(Model &model, Index variable)
{
    const std::array members{Member{.variable = variable, .weight = 1.0}};
    model.add_special_ordered_sets(std::array{SpecialOrderedSetKind::One},
                                   std::array<Offset, 2>{0, 1},
                                   members);
}

void add_binary(Model &model, Index variable)
{
    const Indicator indicator{
        .variable = variable,
        .value = 1,
        .function = 0,
        .lower = 0.0,
        .upper = 1.0,
    };
    model.add_indicators(std::array{indicator});
}

const std::array kHolders{
    Use{.add = add_node, .user = "node"},
    Use{.add = add_member, .user = "set"},
    Use{.add = add_binary, .user = "indicator"},
};

Model held(const Use &use)
{
    Model model;
    const Variable binary = biyos::widest(VariableKind::Binary);
    model.add_variables(std::array{binary, binary, binary});
    model.add_functions(std::array{0.0}, std::array<Offset, 2>{}, {});
    use.add(model, 0);
    use.add(model, 2);
    return model;
}

void expect_held(const Use &use)
{
    SCOPED_TRACE(use.user);
    Model model = held(use);
    const std::string wanted = "in use: variable 2: used by " + std::string(use.user) + " 1";
    EXPECT_EQ(held_by(model, {2}), wanted);
    EXPECT_EQ(held_by(model, {1, 2}), wanted);
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
    expect_each(kSpecialOrderedSets);
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
    expect_tolerances(kConstraints);
    expect_tolerances(kCones);
    expect_tolerances(kComplements);
}

TEST(DeleteTest, Functions)
{
    expect_unused({3, 1});
    expect_unused({1, 3});
}

TEST(DeleteTest, Callbacks)
{
    Model model = covered();
    EXPECT_EQ(model.delete_functions(Indices{0}), (Map{kDeleted, 0, 1, 2}));
    EXPECT_EQ(model.callback(0).covered, (Indices{1}));
    EXPECT_EQ(model.callback(1).covered, (Indices{0, 1}));
    EXPECT_EQ(model.callback(2).covered, (Indices{2, 0}));
}

TEST(DeleteTest, Covered)
{
    Model model = covered();
    EXPECT_EQ(in_use(model, {1}), "in use: function 1: used by callback 1");
    EXPECT_EQ(in_use(model, {2}), "in use: function 2: used by callback 0");
    EXPECT_EQ(in_use(model, {3, 0}), "in use: function 3: used by callback 2");
}

TEST(DeleteTest, Uses)
{
    for (const Use &use : kUses)
    {
        expect_used(use);
    }
}

TEST(DeleteTest, Checks)
{
    Model model = used(kUses.front());
    const Model before = model;
    EXPECT_EQ(refusal([&] { model.delete_functions(Indices{2, 4}); }),
              "invalid argument: function 4: unknown");
    EXPECT_EQ(refusal([&] { model.delete_functions(Indices{2, 2}); }),
              "invalid argument: argument 'idx': function 2 is repeated");
    EXPECT_EQ(biyos::difference(model, before), "");
}

TEST(DeleteTest, Variables)
{
    Model model = built(spaced());
    set_tolerances(model, Family::Variable, {4, 3});
    EXPECT_EQ(model.delete_variables(Indices{3, 1}), kDropped);
    Model wanted = built(compacted());
    set_tolerances(wanted, Family::Variable, {2});
    EXPECT_EQ(biyos::difference(model, wanted), "");
    for (const std::string_view name : spaced().variable_names.names)
    {
        SCOPED_TRACE(name);
        EXPECT_EQ(lookup(model, Family::Variable, name), lookup(wanted, Family::Variable, name));
    }
}

TEST(DeleteTest, VariableNodes)
{
    Model model = built(spaced());
    const Offset w = model.graph().node_of(4);
    const Offset z = model.graph().node_of(2);
    const Offset nodes = model.graph().size();
    model.delete_variables(Indices{3, 1});
    EXPECT_EQ(model.graph().size(), nodes);
    EXPECT_EQ(model.graph().variable(w), 2);
    EXPECT_EQ(model.graph().variable(z), 1);
    EXPECT_EQ(model.add_variable_node(2), w);
    EXPECT_EQ(model.add_variable_node(1), z);
    EXPECT_EQ(model.add_variable_node(0), nodes);
}

TEST(DeleteTest, VariableUses)
{
    for (const Use &use : kHolders)
    {
        expect_held(use);
    }
    Model model = built(spaced());
    EXPECT_EQ(held_by(model, {4}), "in use: variable 4: used by node 0");
    model.clear_functions(Indices{2});
    EXPECT_EQ(held_by(model, {4}), "in use: variable 4: used by node 0");
}

TEST(DeleteTest, SplitNode)
{
    Model model = built(spaced());
    const Offset x = model.add_variable_node(0);
    const std::vector<Offset> children{x};
    const Offset scaled = model.add_node(BIYOS_OP_SUM, children, std::vector<double>{1.0, 3.0});
    model.attach_expressions(std::vector<Index>{1}, std::vector<Offset>{scaled});
    ASSERT_EQ(model.linear_terms(1, 2).size(), 2U);
    EXPECT_EQ(model.linear_terms(1, 2).front().variable, 0);
    EXPECT_EQ(held_by(model, {0}), "in use: variable 0: used by node 4");
}

TEST(DeleteTest, VariableCallbacks)
{
    Model model = built(spaced());
    model.add_callback(std::array<Index, 1>{1}, nullptr);
    EXPECT_EQ(held_by(model, {3, 1}), "in use: variable 1: used by callback 0");
    EXPECT_EQ(held_by(model, {3}), "in use: variable 3: used by callback 0");
    const Model before = model;
    EXPECT_EQ(model.delete_variables(Indices{}), (Map{0, 1, 2, 3, 4}));
    EXPECT_EQ(biyos::difference(model, before), "");
}

TEST(DeleteTest, VariableChecks)
{
    Model model = built(spaced());
    const Model before = model;
    EXPECT_EQ(refusal([&] { model.delete_variables(Indices{3, 5}); }),
              "invalid argument: variable 5: unknown");
    EXPECT_EQ(refusal([&] { model.delete_variables(Indices{3, 3}); }),
              "invalid argument: argument 'idx': variable 3 is repeated");
    EXPECT_EQ(biyos::difference(model, before), "");
}
