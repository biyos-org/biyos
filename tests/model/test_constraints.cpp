#include "core/error.hpp"
#include "core/index.hpp"
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
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::test::failure;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

Model with_functions(std::size_t count)
{
    Model model;
    const std::vector<double> constants(count, 0.0);
    const std::vector<Offset> starts(count + 1, 0);
    model.add_functions(constants, starts, {});
    return model;
}

Constraint on(Index function, double lower, double upper)
{
    return Constraint{.function = function, .lower = lower, .upper = upper};
}

void add(Model &model, const Constraint &constraint)
{
    model.add_constraints(std::span(&constraint, 1));
}

std::string add_refusal(Model &model, const Constraint &constraint)
{
    return refusal([&] { add(model, constraint); });
}

std::string set_refusal(Model &model, Index index, const Constraint &constraint)
{
    return refusal([&] { model.set_constraints(std::span(&index, 1), std::span(&constraint, 1)); });
}

void expect_constraint(const Constraint &found, const Constraint &wanted)
{
    EXPECT_EQ(found.function, wanted.function);
    EXPECT_DOUBLE_EQ(found.lower, wanted.lower);
    EXPECT_DOUBLE_EQ(found.upper, wanted.upper);
}

}

TEST(ConstraintTest, Ranged)
{
    Model model = with_functions(1);
    const Constraint ranged = on(0, -1.0, 2.0);
    add(model, ranged);
    ASSERT_EQ(model.constraint_count(), 1);
    expect_constraint(model.constraint(0), ranged);
}

TEST(ConstraintTest, Free)
{
    Model model = with_functions(1);
    const std::vector<Constraint> added{
        on(0, -kInfinity, kInfinity),
        on(0, -kInfinity, 4.0),
        on(0, 4.0, kInfinity),
    };
    model.add_constraints(added);
    expect_constraint(model.constraint(0), added[0]);
    expect_constraint(model.constraint(1), added[1]);
    expect_constraint(model.constraint(2), added[2]);
}

TEST(ConstraintTest, Equality)
{
    Model model = with_functions(1);
    const Constraint equal = on(0, 3.0, 3.0);
    add(model, equal);
    expect_constraint(model.constraint(0), equal);
}

TEST(ConstraintTest, Reversed)
{
    Model model = with_functions(1);
    const Constraint reversed = on(0, 5.0, 1.0);
    add(model, reversed);
    expect_constraint(model.constraint(0), reversed);
}

TEST(ConstraintTest, Shared)
{
    Model model = with_functions(2);
    const std::vector<Constraint> added{on(1, 0.0, 1.0), on(1, -kInfinity, 5.0)};
    model.add_constraints(added);
    ASSERT_EQ(model.constraint_count(), 2);
    expect_constraint(model.constraint(0), added[0]);
    expect_constraint(model.constraint(1), added[1]);
    EXPECT_EQ(model.function_count(), 2);
}

TEST(ConstraintTest, UnknownFunction)
{
    Model model = with_functions(2);
    add(model, on(1, 0.0, 1.0));
    EXPECT_EQ(add_refusal(model, on(2, 0.0, 1.0)),
              "invalid argument: constraint 1: function 2 is unknown");
    EXPECT_EQ(add_refusal(model, on(-1, kNan, 1.0)),
              "invalid argument: constraint 1: function -1 is unknown");
    EXPECT_EQ(set_refusal(model, 0, on(9, 0.0, 1.0)),
              "invalid argument: constraint 0: function 9 is unknown");
    EXPECT_EQ(model.constraint_count(), 1);
    expect_constraint(model.constraint(0), on(1, 0.0, 1.0));
    Model empty;
    EXPECT_EQ(add_refusal(empty, on(0, 0.0, 1.0)),
              "invalid argument: constraint 0: function 0 is unknown");
}

TEST(ConstraintTest, Infinities)
{
    Model model = with_functions(1);
    const std::string refused = "invalid argument: constraint 0: ";
    EXPECT_EQ(add_refusal(model, on(0, kInfinity, kInfinity)), refused + "lower bound is infinity");
    EXPECT_EQ(add_refusal(model, on(0, -kInfinity, -kInfinity)),
              refused + "upper bound is minus infinity");
    EXPECT_EQ(model.constraint_count(), 0);
}

