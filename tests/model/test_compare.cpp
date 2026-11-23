#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/terms.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "sample.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <span>
#include <string>

namespace
{

using biyos::Complement;
using biyos::ConeKind;
using biyos::difference;
using biyos::Family;
using biyos::Index;
using biyos::Indicator;
using biyos::LinearTerm;
using biyos::Member;
using biyos::Model;
using biyos::Objective;
using biyos::QuadraticTerm;
using biyos::Sense;
using biyos::SosKind;
using biyos::VariableKind;
using biyos::test::built;
using biyos::test::Node;
using biyos::test::Parts;
using biyos::test::sample;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

std::string from_sample(const Parts &parts)
{
    return difference(built(sample()), built(parts));
}

Parts two_binaries()
{
    Parts parts = sample();
    parts.variables[0] = biyos::widest(VariableKind::Binary);
    return parts;
}

Parts with_copy(const Parts &parts)
{
    Parts more = parts;
    more.constants.push_back(more.constants[0]);
    more.starts.push_back(more.starts.back() + 2);
    more.terms.push_back(more.terms[0]);
    more.terms.push_back(more.terms[1]);
    return more;
}

}

TEST(CompareTest, Equal)
{
    const Model model = built(sample());
    EXPECT_EQ(difference(model, model), "");
    EXPECT_EQ(difference(model, built(sample())), "");
    EXPECT_EQ(difference(Model(), Model()), "");
}

TEST(CompareTest, VariableFields)
{
    Parts kind = sample();
    kind.variables[1].kind = VariableKind::Integer;
    kind.indicators.clear();
    kind.indicator_names = {};
    EXPECT_EQ(from_sample(kind), "variable 1: kind 2 against 3");
    Parts lower = sample();
    lower.variables[2].lower = -4.0;
    EXPECT_EQ(from_sample(lower), "variable 2: lower bound -inf against -4");
    Parts upper = sample();
    upper.variables[0].upper = kInfinity;
    EXPECT_EQ(from_sample(upper), "variable 0: upper bound 2.5 against inf");
}

TEST(CompareTest, FunctionFields)
{
    Parts constant = sample();
    constant.constants[2] = 0.2;
    EXPECT_EQ(from_sample(constant), "function 2: constant 0.1 against 0.2");
    Parts variable = sample();
    variable.terms[1].variable = 1;
    EXPECT_EQ(from_sample(variable), "function 0: term 1: variable 2 against 1");
    Parts coefficient = sample();
    coefficient.terms[2].coefficient = 0.2;
    EXPECT_EQ(from_sample(coefficient), "function 1: term 0: coefficient 0.1 against 0.2");
    Parts n = sample();
    n.starts = {0, 2, 2, 3};
    EXPECT_EQ(from_sample(n), "function 1: term count 1 against 0");
}

TEST(CompareTest, QuadraticFields)
{
    Parts first = sample();
    first.quadratic.terms[0].row = 0;
    EXPECT_EQ(from_sample(first), "function 2: quadratic term 0: first variable 1 against 0");
    Parts second = sample();
    second.quadratic.terms[1].row = 1;
    EXPECT_EQ(from_sample(second), "function 0: quadratic term 1: second variable 2 against 1");
    Parts coefficient = sample();
    coefficient.quadratic.terms[2].coefficient = 2.5;
    EXPECT_EQ(from_sample(coefficient), "function 0: quadratic term 0: coefficient 2 against 2.5");
    Parts n = sample();
    n.quadratic.starts = {0, 1, 2};
    n.quadratic.terms.pop_back();
    EXPECT_EQ(from_sample(n), "function 0: quadratic term count 2 against 1");
}

TEST(CompareTest, ConstraintFields)
{
    Parts function = sample();
    function.constraints[2].function = 1;
    EXPECT_EQ(from_sample(function), "constraint 2: function 0 against 1");
    Parts lower = sample();
    lower.constraints[0].lower = -kInfinity;
    EXPECT_EQ(from_sample(lower), "constraint 0: lower bound -1 against -inf");
    Parts upper = sample();
    upper.constraints[1].upper = 5.0;
    EXPECT_EQ(from_sample(upper), "constraint 1: upper bound inf against 5");
}

