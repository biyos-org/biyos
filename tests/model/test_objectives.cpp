#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
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
using biyos::default_objective;
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

Model with_functions(std::size_t count)
{
    Model model;
    const std::vector<double> constants(count, 0.0);
    const std::vector<Offset> starts(count + 1, 0);
    model.add_functions(constants, starts, {});
    return model;
}

Objective weighted(double weight)
{
    Objective objective = default_objective(0, 0);
    objective.weight = weight;
    return objective;
}

Objective tolerant(double absolute, double relative)
{
    Objective objective = default_objective(0, 0);
    objective.absolute = absolute;
    objective.relative = relative;
    return objective;
}

void add(Model &model, const Objective &objective)
{
    model.add_objectives(std::span(&objective, 1));
}

std::string add_refusal(Model &model, const Objective &objective)
{
    return refusal([&] { add(model, objective); });
}

std::string set_refusal(Model &model, Index index, const Objective &objective)
{
    return refusal([&] { model.set_objectives(std::span(&index, 1), std::span(&objective, 1)); });
}

std::uint64_t bits_of(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

void expect_objective(const Objective &found, const Objective &wanted)
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
    expect_objective(default_objective(2, 5), wanted);
    Model model = with_functions(3);
    add(model, default_objective(2, 0));
    expect_objective(model.objective(0), default_objective(2, 0));
}

TEST(ObjectiveTest, Added)
{
    Model model = with_functions(2);
    const std::vector<Objective> added{default_objective(0, 0), kMaximize};
    model.add_objectives(added);
    ASSERT_EQ(model.objective_count(), 2);
    expect_objective(model.objective(0), added[0]);
    expect_objective(model.objective(1), kMaximize);
    model.add_objectives({});
    EXPECT_EQ(model.objective_count(), 2);
}

TEST(ObjectiveTest, Shared)
{
    Model model = with_functions(2);
    Objective other = kMaximize;
    other.sense = Sense::Minimize;
    model.add_objectives(std::vector<Objective>{kMaximize, other});
    ASSERT_EQ(model.objective_count(), 2);
    expect_objective(model.objective(0), kMaximize);
    expect_objective(model.objective(1), other);
    EXPECT_EQ(model.function_count(), 2);
}

TEST(ObjectiveTest, ConstraintFunction)
{
    Model model = with_functions(2);
    const Constraint constraint{.function = 1, .lower = 0.0, .upper = 1.0};
    model.add_constraints(std::span(&constraint, 1));
    add(model, kMaximize);
    expect_objective(model.objective(0), kMaximize);
    EXPECT_EQ(model.constraint(0).function, 1);
    EXPECT_EQ(model.function_count(), 2);
}

TEST(ObjectiveTest, Priorities)
{
    Model model = with_functions(1);
    std::vector<Objective> added;
    for (const int priority : {kLeast, -1, 0, 0, 7, kMost})
    {
        added.push_back(default_objective(0, priority));
    }
    model.add_objectives(added);
    ASSERT_EQ(model.objective_count(), 6);
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        expect_objective(model.objective(static_cast<Index>(k)), added[k]);
    }
}

TEST(ObjectiveTest, UnknownFunction)
{
    Model model = with_functions(2);
    add(model, kMaximize);
    EXPECT_EQ(add_refusal(model, default_objective(2, 0)),
              "invalid argument: objective 1: function 2 is unknown");
    EXPECT_EQ(add_refusal(model, default_objective(-1, 0)),
              "invalid argument: objective 1: function -1 is unknown");
    EXPECT_EQ(set_refusal(model, 0, default_objective(9, 0)),
              "invalid argument: objective 0: function 9 is unknown");
    EXPECT_EQ(model.objective_count(), 1);
    expect_objective(model.objective(0), kMaximize);
    Model empty;
    EXPECT_EQ(add_refusal(empty, default_objective(0, 0)),
              "invalid argument: objective 0: function 0 is unknown");
}

