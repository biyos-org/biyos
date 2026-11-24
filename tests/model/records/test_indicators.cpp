#include "core/error.hpp"
#include "core/index.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::Constraint;
using biyos::ErrorKind;
using biyos::Index;
using biyos::Indicator;
using biyos::Model;
using biyos::Offset;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::failure;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

Model with_parts(std::size_t n)
{
    Model model;
    const std::vector<Variable> variables{
        biyos::widest(VariableKind::Binary),
        biyos::widest(VariableKind::Continuous),
        biyos::widest(VariableKind::Binary),
    };
    model.add_variables(variables);
    const std::vector<double> constants(n, 0.0);
    const std::vector<Offset> starts(n + 1, 0);
    model.add_functions(constants, starts, {});
    return model;
}

Indicator when(Index variable, int value, Index function, double lower, double upper)
{
    return Indicator{
        .variable = variable,
        .value = value,
        .function = function,
        .lower = lower,
        .upper = upper,
    };
}

void add(Model &model, const Indicator &indicator)
{
    model.add_indicators(std::span(&indicator, 1));
}

std::string add_refusal(Model &model, const Indicator &indicator)
{
    return refusal([&] { add(model, indicator); });
}

std::string set_refusal(Model &model, Index index, const Indicator &indicator)
{
    return refusal([&] { model.set_indicators(std::span(&index, 1), std::span(&indicator, 1)); });
}

void set_kind(Model &model, Index index, VariableKind kind)
{
    const Variable changed = biyos::widest(kind);
    model.set_variables(std::span(&index, 1), std::span(&changed, 1));
}

std::string kind_failure(Model &model, Index index, VariableKind kind)
{
    return failure([&] { set_kind(model, index, kind); }, ErrorKind::InUse);
}

void expect_indicator(const Indicator &found, const Indicator &wanted)
{
    EXPECT_EQ(found.variable, wanted.variable);
    EXPECT_EQ(found.value, wanted.value);
    EXPECT_EQ(found.function, wanted.function);
    EXPECT_DOUBLE_EQ(found.lower, wanted.lower);
    EXPECT_DOUBLE_EQ(found.upper, wanted.upper);
}

}

TEST(IndicatorTest, BothValues)
{
    Model model = with_parts(1);
    const std::vector<Indicator> added{when(0, 1, 0, -kInfinity, 4.0), when(0, 0, 0, 1.0, 1.0)};
    model.add_indicators(added);
    ASSERT_EQ(model.n_indicators(), 2);
    expect_indicator(model.indicator(0), added[0]);
    expect_indicator(model.indicator(1), added[1]);
}

TEST(IndicatorTest, NotBinary)
{
    Model model = with_parts(1);
    const std::string refused = "invalid argument: indicator 0: ";
    EXPECT_EQ(add_refusal(model, when(1, 1, 0, 0.0, 1.0)), refused + "variable 1 is not binary");
    EXPECT_EQ(add_refusal(model, when(3, 1, 0, 0.0, 1.0)), refused + "variable 3 is unknown");
    EXPECT_EQ(add_refusal(model, when(-1, 1, 0, 0.0, 1.0)), refused + "variable -1 is unknown");
    EXPECT_EQ(add_refusal(model, when(0, 2, 0, 0.0, 1.0)), refused + "value 2 is neither 0 nor 1");
    EXPECT_EQ(add_refusal(model, when(0, -1, 0, 0.0, 1.0)),
              refused + "value -1 is neither 0 nor 1");
    EXPECT_EQ(model.n_indicators(), 0);
}

TEST(IndicatorTest, KindChange)
{
    Model model = with_parts(1);
    add(model, when(2, 1, 0, 0.0, 1.0));
    add(model, when(2, 0, 0, 0.0, 1.0));
    EXPECT_EQ(kind_failure(model, 2, VariableKind::Integer),
              "in use: variable 2: indicator 0 needs it binary");
    EXPECT_EQ(kind_failure(model, 2, VariableKind::Continuous),
              "in use: variable 2: indicator 0 needs it binary");
    EXPECT_EQ(model.variable(2).kind, VariableKind::Binary);
    const Variable fixed{.kind = VariableKind::Binary, .lower = 1.0, .upper = 1.0};
    const Index used = 2;
    model.set_variables(std::span(&used, 1), std::span(&fixed, 1));
    EXPECT_DOUBLE_EQ(model.variable(2).lower, 1.0);
    set_kind(model, 0, VariableKind::Integer);
    EXPECT_EQ(model.variable(0).kind, VariableKind::Integer);
}