TEST(CompareTest, ObjectiveFields)
{
    Parts function = sample();
    function.objectives[1].function = 1;
    EXPECT_EQ(from_sample(function), "objective 1: function 0 against 1");
    Parts sense = sample();
    sense.objectives[0].sense = Sense::Minimize;
    EXPECT_EQ(from_sample(sense), "objective 0: sense 2 against 1");
    Parts priority = sample();
    priority.objectives[1].priority = -4;
    EXPECT_EQ(from_sample(priority), "objective 1: priority 0 against -4");
    Parts weight = sample();
    weight.objectives[0].weight = 3.0;
    EXPECT_EQ(from_sample(weight), "objective 0: weight 2 against 3");
    Parts absolute = sample();
    absolute.objectives[1].absolute = kInfinity;
    EXPECT_EQ(from_sample(absolute), "objective 1: absolute tolerance 0 against inf");
    Parts relative = sample();
    relative.objectives[1].relative = 0.25;
    EXPECT_EQ(from_sample(relative), "objective 1: relative tolerance inf against 0.25");
}

TEST(CompareTest, IndicatorFields)
{
    Parts variable = two_binaries();
    variable.indicators[0].variable = 0;
    EXPECT_EQ(difference(built(two_binaries()), built(variable)),
              "indicator 0: variable 1 against 0");
    Parts value = sample();
    value.indicators[1].value = 1;
    EXPECT_EQ(from_sample(value), "indicator 1: value 0 against 1");
    Parts function = sample();
    function.indicators[0].function = 1;
    EXPECT_EQ(from_sample(function), "indicator 0: function 2 against 1");
    Parts lower = sample();
    lower.indicators[0].lower = -1.0;
    EXPECT_EQ(from_sample(lower), "indicator 0: lower bound -inf against -1");
    Parts upper = sample();
    upper.indicators[1].upper = 2.0;
    EXPECT_EQ(from_sample(upper), "indicator 1: upper bound 1 against 2");
}

TEST(CompareTest, SosFields)
{
    Parts kind = sample();
    kind.sos.kinds[1] = SosKind::Sos2;
    EXPECT_EQ(from_sample(kind), "set 1: kind 1 against 2");
    Parts variable = sample();
    variable.sos.members[3].variable = 2;
    EXPECT_EQ(from_sample(variable), "set 1: member 0: variable 0 against 2");
    Parts weight = sample();
    weight.sos.members[2].weight = 2.0;
    EXPECT_EQ(from_sample(weight), "set 0: member 2: weight 2.5 against 2");
    Parts n = sample();
    n.sos.members.pop_back();
    n.sos.starts[2] = 4;
    EXPECT_EQ(from_sample(n), "set 1: member count 2 against 1");
}

TEST(CompareTest, ConeFields)
{
    Parts kind = sample();
    kind.cones.kinds[0] = ConeKind::Rotated;
    EXPECT_EQ(from_sample(kind), "cone 0: kind 2 against 3");
    Parts member = sample();
    member.cones.functions[1] = 1;
    EXPECT_EQ(from_sample(member), "cone 0: member 1: function 0 against 1");
    Parts members = sample();
    members.cones.starts[2] = 4;
    members.cones.functions.pop_back();
    EXPECT_EQ(from_sample(members), "cone 1: member count 3 against 2");
    Parts value = sample();
    value.cones.parameters[0] = 0.5;
    EXPECT_EQ(from_sample(value), "cone 1: parameter 0: value 0.25 against 0.5");
    Parts parameters = sample();
    parameters.cones.parameter_starts[2] = 1;
    parameters.cones.parameters.pop_back();
    EXPECT_EQ(from_sample(parameters), "cone 1: parameter count 2 against 1");
    Parts both = sample();
    both.cones.kinds[1] = ConeKind::DualPower;
    both.cones.functions[2] = 1;
    both.cones.functions[3] = 0;
    EXPECT_EQ(from_sample(both), "cone 1: kind 6 against 7");
}