TEST(ObjectiveTest, UnknownSense)
{
    Model model = with_functions(1);
    Objective none = default_objective(0, 0);
    none.sense = Sense{0};
    Objective third = default_objective(0, 0);
    third.sense = Sense{3};
    EXPECT_EQ(add_refusal(model, none), "invalid argument: objective 0: sense 0 is unknown");
    EXPECT_EQ(add_refusal(model, third), "invalid argument: objective 0: sense 3 is unknown");
    EXPECT_EQ(model.objective_count(), 0);
}

TEST(ObjectiveTest, Weights)
{
    Model model = with_functions(1);
    const std::string refused = "invalid argument: objective 0: weight ";
    EXPECT_EQ(add_refusal(model, weighted(kNan)), refused + "is NaN");
    EXPECT_EQ(add_refusal(model, weighted(kSignedNan)), refused + "is NaN");
    EXPECT_EQ(add_refusal(model, weighted(kInfinity)), refused + "is infinity");
    EXPECT_EQ(add_refusal(model, weighted(-kInfinity)), refused + "is minus infinity");
    EXPECT_EQ(add_refusal(model, weighted(0.0)), refused + "0 is not positive");
    EXPECT_EQ(add_refusal(model, weighted(-0.0)), refused + "-0 is not positive");
    EXPECT_EQ(add_refusal(model, weighted(-2.0)), refused + "-2 is not positive");
    EXPECT_EQ(model.objective_count(), 0);
    const double least = std::numeric_limits<double>::denorm_min();
    const double most = (std::numeric_limits<double>::max)();
    model.add_objectives(std::vector<Objective>{weighted(least), weighted(most)});
    expect_objective(model.objective(0), weighted(least));
    expect_objective(model.objective(1), weighted(most));
}

TEST(ObjectiveTest, Tolerances)
{
    Model model = with_functions(1);
    const std::string refused = "invalid argument: objective 0: ";
    EXPECT_EQ(add_refusal(model, tolerant(kNan, 0.0)), refused + "absolute tolerance is NaN");
    EXPECT_EQ(add_refusal(model, tolerant(0.0, kNan)), refused + "relative tolerance is NaN");
    EXPECT_EQ(add_refusal(model, tolerant(kSignedNan, 0.0)), refused + "absolute tolerance is NaN");
    EXPECT_EQ(add_refusal(model, tolerant(-1.0, 0.0)),
              refused + "absolute tolerance -1 is negative");
    EXPECT_EQ(add_refusal(model, tolerant(0.0, -kInfinity)),
              refused + "relative tolerance is minus infinity");
    const double least = std::numeric_limits<double>::denorm_min();
    EXPECT_EQ(add_refusal(model, tolerant(0.0, -least)),
              refused + "relative tolerance -5e-324 is negative");
    EXPECT_EQ(model.objective_count(), 0);
    const std::vector<Objective> kept{
        tolerant(kInfinity, kInfinity),
        tolerant(-0.0, -0.0),
        tolerant(least, 0.25),
    };
    model.add_objectives(kept);
    ASSERT_EQ(model.objective_count(), 3);
    for (std::size_t k = 0; k < kept.size(); ++k)
    {
        expect_objective(model.objective(static_cast<Index>(k)), kept[k]);
    }
}

TEST(ObjectiveTest, Order)
{
    Model model = with_functions(1);
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
    EXPECT_EQ(add_refusal(model, found), refused + "function 1 is unknown");
    found.function = 0;
    EXPECT_EQ(add_refusal(model, found), refused + "sense 0 is unknown");
    found.sense = Sense::Maximize;
    EXPECT_EQ(add_refusal(model, found), refused + "weight 0 is not positive");
    found.weight = 1.0;
    EXPECT_EQ(add_refusal(model, found), refused + "absolute tolerance is NaN");
    found.absolute = 0.0;
    EXPECT_EQ(add_refusal(model, found), refused + "relative tolerance is NaN");
    const std::vector<Objective> both{weighted(-1.0), bad};
    EXPECT_EQ(refusal([&] { model.add_objectives(both); }), refused + "weight -1 is not positive");
    EXPECT_EQ(model.objective_count(), 0);
}