TEST(IndicatorTest, KindList)
{
    Model model = with_parts(1);
    add(model, when(0, 1, 0, 0.0, 1.0));
    add(model, when(2, 1, 0, 0.0, 1.0));
    const std::vector<Index> indices{1, 2, 2};
    const std::vector<Variable> changed{
        biyos::widest(VariableKind::Integer),
        biyos::widest(VariableKind::Integer),
        biyos::widest(VariableKind::Binary),
    };
    const auto change = [&] { model.set_variables(indices, changed); };
    EXPECT_EQ(failure(change, ErrorKind::InUse), "in use: variable 2: indicator 1 needs it binary");
    EXPECT_EQ(model.variable(1).kind, VariableKind::Continuous);
    EXPECT_EQ(model.variable(2).kind, VariableKind::Binary);
}

TEST(IndicatorTest, KindFreed)
{
    Model model = with_parts(1);
    add(model, when(0, 1, 0, 0.0, 1.0));
    const Index index = 0;
    const Indicator moved = when(2, 1, 0, 0.0, 1.0);
    model.set_indicators(std::span(&index, 1), std::span(&moved, 1));
    set_kind(model, 0, VariableKind::Continuous);
    EXPECT_EQ(model.variable(0).kind, VariableKind::Continuous);
    EXPECT_EQ(kind_failure(model, 2, VariableKind::SemiInteger),
              "in use: variable 2: indicator 0 needs it binary");
}

TEST(IndicatorTest, SharedFunction)
{
    Model model = with_parts(2);
    const std::vector<Indicator> added{when(0, 1, 1, 0.0, 1.0), when(2, 0, 1, -kInfinity, 5.0)};
    model.add_indicators(added);
    ASSERT_EQ(model.n_indicators(), 2);
    expect_indicator(model.indicator(0), added[0]);
    expect_indicator(model.indicator(1), added[1]);
    EXPECT_EQ(model.n_functions(), 2);
}

TEST(IndicatorTest, WithConstraint)
{
    Model model = with_parts(1);
    const Constraint row{.function = 0, .lower = -1.0, .upper = 1.0};
    model.add_constraints(std::span(&row, 1));
    const Indicator switched = when(0, 1, 0, 0.0, 0.0);
    add(model, switched);
    ASSERT_EQ(model.n_constraints(), 1);
    ASSERT_EQ(model.n_indicators(), 1);
    EXPECT_DOUBLE_EQ(model.constraint(0).lower, -1.0);
    expect_indicator(model.indicator(0), switched);
}

TEST(IndicatorTest, Copy)
{
    Model model = with_parts(2);
    model.add_indicators(std::vector<Indicator>{when(0, 1, 1, 0.0, 1.0), when(2, 0, 1, -1.0, 1.0)});
    const Model copy = model;
    EXPECT_EQ(biyos::difference(model, copy), "");
    Model changed = model;
    const Index index = 1;
    const Indicator other = when(2, 1, 1, -1.0, 1.0);
    changed.set_indicators(std::span(&index, 1), std::span(&other, 1));
    EXPECT_EQ(biyos::difference(model, changed), "indicator 1: value 0 against 1");
    EXPECT_EQ(biyos::difference(model, copy), "");
}

TEST(IndicatorTest, Bounds)
{
    Model model = with_parts(1);
    const std::string refused = "invalid argument: indicator 0: ";
    EXPECT_EQ(add_refusal(model, when(0, 1, 0, kInfinity, kInfinity)),
              refused + "lower bound is infinity");
    EXPECT_EQ(add_refusal(model, when(0, 1, 0, -kInfinity, -kInfinity)),
              refused + "upper bound is minus infinity");
    EXPECT_EQ(add_refusal(model, when(0, 1, 0, kNan, 1.0)), refused + "lower bound is NaN");
    EXPECT_EQ(add_refusal(model, when(0, 1, 0, 0.0, kNan)), refused + "upper bound is NaN");
    EXPECT_EQ(add_refusal(model, when(0, 1, 1, 0.0, 1.0)), refused + "function 1 is unknown");
    EXPECT_EQ(model.n_indicators(), 0);
    const Indicator reversed = when(0, 1, 0, 5.0, 1.0);
    add(model, reversed);
    expect_indicator(model.indicator(0), reversed);
}

