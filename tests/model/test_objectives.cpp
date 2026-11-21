#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::Constraint;
using biyos::default_obj;
using biyos::difference;
using biyos::Index;
using biyos::Model;
using biyos::Objective;
using biyos::Offset;
using biyos::Sense;
using biyos::test::failure;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
const double kSignedNan = std::copysign(kNan, -1.0);
constexpr int kLeast = (std::numeric_limits<int>::min)();
constexpr int kMost = (std::numeric_limits<int>::max)();

const Objective kMaximize{
    .function = 1,
    .sense = Sense::Maximize,
    .priority = -3,
    .weight = 2.5,
    .absolute = 0.5,
    .relative = kInfinity,
};

Model with_funs(std::size_t n)
{
    Model mdl;
    const std::vector<double> constants(n, 0.0);
    const std::vector<Offset> starts(n + 1, 0);
    mdl.add_funs(constants, starts, {});
    return mdl;
}

Objective weighted(double weight)
{
    Objective obj = default_obj(0, 0);
    obj.weight = weight;
    return obj;
}

Objective tolerant(double absolute, double relative)
{
    Objective obj = default_obj(0, 0);
    obj.absolute = absolute;
    obj.relative = relative;
    return obj;
}

void add(Model &mdl, const Objective &obj)
{
    mdl.add_objs(std::span(&obj, 1));
}

std::string add_refusal(Model &mdl, const Objective &obj)
{
    return refusal([&] { add(mdl, obj); });
}

std::string set_refusal(Model &mdl, Index idx, const Objective &obj)
{
    return refusal([&] { mdl.set_objs(std::span(&idx, 1), std::span(&obj, 1)); });
}

std::uint64_t bits_of(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

void expect_obj(const Objective &found, const Objective &wanted)
{
    EXPECT_EQ(found.function, wanted.function);
    EXPECT_EQ(found.sense, wanted.sense);
    EXPECT_EQ(found.priority, wanted.priority);
    EXPECT_EQ(bits_of(found.weight), bits_of(wanted.weight)) << found.weight;
    EXPECT_EQ(bits_of(found.absolute), bits_of(wanted.absolute)) << found.absolute;
    EXPECT_EQ(bits_of(found.relative), bits_of(wanted.relative)) << found.relative;
}

}

TEST(ObjectiveTest, Defaults)
{
    const Objective wanted{
        .function = 2,
        .sense = Sense::Minimize,
        .priority = 5,
        .weight = 1.0,
        .absolute = 0.0,
        .relative = 0.0,
    };
    expect_obj(default_obj(2, 5), wanted);
    Model mdl = with_funs(3);
    add(mdl, default_obj(2, 0));
    expect_obj(mdl.objective(0), default_obj(2, 0));
}

TEST(ObjectiveTest, Added)
{
    Model mdl = with_funs(2);
    const std::vector<Objective> added{default_obj(0, 0), kMaximize};
    mdl.add_objs(added);
    ASSERT_EQ(mdl.n_objs(), 2);
    expect_obj(mdl.objective(0), added[0]);
    expect_obj(mdl.objective(1), kMaximize);
    mdl.add_objs({});
    EXPECT_EQ(mdl.n_objs(), 2);
}

TEST(ObjectiveTest, Shared)
{
    Model mdl = with_funs(2);
    Objective other = kMaximize;
    other.sense = Sense::Minimize;
    mdl.add_objs(std::vector<Objective>{kMaximize, other});
    ASSERT_EQ(mdl.n_objs(), 2);
    expect_obj(mdl.objective(0), kMaximize);
    expect_obj(mdl.objective(1), other);
    EXPECT_EQ(mdl.n_funs(), 2);
}

TEST(ObjectiveTest, ConstraintFunction)
{
    Model mdl = with_funs(2);
    const Constraint con{.function = 1, .lower = 0.0, .upper = 1.0};
    mdl.add_cons(std::span(&con, 1));
    add(mdl, kMaximize);
    expect_obj(mdl.objective(0), kMaximize);
    EXPECT_EQ(mdl.constraint(0).function, 1);
    EXPECT_EQ(mdl.n_funs(), 2);
}

TEST(ObjectiveTest, Priorities)
{
    Model mdl = with_funs(1);
    std::vector<Objective> added;
    for (const int priority : {kLeast, -1, 0, 0, 7, kMost})
    {
        added.push_back(default_obj(0, priority));
    }
    mdl.add_objs(added);
    ASSERT_EQ(mdl.n_objs(), 6);
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        expect_obj(mdl.objective(static_cast<Index>(k)), added[k]);
    }
}

TEST(ObjectiveTest, UnknownFunction)
{
    Model mdl = with_funs(2);
    add(mdl, kMaximize);
    EXPECT_EQ(add_refusal(mdl, default_obj(2, 0)),
              "invalid argument: objective 1: function 2 is unknown");
    EXPECT_EQ(add_refusal(mdl, default_obj(-1, 0)),
              "invalid argument: objective 1: function -1 is unknown");
    EXPECT_EQ(set_refusal(mdl, 0, default_obj(9, 0)),
              "invalid argument: objective 0: function 9 is unknown");
    EXPECT_EQ(mdl.n_objs(), 1);
    expect_obj(mdl.objective(0), kMaximize);
    Model empty;
    EXPECT_EQ(add_refusal(empty, default_obj(0, 0)),
              "invalid argument: objective 0: function 0 is unknown");
}