TEST(CompareTest, ComplementFields)
{
    Parts function = sample();
    function.complements[0].function = 0;
    EXPECT_EQ(from_sample(function), "complement 0: function 1 against 0");
    Parts multiplier = sample();
    multiplier.complements[1].multiplier = 2;
    EXPECT_EQ(from_sample(multiplier), "complement 1: multiplier 0 against 2");
    Parts lower = sample();
    lower.complements[0].lower = -1.0;
    EXPECT_EQ(from_sample(lower), "complement 0: lower bound 0 against -1");
    Parts upper = sample();
    upper.complements[1].upper = 3.0;
    EXPECT_EQ(from_sample(upper), "complement 1: upper bound 2 against 3");
}

TEST(CompareTest, Counts)
{
    Parts variables = sample();
    variables.variables.push_back(variables.variables[0]);
    EXPECT_EQ(from_sample(variables), "variable count 3 against 4");
    EXPECT_EQ(from_sample(with_copy(sample())), "function count 3 against 4");
    Parts constraints = sample();
    constraints.constraints.pop_back();
    EXPECT_EQ(from_sample(constraints), "constraint count 3 against 2");
    Parts objectives = sample();
    objectives.objectives.pop_back();
    EXPECT_EQ(from_sample(objectives), "objective count 2 against 1");
    Parts indicators = sample();
    indicators.indicators.pop_back();
    indicators.indicator_names = {};
    EXPECT_EQ(from_sample(indicators), "indicator count 2 against 1");
    Parts sos = sample();
    sos.sos.kinds.pop_back();
    sos.sos.starts.pop_back();
    sos.sos.members.resize(3);
    sos.sos_names = {.indices = {}, .names = {}};
    EXPECT_EQ(from_sample(sos), "set count 2 against 1");
    Parts cones = sample();
    cones.cones.kinds.pop_back();
    cones.cones.starts.pop_back();
    cones.cones.parameter_starts.pop_back();
    cones.cones.functions.resize(2);
    cones.cones.parameters.clear();
    cones.cone_names = {};
    EXPECT_EQ(from_sample(cones), "cone count 2 against 1");
    Parts complements = sample();
    complements.complements.pop_back();
    complements.complement_names = {};
    EXPECT_EQ(from_sample(complements), "complement count 2 against 1");
}

TEST(CompareTest, Names)
{
    Parts renamed = sample();
    renamed.name = "other";
    EXPECT_EQ(from_sample(renamed), "model name 'sample' against 'other'");
    Parts unnamed = sample();
    unnamed.name.clear();
    EXPECT_EQ(from_sample(unnamed), "model name 'sample' against none");
    EXPECT_EQ(difference(built(unnamed), built(sample())), "model name none against 'sample'");
    Parts variable = sample();
    variable.variable_names.names[0] = "y";
    EXPECT_EQ(from_sample(variable), "variable 2: name 'z' against 'y'");
    Parts more = sample();
    more.variable_names = {.indices = {2, 1, 0}, .names = {"z", "y", "x"}};
    EXPECT_EQ(from_sample(more), "variable 1: name none against 'y'");
    Parts fewer = sample();
    fewer.variable_names = {.indices = {2}, .names = {"z"}};
    EXPECT_EQ(from_sample(fewer), "variable 0: name 'x' against none");
    Parts function = sample();
    function.function_names.names[0] = "g";
    EXPECT_EQ(from_sample(function), "function 1: name 'f' against 'g'");
    Parts first = sample();
    first.function_names = {.indices = {0, 1}, .names = {"e", "f"}};
    EXPECT_EQ(from_sample(first), "function 0: name none against 'e'");
    Parts constraint = sample();
    constraint.constraint_names.names[0] = "d";
    EXPECT_EQ(from_sample(constraint), "constraint 1: name 'c' against 'd'");
    Parts row = sample();
    row.constraint_names = {.indices = {0, 1}, .names = {"b", "c"}};
    EXPECT_EQ(from_sample(row), "constraint 0: name none against 'b'");
    Parts objective = sample();
    objective.objective_names = {.indices = {1}, .names = {"cost"}};
    EXPECT_EQ(from_sample(objective), "objective 0: name 'cost' against none");
    Parts indicator = sample();
    indicator.indicator_names = {.indices = {0, 1}, .names = {"on", "off"}};
    EXPECT_EQ(from_sample(indicator), "indicator 0: name none against 'on'");
    Parts sos = sample();
    sos.sos_names = {.indices = {0}, .names = {"t"}};
    EXPECT_EQ(from_sample(sos), "set 0: name none against 't'");
    Parts cone = sample();
    cone.cone_names.names[0] = "j";
    EXPECT_EQ(from_sample(cone), "cone 1: name 'k' against 'j'");
    Parts complement = sample();
    complement.complement_names = {.indices = {0, 1}, .names = {"ab", "pair"}};
    EXPECT_EQ(from_sample(complement), "complement 0: name none against 'ab'");
}