TEST(IndicatorTest, Changed)
{
    Model model = with_parts(2);
    add(model, when(0, 1, 0, 0.0, 1.0));
    const Index index = 0;
    const Indicator changed = when(2, 0, 1, -2.0, kInfinity);
    model.set_indicators(std::span(&index, 1), std::span(&changed, 1));
    expect_indicator(model.indicator(0), changed);
    EXPECT_EQ(set_refusal(model, 0, when(1, 0, 1, 0.0, 1.0)),
              "invalid argument: indicator 0: variable 1 is not binary");
    EXPECT_EQ(set_refusal(model, 0, when(0, 3, 1, 0.0, 1.0)),
              "invalid argument: indicator 0: value 3 is neither 0 nor 1");
    EXPECT_EQ(set_refusal(model, 1, when(0, 1, 1, 0.0, 1.0)),
              "invalid argument: indicator 1: unknown");
    expect_indicator(model.indicator(0), changed);
}

TEST(IndicatorTest, Unchanged)
{
    Model model = with_parts(1);
    const Indicator kept = when(0, 1, 0, 0.0, 1.0);
    add(model, kept);
    const std::vector<Indicator> refused{when(0, 0, 0, 1.0, 2.0), when(1, 1, 0, 0.0, 1.0)};
    EXPECT_NE(refusal([&] { model.add_indicators(refused); }), "");
    const std::vector<Index> again{0, 0};
    const std::vector<Indicator> nan{when(2, 0, 0, 2.0, 3.0), when(0, 1, 0, kNan, 1.0)};
    EXPECT_NE(refusal([&] { model.set_indicators(again, nan); }), "");
    ASSERT_EQ(model.n_indicators(), 1);
    expect_indicator(model.indicator(0), kept);
}

TEST(IndicatorTest, LastWins)
{
    Model model = with_parts(1);
    model.add_indicators(std::vector<Indicator>{when(0, 1, 0, 0.0, 1.0), when(0, 1, 0, 0.0, 1.0)});
    const std::vector<Index> indices{1, 0, 1};
    const std::vector<Indicator> changed{
        when(2, 0, 0, 1.0, 2.0),
        when(2, 1, 0, 3.0, 4.0),
        when(0, 0, 0, -kInfinity, 6.0),
    };
    model.set_indicators(indices, changed);
    expect_indicator(model.indicator(0), changed[1]);
    expect_indicator(model.indicator(1), changed[2]);
}

TEST(IndicatorTest, ReadRange)
{
    Model model = with_parts(3);
    const std::vector<Indicator> added{
        when(0, 1, 2, 0.0, 1.0),
        when(2, 0, 0, 2.0, 3.0),
        when(0, 0, 1, 4.0, 5.0),
    };
    model.add_indicators(added);
    const std::span<const Indicator> middle = model.indicators(1, 3);
    ASSERT_EQ(middle.size(), 2U);
    expect_indicator(middle[0], added[1]);
    expect_indicator(middle[1], added[2]);
    EXPECT_TRUE(model.indicators(3, 3).empty());
    EXPECT_EQ(refusal([&] { model.indicator(3); }), "invalid argument: indicator 3: unknown");
    EXPECT_EQ(refusal([&] { model.indicators(-1, 0); }), "invalid argument: indicator -1: unknown");
    EXPECT_EQ(refusal([&] { model.indicators(1, 4); }), "invalid argument: indicator 3: unknown");
    EXPECT_EQ(refusal([&] { model.indicators(0, -1); }),
              "invalid argument: argument 'end': -1 is below 'beg' 0");
}

TEST(IndicatorTest, Mismatch)
{
    Model model = with_parts(1);
    add(model, when(0, 1, 0, 0.0, 1.0));
    const std::vector<Index> indices{0};
    failure([&] { model.set_indicators(indices, {}); }, ErrorKind::Internal);
    expect_indicator(model.indicator(0), when(0, 1, 0, 0.0, 1.0));
}

TEST(IndicatorTest, Moved)
{
    Model model = with_parts(1);
    add(model, when(0, 1, 0, 0.0, 1.0));
    const Model taken = std::move(model);
    expect_indicator(taken.indicator(0), when(0, 1, 0, 0.0, 1.0));
    EXPECT_EQ(model.n_indicators(), 0);
    EXPECT_TRUE(model.indicators(0, 0).empty());
}