TEST(ConstraintTest, NanBounds)
{
    Model model = with_functions(1);
    const std::string refused = "invalid argument: constraint 0: ";
    EXPECT_EQ(add_refusal(model, on(0, kNan, 1.0)), refused + "lower bound is NaN");
    EXPECT_EQ(add_refusal(model, on(0, 0.0, kNan)), refused + "upper bound is NaN");
    EXPECT_EQ(model.constraint_count(), 0);
}

TEST(ConstraintTest, Unchanged)
{
    Model model = with_functions(1);
    const Constraint kept = on(0, 0.0, 1.0);
    add(model, kept);
    const std::vector<Constraint> refused{on(0, 1.0, 2.0), on(0, kNan, 2.0)};
    EXPECT_NE(refusal([&] { model.add_constraints(refused); }), "");
    const std::vector<Index> indices{0, 1};
    const std::vector<Constraint> changed{on(0, 2.0, 3.0), kept};
    EXPECT_NE(refusal([&] { model.set_constraints(indices, changed); }), "");
    const std::vector<Index> again{0, 0};
    const std::vector<Constraint> nan{on(0, 2.0, 3.0), on(0, kNan, 1.0)};
    EXPECT_NE(refusal([&] { model.set_constraints(again, nan); }), "");
    ASSERT_EQ(model.constraint_count(), 1);
    expect_constraint(model.constraint(0), kept);
}

TEST(ConstraintTest, LastWins)
{
    Model model = with_functions(1);
    model.add_constraints(std::vector<Constraint>{on(0, 0.0, 1.0), on(0, 0.0, 1.0)});
    const std::vector<Index> indices{1, 0, 1};
    const std::vector<Constraint> changed{
        on(0, 1.0, 2.0),
        on(0, 3.0, 4.0),
        on(0, -kInfinity, 6.0),
    };
    model.set_constraints(indices, changed);
    expect_constraint(model.constraint(0), changed[1]);
    expect_constraint(model.constraint(1), changed[2]);
}

TEST(ConstraintTest, BadIndex)
{
    Model model = with_functions(1);
    add(model, on(0, 0.0, 1.0));
    EXPECT_EQ(refusal([&] { model.constraint(-1); }), "invalid argument: constraint -1: unknown");
    EXPECT_EQ(refusal([&] { model.constraint(1); }), "invalid argument: constraint 1: unknown");
    EXPECT_EQ(set_refusal(model, 1, on(0, 0.0, 1.0)), "invalid argument: constraint 1: unknown");
    EXPECT_EQ(set_refusal(model, 0, on(0, kInfinity, 1.0)),
              "invalid argument: constraint 0: lower bound is infinity");
}

TEST(ConstraintTest, ReadRange)
{
    Model model = with_functions(3);
    const std::vector<Constraint> added{on(2, 0.0, 1.0), on(0, 2.0, 3.0), on(1, 4.0, 5.0)};
    model.add_constraints(added);
    const std::span<const Constraint> middle = model.constraints(1, 2);
    ASSERT_EQ(middle.size(), 2U);
    expect_constraint(middle[0], added[1]);
    expect_constraint(middle[1], added[2]);
    EXPECT_TRUE(model.constraints(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.constraints(-1, 1); }),
              "invalid argument: constraint -1: unknown");
    EXPECT_EQ(refusal([&] { model.constraints(1, 3); }), "invalid argument: constraint 3: unknown");
    EXPECT_EQ(refusal([&] { model.constraints(4, 0); }), "invalid argument: constraint 4: unknown");
    EXPECT_EQ(refusal([&] { model.constraints(0, -1); }),
              "invalid argument: argument 'count': -1 is negative");
}

TEST(ConstraintTest, Mismatch)
{
    Model model = with_functions(1);
    add(model, on(0, 0.0, 1.0));
    const std::vector<Index> indices{0};
    failure([&] { model.set_constraints(indices, {}); }, biyos::ErrorKind::Internal);
    expect_constraint(model.constraint(0), on(0, 0.0, 1.0));
}

TEST(ConstraintTest, Moved)
{
    Model model = with_functions(1);
    add(model, on(0, 0.0, 1.0));
    const Model taken = std::move(model);
    expect_constraint(taken.constraint(0), on(0, 0.0, 1.0));
    EXPECT_EQ(model.constraint_count(), 0);
    EXPECT_TRUE(model.constraints(0, 0).empty());
    EXPECT_EQ(add_refusal(model, on(0, 0.0, 1.0)),
              "invalid argument: constraint 0: function 0 is unknown");
}