TEST(CompareTest, OneUlp)
{
    Parts bound = sample();
    bound.constraints[2].upper = std::nextafter(3.0, kInfinity);
    EXPECT_EQ(from_sample(bound), "constraint 2: upper bound 3 against 3.0000000000000004");
    Parts coefficient = sample();
    coefficient.terms[0].coefficient = std::nextafter(2.0, 0.0);
    EXPECT_EQ(from_sample(coefficient),
              "function 0: term 0: coefficient 2 against 1.9999999999999998");
    Parts quadratic = sample();
    quadratic.quadratic.terms[0].coefficient = std::nextafter(0.5, kInfinity);
    EXPECT_EQ(from_sample(quadratic),
              "function 2: quadratic term 0: coefficient 0.5 against 0.5000000000000001");
    Parts weight = sample();
    weight.objectives[0].weight = std::nextafter(2.0, kInfinity);
    EXPECT_EQ(from_sample(weight), "objective 0: weight 2 against 2.0000000000000004");
    Parts parameter = sample();
    parameter.cones.parameters[1] = std::nextafter(0.75, kInfinity);
    EXPECT_EQ(from_sample(parameter), "cone 1: parameter 1: value 0.75 against 0.7500000000000001");
}

TEST(CompareTest, SignedZero)
{
    Parts bound = sample();
    bound.variables[0].lower = 0.0;
    EXPECT_EQ(from_sample(bound), "variable 0: lower bound -0 against 0");
    Parts constant = sample();
    constant.constants[1] = -0.0;
    EXPECT_EQ(from_sample(constant), "function 1: constant 0 against -0");
    Parts positive = sample();
    positive.quadratic.terms[0].coefficient = 0.0;
    Parts negative = sample();
    negative.quadratic.terms[0].coefficient = -0.0;
    EXPECT_EQ(difference(built(positive), built(negative)),
              "function 2: quadratic term 0: coefficient 0 against -0");
    Parts tolerance = sample();
    tolerance.objectives[1].absolute = -0.0;
    EXPECT_EQ(from_sample(tolerance), "objective 1: absolute tolerance 0 against -0");
    Parts positive_weight = sample();
    positive_weight.sos.members[4].weight = 0.0;
    Parts negative_weight = sample();
    negative_weight.sos.members[4].weight = -0.0;
    EXPECT_EQ(difference(built(positive_weight), built(negative_weight)),
              "set 1: member 1: weight 0 against -0");
}

TEST(CompareTest, SharedFunction)
{
    const Parts shared = with_copy(sample());
    Parts separate = shared;
    separate.constraints[2].function = 3;
    EXPECT_EQ(difference(built(shared), built(separate)), "constraint 2: function 0 against 3");
    EXPECT_EQ(difference(built(sample()), built(separate)), "function count 3 against 4");
}

