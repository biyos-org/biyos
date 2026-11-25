#include "core/error.hpp"
#include "core/numbers.hpp"
#include "model/checks.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::Bits;
using biyos::difference;
using biyos::Index;
using biyos::Member;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::SpecialOrderedSetKind;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::failure;
using biyos::test::refusal;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kNan = std::numeric_limits<Real>::quiet_NaN();

struct Given
{
    std::vector<SpecialOrderedSetKind> kinds;
    std::vector<Offset> starts;
    std::vector<Member> members;
};

Model with_variables(Index n)
{
    Model model;
    const Variable free = biyos::widest(VariableKind::Continuous);
    model.add_variables(std::vector<Variable>(static_cast<std::size_t>(n), free));
    return model;
}

Given one(SpecialOrderedSetKind kind, std::vector<Member> members)
{
    const auto size = static_cast<Offset>(members.size());
    return Given{
        .kinds = {kind},
        .starts = {0, size},
        .members = std::move(members),
    };
}

void add(Model &model, const Given &given)
{
    model.add_special_ordered_sets(given.kinds, given.starts, given.members);
}

std::string add_refusal(Model &model, const Given &given)
{
    return refusal([&] { add(model, given); });
}

Bits bits_of(Real value)
{
    return std::bit_cast<Bits>(value);
}

void expect_members(std::span<const Member> found, const std::vector<Member> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_EQ(found[k].variable, wanted[k].variable);
        EXPECT_EQ(bits_of(found[k].weight), bits_of(wanted[k].weight)) << found[k].weight;
    }
}

const std::vector<Member> kFirst{
    {.variable = 2, .weight = 3.0},
    {.variable = 0, .weight = -1.5},
    {.variable = 3, .weight = 2.0},
};

const std::vector<Member> kSecond{
    {.variable = 1, .weight = 0.0},
    {.variable = 0, .weight = 1.0},
};

const Given kBoth{
    .kinds = {SpecialOrderedSetKind::One, SpecialOrderedSetKind::Two},
    .starts = {0, 3, 5},
    .members =
        {
            {.variable = 2, .weight = 3.0},
            {.variable = 0, .weight = -1.5},
            {.variable = 3, .weight = 2.0},
            {.variable = 1, .weight = 0.0},
            {.variable = 0, .weight = 1.0},
        },
};

}

TEST(SpecialOrderedSetTest, Kinds)
{
    Model model = with_variables(4);
    add(model, kBoth);
    ASSERT_EQ(model.n_special_ordered_sets(), 2);
    EXPECT_EQ(model.kind_of_special_ordered_set(0), SpecialOrderedSetKind::One);
    EXPECT_EQ(model.kind_of_special_ordered_set(1), SpecialOrderedSetKind::Two);
    expect_members(model.members(0, 1), kFirst);
    expect_members(model.members(1, 2), kSecond);
    EXPECT_EQ(static_cast<int>(SpecialOrderedSetKind::One), 1);
    EXPECT_EQ(static_cast<int>(SpecialOrderedSetKind::Two), 2);
}

TEST(SpecialOrderedSetTest, Order)
{
    Model model = with_variables(4);
    add(model, one(SpecialOrderedSetKind::Two, kFirst));
    add(model, one(SpecialOrderedSetKind::Two, kSecond));
    expect_members(model.members(0, 1), kFirst);
    expect_members(model.members(1, 2), kSecond);
    std::vector<Member> both = kFirst;
    both.insert(both.end(), kSecond.begin(), kSecond.end());
    expect_members(model.members(0, 2), both);
}

TEST(SpecialOrderedSetTest, Shared)
{
    Model model = with_variables(4);
    add(model, kBoth);
    add(model, one(SpecialOrderedSetKind::One, {{.variable = 0, .weight = 1.0}}));
    EXPECT_EQ(model.members(0, 1)[1].variable, 0);
    EXPECT_EQ(model.members(1, 2)[1].variable, 0);
    EXPECT_EQ(model.members(2, 3)[0].variable, 0);
}