TEST(ObjectiveTest, UnknownSense)
{
    Model mdl = with_funs(1);
    Objective none = default_obj(0, 0);
    none.sense = Sense{0};
    Objective third = default_obj(0, 0);
    third.sense = Sense{3};
    EXPECT_EQ(add_refusal(mdl, none), "invalid argument: objective 0: sense 0 is unknown");
    EXPECT_EQ(add_refusal(mdl, third), "invalid argument: objective 0: sense 3 is unknown");
    EXPECT_EQ(mdl.n_objs(), 0);
}

TEST(ObjectiveTest, Weights)
{
    Model mdl = with_funs(1);
    const std::string refused = "invalid argument: objective 0: weight ";
    EXPECT_EQ(add_refusal(mdl, weighted(kNan)), refused + "is NaN");
    EXPECT_EQ(add_refusal(mdl, weighted(kSignedNan)), refused + "is NaN");
    EXPECT_EQ(add_refusal(mdl, weighted(kInfinity)), refused + "is infinity");
    EXPECT_EQ(add_refusal(mdl, weighted(-kInfinity)), refused + "is minus infinity");
    EXPECT_EQ(add_refusal(mdl, weighted(0.0)), refused + "0 is not positive");
    EXPECT_EQ(add_refusal(mdl, weighted(-0.0)), refused + "-0 is not positive");
    EXPECT_EQ(add_refusal(mdl, weighted(-2.0)), refused + "-2 is not positive");
    EXPECT_EQ(mdl.n_objs(), 0);
    const double least = std::numeric_limits<double>::denorm_min();
    const double most = (std::numeric_limits<double>::max)();
    mdl.add_objs(std::vector<Objective>{weighted(least), weighted(most)});
    expect_obj(mdl.objective(0), weighted(least));
    expect_obj(mdl.objective(1), weighted(most));
}

TEST(ObjectiveTest, Tolerances)
{
    Model mdl = with_funs(1);
    const std::string refused = "invalid argument: objective 0: ";
    EXPECT_EQ(add_refusal(mdl, tolerant(kNan, 0.0)), refused + "absolute tolerance is NaN");
    EXPECT_EQ(add_refusal(mdl, tolerant(0.0, kNan)), refused + "relative tolerance is NaN");
    EXPECT_EQ(add_refusal(mdl, tolerant(kSignedNan, 0.0)), refused + "absolute tolerance is NaN");
    EXPECT_EQ(add_refusal(mdl, tolerant(-1.0, 0.0)), refused + "absolute tolerance -1 is negative");
    EXPECT_EQ(add_refusal(mdl, tolerant(0.0, -kInfinity)),
              refused + "relative tolerance is minus infinity");
    const double least = std::numeric_limits<double>::denorm_min();
    EXPECT_EQ(add_refusal(mdl, tolerant(0.0, -least)),
              refused + "relative tolerance -5e-324 is negative");
    EXPECT_EQ(mdl.n_objs(), 0);
    const std::vector<Objective> kept{
        tolerant(kInfinity, kInfinity),
        tolerant(-0.0, -0.0),
        tolerant(least, 0.25),
    };
    mdl.add_objs(kept);
    ASSERT_EQ(mdl.n_objs(), 3);
    for (std::size_t k = 0; k < kept.size(); ++k)
    {
        expect_obj(mdl.objective(static_cast<Index>(k)), kept[k]);
    }
}

TEST(ObjectiveTest, Order)
{
    Model mdl = with_funs(1);
    const Objective bad{
        .function = 1,
        .sense = Sense{0},
        .priority = 0,
        .weight = 0.0,
        .absolute = kNan,
        .relative = kNan,
    };
    const std::string refused = "invalid argument: objective 0: ";
    Objective found = bad;
    EXPECT_EQ(add_refusal(mdl, found), refused + "function 1 is unknown");
    found.function = 0;
    EXPECT_EQ(add_refusal(mdl, found), refused + "sense 0 is unknown");
    found.sense = Sense::Maximize;
    EXPECT_EQ(add_refusal(mdl, found), refused + "weight 0 is not positive");
    found.weight = 1.0;
    EXPECT_EQ(add_refusal(mdl, found), refused + "absolute tolerance is NaN");
    found.absolute = 0.0;
    EXPECT_EQ(add_refusal(mdl, found), refused + "relative tolerance is NaN");
    const std::vector<Objective> both{weighted(-1.0), bad};
    EXPECT_EQ(refusal([&] { mdl.add_objs(both); }), refused + "weight -1 is not positive");
    EXPECT_EQ(mdl.n_objs(), 0);
}

