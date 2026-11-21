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

Model with_funs(std::size_t n)
{
    Model mdl;
    const std::vector<double> constants(n, 0.0);
    const std::vector<Offset> starts(n + 1, 0);
    mdl.add_funs(constants, starts, {});
    return mdl;
}

Constraint on(Index fun, double lower, double upper)
{
    return Constraint{.function = fun, .lower = lower, .upper = upper};
}

void add(Model &mdl, const Constraint &con)
{
    mdl.add_cons(std::span(&con, 1));
}

std::string add_refusal(Model &mdl, const Constraint &con)
{
    return refusal([&] { add(mdl, con); });
}

std::string set_refusal(Model &mdl, Index idx, const Constraint &con)
{
    return refusal([&] { mdl.set_cons(std::span(&idx, 1), std::span(&con, 1)); });
}

void expect_con(const Constraint &found, const Constraint &wanted)
{
    EXPECT_EQ(found.function, wanted.function);
    EXPECT_DOUBLE_EQ(found.lower, wanted.lower);
    EXPECT_DOUBLE_EQ(found.upper, wanted.upper);
}

}

TEST(ConstraintTest, Ranged)
{
    Model mdl = with_funs(1);
    const Constraint ranged = on(0, -1.0, 2.0);
    add(mdl, ranged);
    ASSERT_EQ(mdl.n_cons(), 1);
    expect_con(mdl.constraint(0), ranged);
}

TEST(ConstraintTest, Free)
{
    Model mdl = with_funs(1);
    const std::vector<Constraint> added{
        on(0, -kInfinity, kInfinity),
        on(0, -kInfinity, 4.0),
        on(0, 4.0, kInfinity),
    };
    mdl.add_cons(added);
    expect_con(mdl.constraint(0), added[0]);
    expect_con(mdl.constraint(1), added[1]);
    expect_con(mdl.constraint(2), added[2]);
}

TEST(ConstraintTest, Equality)
{
    Model mdl = with_funs(1);
    const Constraint equal = on(0, 3.0, 3.0);
    add(mdl, equal);
    expect_con(mdl.constraint(0), equal);
}

TEST(ConstraintTest, Reversed)
{
    Model mdl = with_funs(1);
    const Constraint reversed = on(0, 5.0, 1.0);
    add(mdl, reversed);
    expect_con(mdl.constraint(0), reversed);
}

TEST(ConstraintTest, Shared)
{
    Model mdl = with_funs(2);
    const std::vector<Constraint> added{on(1, 0.0, 1.0), on(1, -kInfinity, 5.0)};
    mdl.add_cons(added);
    ASSERT_EQ(mdl.n_cons(), 2);
    expect_con(mdl.constraint(0), added[0]);
    expect_con(mdl.constraint(1), added[1]);
    EXPECT_EQ(mdl.n_funs(), 2);
}

TEST(ConstraintTest, UnknownFunction)
{
    Model mdl = with_funs(2);
    add(mdl, on(1, 0.0, 1.0));
    EXPECT_EQ(add_refusal(mdl, on(2, 0.0, 1.0)),
              "invalid argument: constraint 1: function 2 is unknown");
    EXPECT_EQ(add_refusal(mdl, on(-1, kNan, 1.0)),
              "invalid argument: constraint 1: function -1 is unknown");
    EXPECT_EQ(set_refusal(mdl, 0, on(9, 0.0, 1.0)),
              "invalid argument: constraint 0: function 9 is unknown");
    EXPECT_EQ(mdl.n_cons(), 1);
    expect_con(mdl.constraint(0), on(1, 0.0, 1.0));
    Model empty;
    EXPECT_EQ(add_refusal(empty, on(0, 0.0, 1.0)),
              "invalid argument: constraint 0: function 0 is unknown");
}

TEST(ConstraintTest, Infinities)
{
    Model mdl = with_funs(1);
    const std::string refused = "invalid argument: constraint 0: ";
    EXPECT_EQ(add_refusal(mdl, on(0, kInfinity, kInfinity)), refused + "lower bound is infinity");
    EXPECT_EQ(add_refusal(mdl, on(0, -kInfinity, -kInfinity)),
              refused + "upper bound is minus infinity");
    EXPECT_EQ(mdl.n_cons(), 0);
}

TEST(ConstraintTest, NanBounds)
{
    Model mdl = with_funs(1);
    const std::string refused = "invalid argument: constraint 0: ";
    EXPECT_EQ(add_refusal(mdl, on(0, kNan, 1.0)), refused + "lower bound is NaN");
    EXPECT_EQ(add_refusal(mdl, on(0, 0.0, kNan)), refused + "upper bound is NaN");
    EXPECT_EQ(mdl.n_cons(), 0);
}