TEST(SpecialOrderedSetTest, Empty)
{
    Model model = with_variables(1);
    const Given none{
        .kinds = {},
        .starts = {0},
        .members = {},
    };
    add(model, none);
    EXPECT_EQ(model.n_special_ordered_sets(), 0);
    add(model, one(SpecialOrderedSetKind::One, {}));
    add(model, one(SpecialOrderedSetKind::Two, {{.variable = 0, .weight = 0.0}}));
    ASSERT_EQ(model.n_special_ordered_sets(), 2);
    EXPECT_TRUE(model.members(0, 1).empty());
    EXPECT_EQ(model.members(1, 2).size(), 1U);
    EXPECT_TRUE(model.members(2, 2).empty());
}

TEST(SpecialOrderedSetTest, EqualWeights)
{
    Model model = with_variables(4);
    const std::vector<Member> equal{
        {.variable = 0, .weight = 2.0},
        {.variable = 1, .weight = 1.0},
        {.variable = 2, .weight = 2.0},
    };
    EXPECT_EQ(add_refusal(model, one(SpecialOrderedSetKind::Two, equal)),
              "invalid argument: set 0: weight 2 is repeated");
    const std::vector<Member> zeros{
        {.variable = 0, .weight = 0.0},
        {.variable = 1, .weight = -0.0},
    };
    EXPECT_EQ(add_refusal(model, one(SpecialOrderedSetKind::One, zeros)),
              "invalid argument: set 0: weight 0 is repeated");
    const std::vector<Member> close{
        {.variable = 0, .weight = 1.0},
        {.variable = 1, .weight = std::nextafter(1.0, 2.0)},
    };
    add(model, one(SpecialOrderedSetKind::One, close));
    expect_members(model.members(0, 1), close);
}

TEST(SpecialOrderedSetTest, Repeated)
{
    Model model = with_variables(4);
    const std::vector<Member> twice{
        {.variable = 3, .weight = 1.0},
        {.variable = 1, .weight = 2.0},
        {.variable = 3, .weight = 3.0},
    };
    EXPECT_EQ(add_refusal(model, one(SpecialOrderedSetKind::One, twice)),
              "invalid argument: set 0: variable 3 is repeated");
    EXPECT_EQ(model.n_special_ordered_sets(), 0);
}

TEST(SpecialOrderedSetTest, UnknownKind)
{
    Model model = with_variables(1);
    EXPECT_EQ(add_refusal(model, one(static_cast<SpecialOrderedSetKind>(3), {})),
              "invalid argument: set 0: kind 3 is unknown");
    EXPECT_EQ(add_refusal(model, one(static_cast<SpecialOrderedSetKind>(0), {})),
              "invalid argument: set 0: kind 0 is unknown");
}

TEST(SpecialOrderedSetTest, UnknownVariable)
{
    Model model = with_variables(2);
    EXPECT_EQ(add_refusal(model, one(SpecialOrderedSetKind::One, {{.variable = 2, .weight = 1.0}})),
              "invalid argument: set 0: variable 2 is unknown");
    EXPECT_EQ(
        add_refusal(model, one(SpecialOrderedSetKind::One, {{.variable = -1, .weight = 1.0}})),
        "invalid argument: set 0: variable -1 is unknown");
}

TEST(SpecialOrderedSetTest, BadWeights)
{
    Model model = with_variables(2);
    EXPECT_EQ(
        add_refusal(model, one(SpecialOrderedSetKind::One, {{.variable = 1, .weight = kNan}})),
        "invalid argument: set 0: weight of variable 1 is NaN");
    EXPECT_EQ(
        add_refusal(model, one(SpecialOrderedSetKind::Two, {{.variable = 0, .weight = kInfinity}})),
        "invalid argument: set 0: weight of variable 0 is infinity");
    EXPECT_EQ(add_refusal(model,
                          one(SpecialOrderedSetKind::Two, {{.variable = 0, .weight = -kInfinity}})),
              "invalid argument: set 0: weight of variable 0 is minus infinity");
}