TEST(CompareTest, Order)
{
    Parts tables = sample();
    tables.variables[2].upper = 4.0;
    tables.constraints[0].lower = -2.0;
    EXPECT_EQ(from_sample(tables), "variable 2: upper bound 3 against 4");
    Parts n = sample();
    n.variables[0].upper = 1.0;
    n.variables.push_back(n.variables[0]);
    EXPECT_EQ(from_sample(n), "variable count 3 against 4");
    Parts function = sample();
    function.constants[0] = 2.0;
    function.terms[0] = LinearTerm{.variable = 1, .coefficient = 2.0};
    EXPECT_EQ(from_sample(function), "function 0: constant 1.5 against 2");
    EXPECT_EQ(difference(built(function), built(sample())), "function 0: constant 2 against 1.5");
    Parts linear = sample();
    linear.terms[1].coefficient = -2.0;
    linear.quadratic.terms[2].coefficient = 2.5;
    EXPECT_EQ(from_sample(linear), "function 0: term 1: coefficient -1 against -2");
    Parts quadratic = sample();
    quadratic.constants[2] = 0.2;
    quadratic.quadratic.terms[0].coefficient = 1.0;
    EXPECT_EQ(from_sample(quadratic), "function 2: constant 0.1 against 0.2");
    Parts objective = sample();
    objective.constraints[2].upper = 4.0;
    objective.objectives[0].weight = 3.0;
    EXPECT_EQ(from_sample(objective), "constraint 2: upper bound 3 against 4");
    Parts cone = sample();
    cone.objectives[1].weight = 3.0;
    cone.cones.kinds[0] = ConeKind::Nonnegative;
    EXPECT_EQ(from_sample(cone), "objective 1: weight 1 against 3");
    Parts first = sample();
    first.name = "other";
    first.variables.push_back(first.variables[0]);
    EXPECT_EQ(from_sample(first), "model name 'sample' against 'other'");
    Parts later = sample();
    later.variable_names.names[1] = "y";
    later.constants[0] = 2.0;
    EXPECT_EQ(from_sample(later), "variable 0: name 'x' against 'y'");
    Parts last = sample();
    last.objectives[0].weight = 3.0;
    last.indicators[0].upper = 5.0;
    last.complements[0].upper = 5.0;
    EXPECT_EQ(from_sample(last), "objective 0: weight 2 against 3");
}

TEST(CompareTest, FieldOrder)
{
    Parts kind = sample();
    kind.variables[1].kind = VariableKind::Integer;
    kind.indicators.clear();
    kind.indicator_names = {};
    kind.variables[1].lower = 0.5;
    EXPECT_EQ(from_sample(kind), "variable 1: kind 2 against 3");
    Parts bounds = sample();
    bounds.variables[0].lower = -1.0;
    bounds.variables[0].upper = 3.0;
    EXPECT_EQ(from_sample(bounds), "variable 0: lower bound -0 against -1");
    Parts term = sample();
    term.terms[0] = LinearTerm{.variable = 1, .coefficient = 3.0};
    EXPECT_EQ(from_sample(term), "function 0: term 0: variable 0 against 1");
    Parts function = sample();
    function.constraints[0].function = 1;
    function.constraints[0].lower = -2.0;
    EXPECT_EQ(from_sample(function), "constraint 0: function 0 against 1");
    Parts range = sample();
    range.constraints[1].lower = -1.0;
    range.constraints[1].upper = 5.0;
    EXPECT_EQ(from_sample(range), "constraint 1: lower bound 0 against -1");
    Parts pair = sample();
    pair.quadratic.terms[1] = QuadraticTerm{.row = 1, .column = 2, .coefficient = 3.0};
    EXPECT_EQ(from_sample(pair), "function 0: quadratic term 1: first variable 0 against 1");
    Parts later = sample();
    later.quadratic.terms[1] = QuadraticTerm{.row = 1, .column = 0, .coefficient = 3.0};
    EXPECT_EQ(from_sample(later), "function 0: quadratic term 1: second variable 2 against 1");
    Parts both = sample();
    both.quadratic.terms[1] = QuadraticTerm{.row = 1, .column = 1, .coefficient = -1.5};
    EXPECT_EQ(from_sample(both), "function 0: quadratic term 1: first variable 0 against 1");
    Parts named = sample();
    named.variables[2].upper = 4.0;
    named.variable_names.names[0] = "y";
    EXPECT_EQ(from_sample(named), "variable 2: upper bound 3 against 4");
    Parts terms = sample();
    terms.quadratic.terms[2].coefficient = 2.5;
    terms.function_names = {.indices = {1, 0}, .names = {"f", "e"}};
    EXPECT_EQ(from_sample(terms), "function 0: quadratic term 0: coefficient 2 against 2.5");
    Parts row = sample();
    row.constraints[1].upper = 5.0;
    row.constraint_names.names[0] = "d";
    EXPECT_EQ(from_sample(row), "constraint 1: upper bound inf against 5");
    Parts goal = sample();
    goal.objectives[0].relative = 1.0;
    goal.objective_names.names[0] = "profit";
    EXPECT_EQ(from_sample(goal), "objective 0: relative tolerance 0 against 1");
}

