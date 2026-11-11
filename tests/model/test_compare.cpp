#include "expr/terms.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "sample.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

namespace
{

using biyos::difference;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Objective;
using biyos::QuadraticTerm;
using biyos::Sense;
using biyos::VariableKind;
using biyos::test::built;
using biyos::test::Parts;
using biyos::test::sample;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

std::string from_sample(const Parts &parts)
{
    return difference(built(sample()), built(parts));
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
    Parts count = sample();
    count.starts = {0, 2, 2, 3};
    EXPECT_EQ(from_sample(count), "function 1: term count 1 against 0");
}

TEST(CompareTest, QuadraticFields)
{
    Parts first = sample();
    first.quadratic.terms[0].first = 0;
    EXPECT_EQ(from_sample(first), "function 2: quadratic term 0: first variable 1 against 0");
    Parts second = sample();
    second.quadratic.terms[1].first = 1;
    EXPECT_EQ(from_sample(second), "function 0: quadratic term 1: second variable 2 against 1");
    Parts coefficient = sample();
    coefficient.quadratic.terms[2].coefficient = 2.5;
    EXPECT_EQ(from_sample(coefficient), "function 0: quadratic term 0: coefficient 2 against 2.5");
    Parts count = sample();
    count.quadratic.starts = {0, 1, 2};
    count.quadratic.terms.pop_back();
    EXPECT_EQ(from_sample(count), "function 0: quadratic term count 2 against 1");
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
    Parts count = sample();
    count.variables[0].upper = 1.0;
    count.variables.push_back(count.variables[0]);
    EXPECT_EQ(from_sample(count), "variable count 3 against 4");
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
}

TEST(CompareTest, FieldOrder)
{
    Parts kind = sample();
    kind.variables[1].kind = VariableKind::Integer;
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
    pair.quadratic.terms[1] = QuadraticTerm{.first = 1, .second = 2, .coefficient = 3.0};
    EXPECT_EQ(from_sample(pair), "function 0: quadratic term 1: first variable 0 against 1");
    Parts later = sample();
    later.quadratic.terms[1] = QuadraticTerm{.first = 1, .second = 0, .coefficient = 3.0};
    EXPECT_EQ(from_sample(later), "function 0: quadratic term 1: second variable 2 against 1");
    Parts both = sample();
    both.quadratic.terms[1] = QuadraticTerm{.first = 1, .second = 1, .coefficient = -1.5};
    EXPECT_EQ(from_sample(both), "function 0: quadratic term 1: first variable 0 against 1");
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