TEST(SpecialOrderedSetTest, Atomic)
{
    Model model = with_variables(4);
    add(model, kBoth);
    Given given = kBoth;
    given.members[4].variable = 1;
    EXPECT_EQ(add_refusal(model, given), "invalid argument: set 3: variable 1 is repeated");
    ASSERT_EQ(model.n_special_ordered_sets(), 2);
    expect_members(model.members(0, 2), kBoth.members);
}

TEST(SpecialOrderedSetTest, ReadRange)
{
    Model model = with_variables(4);
    add(model, kBoth);
    EXPECT_EQ(refusal([&] { model.kind_of_special_ordered_set(-1); }),
              "invalid argument: set -1: unknown");
    EXPECT_EQ(refusal([&] { model.kind_of_special_ordered_set(2); }),
              "invalid argument: set 2: unknown");
    EXPECT_EQ(refusal([&] { model.members(-1, 0); }), "invalid argument: set -1: unknown");
    EXPECT_EQ(refusal([&] { model.members(1, 3); }), "invalid argument: set 2: unknown");
    EXPECT_EQ(refusal([&] { model.members(3, 3); }), "invalid argument: set 3: unknown");
    EXPECT_EQ(refusal([&] { model.members(0, -1); }),
              "invalid argument: argument 'end': -1 is below 'beg' 0");
}

TEST(SpecialOrderedSetTest, Mismatch)
{
    Model model = with_variables(1);
    Given given = one(SpecialOrderedSetKind::One, {{.variable = 0, .weight = 1.0}});
    given.starts = {0, 2};
    failure([&] { add(model, given); }, biyos::ErrorKind::Internal);
    given.starts = {0};
    failure([&] { add(model, given); }, biyos::ErrorKind::Internal);
    EXPECT_EQ(model.n_special_ordered_sets(), 0);
}

TEST(SpecialOrderedSetTest, Room)
{
    Model model = with_variables(1);
    model.check_special_ordered_set_room(biyos::kRoom, 0);
    const auto beyond = [&] { model.check_special_ordered_set_room(biyos::kRoom + 1, 0); };
    EXPECT_EQ(failure(beyond, biyos::ErrorKind::NoMemory),
              "out of memory: model: no set index left");
    const Offset most = (std::numeric_limits<Offset>::max)();
    const auto members = [&] { model.check_special_ordered_set_room(0, most); };
    EXPECT_EQ(failure(members, biyos::ErrorKind::NoMemory),
              "out of memory: model: no member offset left");
    add(model, one(SpecialOrderedSetKind::One, {}));
    const auto full = [&] { model.check_special_ordered_set_room(biyos::kRoom, 0); };
    EXPECT_EQ(failure(full, biyos::ErrorKind::NoMemory), "out of memory: model: no set index left");
}

TEST(SpecialOrderedSetTest, Moved)
{
    Model model = with_variables(4);
    add(model, kBoth);
    const Model taken = std::move(model);
    EXPECT_EQ(taken.n_special_ordered_sets(), 2);
    expect_members(taken.members(1, 2), kSecond);
    EXPECT_EQ(model.n_special_ordered_sets(), 0);
    EXPECT_TRUE(model.members(0, 0).empty());
}

TEST(SpecialOrderedSetTest, Copied)
{
    Model model = with_variables(4);
    add(model, kBoth);
    Model copy = model;
    EXPECT_EQ(difference(model, copy), "");
    add(copy, one(SpecialOrderedSetKind::Two, kSecond));
    EXPECT_EQ(difference(model, copy), "set count 2 against 3");
    EXPECT_EQ(model.n_special_ordered_sets(), 2);
    EXPECT_EQ(copy.n_special_ordered_sets(), 3);
    EXPECT_EQ(copy.kind_of_special_ordered_set(1), model.kind_of_special_ordered_set(1));
    expect_members(copy.members(0, 2), kBoth.members);
}