TEST(CompareTest, ObjectiveOrder)
{
    Parts changed = sample();
    Objective &objective = changed.objectives[0];
    objective = Objective{
        .function = 1,
        .sense = Sense::Minimize,
        .priority = 5,
        .weight = 3.0,
        .absolute = 1.0,
        .relative = 1.0,
    };
    EXPECT_EQ(from_sample(changed), "objective 0: function 2 against 1");
    objective.function = 2;
    EXPECT_EQ(from_sample(changed), "objective 0: sense 2 against 1");
    objective.sense = Sense::Maximize;
    EXPECT_EQ(from_sample(changed), "objective 0: priority 1 against 5");
    objective.priority = 1;
    EXPECT_EQ(from_sample(changed), "objective 0: weight 2 against 3");
    objective.weight = 2.0;
    EXPECT_EQ(from_sample(changed), "objective 0: absolute tolerance 0.5 against 1");
    objective.absolute = 0.5;
    EXPECT_EQ(from_sample(changed), "objective 0: relative tolerance 0 against 1");
    objective.relative = 0.0;
    EXPECT_EQ(from_sample(changed), "");
}

TEST(CompareTest, IndicatorOrder)
{
    Parts changed = two_binaries();
    Indicator &indicator = changed.indicators[1];
    indicator = Indicator{
        .variable = 0,
        .value = 1,
        .function = 0,
        .lower = 0.0,
        .upper = 3.0,
    };
    changed.indicator_names = {.indices = {1}, .names = {"on"}};
    const Model base = built(two_binaries());
    EXPECT_EQ(difference(base, built(changed)), "indicator 1: variable 1 against 0");
    indicator.variable = 1;
    EXPECT_EQ(difference(base, built(changed)), "indicator 1: value 0 against 1");
    indicator.value = 0;
    EXPECT_EQ(difference(base, built(changed)), "indicator 1: function 2 against 0");
    indicator.function = 2;
    EXPECT_EQ(difference(base, built(changed)), "indicator 1: lower bound 1 against 0");
    indicator.lower = 1.0;
    EXPECT_EQ(difference(base, built(changed)), "indicator 1: upper bound 1 against 3");
    indicator.upper = 1.0;
    EXPECT_EQ(difference(base, built(changed)), "indicator 1: name 'off' against 'on'");
    changed.indicator_names.names[0] = "off";
    EXPECT_EQ(difference(base, built(changed)), "");
}

TEST(CompareTest, SosOrder)
{
    Parts changed = sample();
    changed.objectives[1].priority = 1;
    changed.sos.kinds[1] = SosKind::Sos2;
    EXPECT_EQ(from_sample(changed), "objective 1: priority 0 against 1");
    changed.objectives[1].priority = 0;
    changed.sos.members[3] = Member{.variable = 2, .weight = -2.0};
    changed.sos_names.names[0] = "t";
    EXPECT_EQ(from_sample(changed), "set 1: kind 1 against 2");
    changed.sos.kinds[1] = SosKind::Sos1;
    EXPECT_EQ(from_sample(changed), "set 1: member 0: variable 0 against 2");
    changed.sos.members[3].variable = 0;
    EXPECT_EQ(from_sample(changed), "set 1: member 0: weight -1 against -2");
    changed.sos.members[3].weight = -1.0;
    EXPECT_EQ(from_sample(changed), "set 1: name 's' against 't'");
    changed.sos_names.names[0] = "s";
    EXPECT_EQ(from_sample(changed), "");
}