TEST(ConstraintTest, Unchanged)
{
    Model mdl = with_funs(1);
    const Constraint kept = on(0, 0.0, 1.0);
    add(mdl, kept);
    const std::vector<Constraint> refused{on(0, 1.0, 2.0), on(0, kNan, 2.0)};
    EXPECT_NE(refusal([&] { mdl.add_cons(refused); }), "");
    const std::vector<Index> idx{0, 1};
    const std::vector<Constraint> changed{on(0, 2.0, 3.0), kept};
    EXPECT_NE(refusal([&] { mdl.set_cons(idx, changed); }), "");
    const std::vector<Index> again{0, 0};
    const std::vector<Constraint> nan{on(0, 2.0, 3.0), on(0, kNan, 1.0)};
    EXPECT_NE(refusal([&] { mdl.set_cons(again, nan); }), "");
    ASSERT_EQ(mdl.n_cons(), 1);
    expect_con(mdl.constraint(0), kept);
}

TEST(ConstraintTest, LastWins)
{
    Model mdl = with_funs(1);
    mdl.add_cons(std::vector<Constraint>{on(0, 0.0, 1.0), on(0, 0.0, 1.0)});
    const std::vector<Index> idx{1, 0, 1};
    const std::vector<Constraint> changed{
        on(0, 1.0, 2.0),
        on(0, 3.0, 4.0),
        on(0, -kInfinity, 6.0),
    };
    mdl.set_cons(idx, changed);
    expect_con(mdl.constraint(0), changed[1]);
    expect_con(mdl.constraint(1), changed[2]);
}

TEST(ConstraintTest, BadIndex)
{
    Model mdl = with_funs(1);
    add(mdl, on(0, 0.0, 1.0));
    EXPECT_EQ(refusal([&] { mdl.constraint(-1); }), "invalid argument: constraint -1: unknown");
    EXPECT_EQ(refusal([&] { mdl.constraint(1); }), "invalid argument: constraint 1: unknown");
    EXPECT_EQ(set_refusal(mdl, 1, on(0, 0.0, 1.0)), "invalid argument: constraint 1: unknown");
    EXPECT_EQ(set_refusal(mdl, 0, on(0, kInfinity, 1.0)),
              "invalid argument: constraint 0: lower bound is infinity");
}

TEST(ConstraintTest, ReadRange)
{
    Model mdl = with_funs(3);
    const std::vector<Constraint> added{on(2, 0.0, 1.0), on(0, 2.0, 3.0), on(1, 4.0, 5.0)};
    mdl.add_cons(added);
    const std::span<const Constraint> middle = mdl.cons(1, 2);
    ASSERT_EQ(middle.size(), 2U);
    expect_con(middle[0], added[1]);
    expect_con(middle[1], added[2]);
    EXPECT_TRUE(mdl.cons(3, 0).empty());
    EXPECT_EQ(refusal([&] { mdl.cons(-1, 1); }), "invalid argument: constraint -1: unknown");
    EXPECT_EQ(refusal([&] { mdl.cons(1, 3); }), "invalid argument: constraint 3: unknown");
    EXPECT_EQ(refusal([&] { mdl.cons(4, 0); }), "invalid argument: constraint 4: unknown");
    EXPECT_EQ(refusal([&] { mdl.cons(0, -1); }), "invalid argument: argument 'n': -1 is negative");
}

TEST(ConstraintTest, Mismatch)
{
    Model mdl = with_funs(1);
    add(mdl, on(0, 0.0, 1.0));
    const std::vector<Index> idx{0};
    failure([&] { mdl.set_cons(idx, {}); }, biyos::ErrorKind::Internal);
    expect_con(mdl.constraint(0), on(0, 0.0, 1.0));
}

TEST(ConstraintTest, Moved)
{
    Model mdl = with_funs(1);
    add(mdl, on(0, 0.0, 1.0));
    const Model taken = std::move(mdl);
    expect_con(taken.constraint(0), on(0, 0.0, 1.0));
    EXPECT_EQ(mdl.n_cons(), 0);
    EXPECT_TRUE(mdl.cons(0, 0).empty());
    EXPECT_EQ(add_refusal(mdl, on(0, 0.0, 1.0)),
              "invalid argument: constraint 0: function 0 is unknown");
}
