#include "writable.hpp"
#include "writing.hpp"

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::default_objective;
using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::Real;
using biyos::VariableKind;
using biyos::test::built;
using biyos::test::Parts;
using biyos::test::refusal;
using biyos::test::sample;
using biyos::test::writable;

namespace
{

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();

std::string fault(VariableKind kind, Real lower, Real upper)
{
    Parts parts = writable();
    parts.variables[1] = {.kind = kind, .lower = lower, .upper = upper};
    return refusal(built(parts));
}

void rename(Model &model, Family family, Index index, std::string_view name)
{
    model.set_names(family, std::span(&index, 1), std::span(&name, 1));
}

}

TEST(MpsTest, Writable)
{
    EXPECT_EQ(refusal(built(writable())), "");
}

TEST(MpsTest, SecondObjective)
{
    Parts parts = writable();
    parts.objectives.push_back(default_objective(0, 1));
    EXPECT_EQ(refusal(built(parts)),
              "unsupported: objective 1: free MPS cannot hold a second objective");
}

TEST(MpsTest, Weight)
{
    Parts parts = writable();
    parts.objectives[0].weight = 2.0;
    EXPECT_EQ(refusal(built(parts)),
              "unsupported: objective 0: free MPS cannot hold a weight other than 1, here 2");
}

TEST(MpsTest, Quadratic)
{
    Parts parts = writable();
    parts.quadratic = sample().quadratic;
    EXPECT_EQ(refusal(built(parts)),
              "unsupported: function 2: free MPS cannot hold quadratic terms");
}

TEST(MpsTest, Expression)
{
    Parts parts = writable();
    parts.expressions = sample().expressions;
    EXPECT_EQ(refusal(built(parts)), "unsupported: function 2: free MPS cannot hold an expression");
}

TEST(MpsTest, Records)
{
    const Parts full = sample();
    Parts cones = writable();
    cones.cones = full.cones;
    EXPECT_EQ(refusal(built(cones)), "unsupported: cone 0: free MPS cannot hold cones");
    Parts complements = writable();
    complements.complements = full.complements;
    EXPECT_EQ(refusal(built(complements)),
              "unsupported: complement 0: free MPS cannot hold complements");
    Parts indicators = writable();
    indicators.indicators = full.indicators;
    EXPECT_EQ(refusal(built(indicators)),
              "unsupported: indicator 0: free MPS cannot hold indicators");
}

TEST(MpsTest, SetNames)
{
    Parts parts = writable();
    parts.special_ordered_sets = sample().special_ordered_sets;
    Model members = built(parts);
    rename(members, Family::Variable, 1, "S2");
    EXPECT_EQ(refusal(members), "unsupported: set 0: member 'S2' cannot be written in free MPS");
    Model sets = built(parts);
    rename(sets, Family::SpecialOrderedSet, 1, "'MARKER'");
    EXPECT_EQ(refusal(sets), "");
    rename(sets, Family::SpecialOrderedSet, 1, "$s");
    EXPECT_EQ(refusal(sets), "unsupported: set 1: name '$s' cannot be written in free MPS");
}

TEST(MpsTest, Callback)
{
    Model model = built(writable());
    model.add_callback(std::vector<Index>{1}, nullptr);
    EXPECT_EQ(refusal(model), "unsupported: callback 0: free MPS cannot hold callbacks");
}

TEST(MpsTest, Repeated)
{
    Model columns = built(writable());
    rename(columns, Family::Variable, 1, "z");
    EXPECT_EQ(refusal(columns), "unsupported: variable 2: name 'z' is repeated");
    Model rows = built(writable());
    rename(rows, Family::Constraint, 2, "cost");
    EXPECT_EQ(refusal(rows), "unsupported: constraint 2: name 'cost' is repeated");
}

TEST(MpsTest, BadNames)
{
    const std::vector<std::string_view> names{"a b", "$a", "'MARKER'", "tab\t"};
    for (const std::string_view name : names)
    {
        Model model = built(writable());
        rename(model, Family::Constraint, 0, name);
        const std::string reason = "name '" + std::string(name) + "' cannot be written in free MPS";
        EXPECT_EQ(refusal(model), "unsupported: constraint 0: " + reason);
    }
    Model model = built(writable());
    rename(model, Family::Variable, 1, "'MARKER'");
    EXPECT_EQ(refusal(model), "");
    rename(model, Family::Variable, 1, "$b");
    EXPECT_EQ(refusal(model), "unsupported: variable 1: name '$b' cannot be written in free MPS");
}

TEST(MpsTest, ModelName)
{
    for (const std::string_view name : {" sample", "sample ", "two\nlines"})
    {
        Model model = built(writable());
        model.set_name(name);
        const std::string reason = "name '" + std::string(name) + "' cannot be written in free MPS";
        EXPECT_EQ(refusal(model), "unsupported: model: " + reason);
    }
}

TEST(MpsTest, KindBounds)
{
    EXPECT_EQ(fault(VariableKind::Binary, 0.0, 0.0),
              "unsupported: variable 1: bounds [0, 0] of its kind cannot be written in free MPS");
    EXPECT_EQ(fault(VariableKind::SemiContinuous, 1.0, kInfinity),
              "unsupported: variable 1: bounds [1, inf] of its kind cannot be written in free MPS");
    EXPECT_EQ(fault(VariableKind::SemiInteger, 2.0, 5.0), "");
}

TEST(MpsTest, RowBounds)
{
    const Real big = std::numeric_limits<Real>::max();
    Parts parts = writable();
    parts.constraints[1] = {.function = 1, .lower = -big, .upper = big};
    EXPECT_EQ(refusal(built(parts)),
              "unsupported: constraint 1: bounds [-1.7976931348623157e+308, "
              "1.7976931348623157e+308] cannot be written exactly in free MPS");
    parts.constraints[1] = {.function = 0, .lower = -big, .upper = 0.0};
    parts.constants[0] = big;
    EXPECT_EQ(refusal(built(parts)),
              "unsupported: constraint 1: a bound overflows once the constant moves into it");
}