TEST(CompareTest, ComplementOrder)
{
    Parts changed = sample();
    Complement &complement = changed.complements[1];
    complement = Complement{.function = 2, .lower = 0.0, .upper = 1.0, .multiplier = 1};
    changed.complement_names = {.indices = {1}, .names = {"other"}};
    EXPECT_EQ(from_sample(changed), "complement 1: function 0 against 2");
    complement.function = 0;
    EXPECT_EQ(from_sample(changed), "complement 1: lower bound -2 against 0");
    complement.lower = -2.0;
    EXPECT_EQ(from_sample(changed), "complement 1: upper bound 2 against 1");
    complement.upper = 2.0;
    EXPECT_EQ(from_sample(changed), "complement 1: multiplier 0 against 1");
    complement.multiplier = 0;
    EXPECT_EQ(from_sample(changed), "complement 1: name 'pair' against 'other'");
    changed.complement_names.names[0] = "pair";
    EXPECT_EQ(from_sample(changed), "");
}

TEST(CompareTest, ComplementTolerance)
{
    Parts renamed = sample();
    renamed.complement_names = {.indices = {1}, .names = {"other"}};
    Model changed = built(renamed);
    const Index index = 1;
    const double relative = 0.5;
    changed.set_tolerances(Family::Complement, std::span(&index, 1), {}, std::span(&relative, 1));
    const Model base = built(sample());
    EXPECT_EQ(difference(base, changed), "complement 1: relative tolerance nan against 0.5");
    Parts bounded = renamed;
    bounded.complements[1].upper = 3.0;
    Model both = built(bounded);
    both.set_tolerances(Family::Complement, std::span(&index, 1), {}, std::span(&relative, 1));
    EXPECT_EQ(difference(base, both), "complement 1: upper bound 2 against 3");
}

TEST(CompareTest, VariableTolerance)
{
    Parts renamed = sample();
    renamed.variable_names.names[0] = "w";
    Model changed = built(renamed);
    const Index index = 2;
    const double relative = 0.5;
    changed.set_tolerances(Family::Variable, std::span(&index, 1), {}, std::span(&relative, 1));
    const Model base = built(sample());
    EXPECT_EQ(difference(base, changed), "variable 2: relative tolerance nan against 0.5");
    Parts bounded = renamed;
    bounded.variables[2].upper = 4.0;
    Model both = built(bounded);
    both.set_tolerances(Family::Variable, std::span(&index, 1), {}, std::span(&relative, 1));
    EXPECT_EQ(difference(base, both), "variable 2: upper bound 3 against 4");
}

TEST(CompareTest, ConstraintTolerance)
{
    Parts renamed = sample();
    renamed.constraint_names.names[0] = "d";
    Model changed = built(renamed);
    const Index index = 1;
    const double relative = 0.5;
    changed.set_tolerances(Family::Constraint, std::span(&index, 1), {}, std::span(&relative, 1));
    const Model base = built(sample());
    EXPECT_EQ(difference(base, changed), "constraint 1: relative tolerance nan against 0.5");
    Parts bounded = renamed;
    bounded.constraints[1].upper = 5.0;
    Model both = built(bounded);
    both.set_tolerances(Family::Constraint, std::span(&index, 1), {}, std::span(&relative, 1));
    EXPECT_EQ(difference(base, both), "constraint 1: upper bound inf against 5");
}