TEST(ObjectiveTest, Unchanged)
{
    Model model = with_functions(2);
    add(model, kMaximize);
    const std::vector<Objective> refused{default_objective(0, 1), weighted(kNan)};
    EXPECT_NE(refusal([&] { model.add_objectives(refused); }), "");
    const std::vector<Index> indices{0, 1};
    const std::vector<Objective> changed{default_objective(0, 4), kMaximize};
    EXPECT_NE(refusal([&] { model.set_objectives(indices, changed); }), "");
    const std::vector<Index> again{0, 0};
    const std::vector<Objective> nan{default_objective(0, 4), tolerant(kNan, 0.0)};
    EXPECT_NE(refusal([&] { model.set_objectives(again, nan); }), "");
    ASSERT_EQ(model.objective_count(), 1);
    expect_objective(model.objective(0), kMaximize);
}

TEST(ObjectiveTest, LastWins)
{
    Model model = with_functions(2);
    model.add_objectives(std::vector<Objective>{default_objective(0, 0), default_objective(0, 1)});
    const std::vector<Index> indices{1, 0, 1};
    const std::vector<Objective> changed{kMaximize, tolerant(1.0, 2.0), default_objective(1, 9)};
    model.set_objectives(indices, changed);
    expect_objective(model.objective(0), changed[1]);
    expect_objective(model.objective(1), changed[2]);
}

TEST(ObjectiveTest, BadIndex)
{
    Model model = with_functions(1);
    add(model, default_objective(0, 0));
    EXPECT_EQ(refusal([&] { model.objective(-1); }), "invalid argument: objective -1: unknown");
    EXPECT_EQ(refusal([&] { model.objective(1); }), "invalid argument: objective 1: unknown");
    EXPECT_EQ(set_refusal(model, 1, default_objective(0, 0)),
              "invalid argument: objective 1: unknown");
    EXPECT_EQ(set_refusal(model, 0, weighted(0.0)),
              "invalid argument: objective 0: weight 0 is not positive");
}

TEST(ObjectiveTest, ReadRange)
{
    Model model = with_functions(3);
    const std::vector<Objective> added{default_objective(2, 0), kMaximize, default_objective(0, 2)};
    model.add_objectives(added);
    const std::span<const Objective> middle = model.objectives(1, 2);
    ASSERT_EQ(middle.size(), 2U);
    expect_objective(middle[0], added[1]);
    expect_objective(middle[1], added[2]);
    EXPECT_TRUE(model.objectives(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.objectives(-1, 1); }), "invalid argument: objective -1: unknown");
    EXPECT_EQ(refusal([&] { model.objectives(1, 3); }), "invalid argument: objective 3: unknown");
    EXPECT_EQ(refusal([&] { model.objectives(4, 0); }), "invalid argument: objective 4: unknown");
    EXPECT_EQ(refusal([&] { model.objectives(0, -1); }),
              "invalid argument: argument 'count': -1 is negative");
}

TEST(ObjectiveTest, Mismatch)
{
    Model model = with_functions(1);
    add(model, default_objective(0, 0));
    const std::vector<Index> indices{0};
    failure([&] { model.set_objectives(indices, {}); }, biyos::ErrorKind::Internal);
    expect_objective(model.objective(0), default_objective(0, 0));
}

TEST(ObjectiveTest, Room)
{
    Model model = with_functions(1);
    model.check_objective_room(biyos::kRoom);
    const auto beyond = [&] { model.check_objective_room(biyos::kRoom + 1); };
    EXPECT_EQ(failure(beyond, biyos::ErrorKind::NoMemory),
              "out of memory: model: no objective index left");
    add(model, default_objective(0, 0));
    const auto full = [&] { model.check_objective_room(biyos::kRoom); };
    EXPECT_EQ(failure(full, biyos::ErrorKind::NoMemory),
              "out of memory: model: no objective index left");
    model.check_objective_room(biyos::kRoom - 1);
}

TEST(ObjectiveTest, Moved)
{
    Model model = with_functions(2);
    add(model, kMaximize);
    const Model taken = std::move(model);
    expect_objective(taken.objective(0), kMaximize);
    EXPECT_EQ(model.objective_count(), 0);
    EXPECT_TRUE(model.objectives(0, 0).empty());
    EXPECT_EQ(add_refusal(model, kMaximize),
              "invalid argument: objective 0: function 1 is unknown");
}