TEST(ObjectiveTest, Unchanged)
{
    Model mdl = with_funs(2);
    add(mdl, kMaximize);
    const std::vector<Objective> refused{default_obj(0, 1), weighted(kNan)};
    EXPECT_NE(refusal([&] { mdl.add_objs(refused); }), "");
    const std::vector<Index> idx{0, 1};
    const std::vector<Objective> changed{default_obj(0, 4), kMaximize};
    EXPECT_NE(refusal([&] { mdl.set_objs(idx, changed); }), "");
    const std::vector<Index> again{0, 0};
    const std::vector<Objective> nan{default_obj(0, 4), tolerant(kNan, 0.0)};
    EXPECT_NE(refusal([&] { mdl.set_objs(again, nan); }), "");
    ASSERT_EQ(mdl.n_objs(), 1);
    expect_obj(mdl.objective(0), kMaximize);
}

TEST(ObjectiveTest, LastWins)
{
    Model mdl = with_funs(2);
    mdl.add_objs(std::vector<Objective>{default_obj(0, 0), default_obj(0, 1)});
    const std::vector<Index> idx{1, 0, 1};
    const std::vector<Objective> changed{kMaximize, tolerant(1.0, 2.0), default_obj(1, 9)};
    mdl.set_objs(idx, changed);
    expect_obj(mdl.objective(0), changed[1]);
    expect_obj(mdl.objective(1), changed[2]);
}

TEST(ObjectiveTest, BadIndex)
{
    Model mdl = with_funs(1);
    add(mdl, default_obj(0, 0));
    EXPECT_EQ(refusal([&] { mdl.objective(-1); }), "invalid argument: objective -1: unknown");
    EXPECT_EQ(refusal([&] { mdl.objective(1); }), "invalid argument: objective 1: unknown");
    EXPECT_EQ(set_refusal(mdl, 1, default_obj(0, 0)), "invalid argument: objective 1: unknown");
    EXPECT_EQ(set_refusal(mdl, 0, weighted(0.0)),
              "invalid argument: objective 0: weight 0 is not positive");
}

TEST(ObjectiveTest, ReadRange)
{
    Model mdl = with_funs(3);
    const std::vector<Objective> added{default_obj(2, 0), kMaximize, default_obj(0, 2)};
    mdl.add_objs(added);
    const std::span<const Objective> middle = mdl.objs(1, 2);
    ASSERT_EQ(middle.size(), 2U);
    expect_obj(middle[0], added[1]);
    expect_obj(middle[1], added[2]);
    EXPECT_TRUE(mdl.objs(3, 0).empty());
    EXPECT_EQ(refusal([&] { mdl.objs(-1, 1); }), "invalid argument: objective -1: unknown");
    EXPECT_EQ(refusal([&] { mdl.objs(1, 3); }), "invalid argument: objective 3: unknown");
    EXPECT_EQ(refusal([&] { mdl.objs(4, 0); }), "invalid argument: objective 4: unknown");
    EXPECT_EQ(refusal([&] { mdl.objs(0, -1); }), "invalid argument: argument 'n': -1 is negative");
}

TEST(ObjectiveTest, Mismatch)
{
    Model mdl = with_funs(1);
    add(mdl, default_obj(0, 0));
    const std::vector<Index> idx{0};
    failure([&] { mdl.set_objs(idx, {}); }, biyos::ErrorKind::Internal);
    expect_obj(mdl.objective(0), default_obj(0, 0));
}

TEST(ObjectiveTest, Room)
{
    Model mdl = with_funs(1);
    mdl.check_obj_room(biyos::kRoom);
    const auto beyond = [&] { mdl.check_obj_room(biyos::kRoom + 1); };
    EXPECT_EQ(failure(beyond, biyos::ErrorKind::NoMemory),
              "out of memory: model: no objective index left");
    add(mdl, default_obj(0, 0));
    const auto full = [&] { mdl.check_obj_room(biyos::kRoom); };
    EXPECT_EQ(failure(full, biyos::ErrorKind::NoMemory),
              "out of memory: model: no objective index left");
    mdl.check_obj_room(biyos::kRoom - 1);
}

TEST(ObjectiveTest, Moved)
{
    Model mdl = with_funs(2);
    add(mdl, kMaximize);
    const Model taken = std::move(mdl);
    expect_obj(taken.objective(0), kMaximize);
    EXPECT_EQ(mdl.n_objs(), 0);
    EXPECT_TRUE(mdl.objs(0, 0).empty());
    EXPECT_EQ(add_refusal(mdl, kMaximize), "invalid argument: objective 0: function 1 is unknown");
}

TEST(ObjectiveTest, Copied)
{
    Model mdl = with_funs(2);
    mdl.add_objs(std::vector<Objective>{kMaximize, default_obj(0, 1)});
    Model copy = mdl;
    EXPECT_EQ(difference(mdl, copy), "");
    const Index idx = 1;
    copy.set_objs(std::span(&idx, 1), std::span(&kMaximize, 1));
    EXPECT_EQ(difference(mdl, copy), "objective 1: function 0 against 1");
    expect_obj(mdl.objective(1), default_obj(0, 1));
}