TEST(CompareTest, ConeTolerance)
{
    Parts renamed = sample();
    renamed.cone_names.names[0] = "j";
    Model changed = built(renamed);
    const Index index = 1;
    const double relative = 0.5;
    changed.set_tolerances(Family::Cone, std::span(&index, 1), {}, std::span(&relative, 1));
    const Model base = built(sample());
    EXPECT_EQ(difference(base, changed), "cone 1: relative tolerance nan against 0.5");
    Parts valued = renamed;
    valued.cones.parameters[0] = 0.5;
    Model both = built(valued);
    both.set_tolerances(Family::Cone, std::span(&index, 1), {}, std::span(&relative, 1));
    EXPECT_EQ(difference(base, both), "cone 1: parameter 0: value 0.25 against 0.5");
}

TEST(CompareTest, ExpressionFields)
{
    Parts op = sample();
    op.expressions.nodes[2].code = BIYOS_OP_LOG;
    EXPECT_EQ(from_sample(op), "function 0: node 2 against 2: operator 9 against 10");
    Parts coefficient = sample();
    coefficient.expressions.nodes[3].values = {0.25};
    EXPECT_EQ(from_sample(coefficient), "function 0: node 3 against 3: constant 0.5 against 0.25");
    Parts variable = sample();
    variable.expressions.nodes[1].variable = 1;
    EXPECT_EQ(from_sample(variable), "function 0: node 1 against 1: variable 2 against 1");
    Parts weight = sample();
    weight.expressions.nodes[4].values = {0.0, 2.0, 4.0};
    EXPECT_EQ(from_sample(weight), "function 0: node 4 against 4: child 1: weight 3 against 4");
    Parts first = sample();
    first.expressions.nodes[4].values = {0.0, -0.0, 3.0};
    EXPECT_EQ(from_sample(first), "function 0: node 4 against 4: child 0: weight 2 against -0");
    Parts exponent = sample();
    exponent.expressions.nodes[5].values = {2.5};
    EXPECT_EQ(from_sample(exponent), "function 1: node 5 against 5: constant 1.5 against 2.5");
    Parts count = sample();
    count.expressions.nodes[3].children = {2, 1, 1};
    EXPECT_EQ(from_sample(count), "function 0: node 3 against 3: child count 2 against 3");
    Parts root = sample();
    root.expressions.functions = {0, 1};
    root.expressions.roots = {4, 5};
    EXPECT_EQ(from_sample(root), "function 2: root node 3 against none");
    EXPECT_EQ(difference(built(root), built(sample())), "function 2: root none against node 3");
}

TEST(CompareTest, SumConstant)
{
    Parts a = sample();
    a.expressions.nodes.push_back(
        Node{.code = BIYOS_OP_SUM, .children = {2}, .values = {0.0, 1.0}});
    a.expressions.nodes.push_back(Node{.code = BIYOS_OP_SIN, .children = {6}});
    a.expressions.functions = {0};
    a.expressions.roots = {7};
    Parts b = a;
    b.expressions.nodes[6].values = {-0.0, 1.0};
    EXPECT_EQ(difference(built(a), built(b)),
              "function 0: node 6 against 6: constant 0 against -0");
}

TEST(CompareTest, SharedNodes)
{
    Parts separate = sample();
    std::vector<Node> &nodes = separate.expressions.nodes;
    nodes.insert(nodes.begin() + 3, nodes[2]);
    nodes[4].children = {3, 1};
    nodes[5].children = {2, 4};
    nodes[6].children = {5};
    separate.expressions.roots = {5, 4, 6};
    EXPECT_EQ(from_sample(separate), "function 0: node 2 against 3: shared as node 2 against 2");
    EXPECT_EQ(difference(built(separate), built(sample())),
              "function 0: node 3 against 2: shared as node 2 against 2");
}

TEST(CompareTest, Renumbered)
{
    Parts renumbered = sample();
    std::vector<Node> &nodes = renumbered.expressions.nodes;
    nodes[0].variable = 2;
    nodes[1].variable = 0;
    nodes[2].children = {1};
    nodes[3].children = {2, 0};
    EXPECT_EQ(from_sample(renumbered), "");
    Parts across = sample();
    across.expressions.functions = {1, 2, 0};
    across.expressions.roots = {5, 3, 4};
    EXPECT_EQ(from_sample(across), "");
}
