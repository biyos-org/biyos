#include "core/error.hpp"
#include "core/index.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Index;
using biyos::Model;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

Variable continuous(double lower, double upper)
{
    return Variable{.kind = VariableKind::Continuous, .lower = lower, .upper = upper};
}

Variable binary(double lower, double upper)
{
    return Variable{.kind = VariableKind::Binary, .lower = lower, .upper = upper};
}

void expect_same(const Variable &found, const Variable &wanted)
{
    EXPECT_EQ(found.kind, wanted.kind);
    EXPECT_DOUBLE_EQ(found.lower, wanted.lower);
    EXPECT_DOUBLE_EQ(found.upper, wanted.upper);
}

std::string add_refusal(Model &mdl, const Variable &var)
{
    return refusal([&] { mdl.add_vars(std::span(&var, 1)); });
}

std::string set_refusal(Model &mdl, Index idx, const Variable &var)
{
    return refusal([&] { mdl.set_vars(std::span(&idx, 1), std::span(&var, 1)); });
}

std::string range(const std::string &bnds, const std::string &allowed)
{
    return "bounds " + bnds + " are outside " + allowed + ", the widest of its kind";
}

}

TEST(VariableTest, EachKind)
{
    const std::vector<Variable> added{
        continuous(-1.5, 2.5),
        binary(0.0, 1.0),
        Variable{.kind = VariableKind::Integer, .lower = -3.0, .upper = 7.0},
        Variable{.kind = VariableKind::SemiContinuous, .lower = 2.0, .upper = 10.0},
        Variable{.kind = VariableKind::SemiInteger, .lower = 1.0, .upper = 4.0},
    };
    Model mdl;
    mdl.add_vars(added);
    ASSERT_EQ(mdl.n_vars(), 5);
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        expect_same(mdl.variable(static_cast<Index>(k)), added[k]);
    }
}

TEST(VariableTest, BinaryBounds)
{
    const std::vector<Variable> kept{binary(0.0, 1.0),
                                     binary(0.0, 0.0),
                                     binary(1.0, 1.0),
                                     binary(0.25, 0.75)};
    Model mdl;
    mdl.add_vars(kept);
    const std::string refused = "invalid argument: variable 4: ";
    EXPECT_EQ(add_refusal(mdl, binary(-1.0, 1.0)), refused + range("[-1, 1]", "[0, 1]"));
    EXPECT_EQ(add_refusal(mdl, binary(0.0, 2.0)), refused + range("[0, 2]", "[0, 1]"));
    EXPECT_EQ(add_refusal(mdl, binary(-kInfinity, kInfinity)),
              refused + range("[-inf, inf]", "[0, 1]"));
    EXPECT_EQ(mdl.n_vars(), 4);
}

TEST(VariableTest, BinaryChange)
{
    const std::vector<Variable> added{continuous(0.0, 5.0),
                                      continuous(0.0, 1.0),
                                      continuous(-kInfinity, kInfinity)};
    Model mdl;
    mdl.add_vars(added);
    EXPECT_EQ(set_refusal(mdl, 0, binary(0.0, 5.0)),
              "invalid argument: variable 0: " + range("[0, 5]", "[0, 1]"));
    EXPECT_EQ(set_refusal(mdl, 2, binary(-kInfinity, kInfinity)),
              "invalid argument: variable 2: " + range("[-inf, inf]", "[0, 1]"));
    const std::vector<Index> idx{1};
    const std::vector<Variable> changed{binary(0.0, 1.0)};
    mdl.set_vars(idx, changed);
    expect_same(mdl.variable(0), added[0]);
    expect_same(mdl.variable(1), changed[0]);
    expect_same(mdl.variable(2), added[2]);
}

TEST(VariableTest, Infinities)
{
    const std::vector<Variable> kept{continuous(-kInfinity, kInfinity),
                                     continuous(-kInfinity, 0.0),
                                     continuous(0.0, kInfinity)};
    Model mdl;
    mdl.add_vars(kept);
    const std::string refused = "invalid argument: variable 3: ";
    EXPECT_EQ(add_refusal(mdl, continuous(kInfinity, kInfinity)),
              refused + "lower bound is infinity");
    EXPECT_EQ(add_refusal(mdl, continuous(-kInfinity, -kInfinity)),
              refused + "upper bound is minus infinity");
    const Variable integer{.kind = VariableKind::Integer, .lower = kInfinity, .upper = 1.0};
    EXPECT_EQ(add_refusal(mdl, integer), refused + "lower bound is infinity");
    EXPECT_EQ(set_refusal(mdl, 0, continuous(0.0, -kInfinity)),
              "invalid argument: variable 0: upper bound is minus infinity");
    ASSERT_EQ(mdl.n_vars(), 3);
    expect_same(mdl.variable(0), kept[0]);
}

TEST(VariableTest, NanBounds)
{
    Model mdl;
    const std::string refused = "invalid argument: variable 0: ";
    EXPECT_EQ(add_refusal(mdl, continuous(kNan, 1.0)), refused + "lower bound is NaN");
    EXPECT_EQ(add_refusal(mdl, continuous(0.0, kNan)), refused + "upper bound is NaN");
    EXPECT_EQ(add_refusal(mdl, binary(kNan, kNan)), refused + "lower bound is NaN");
    EXPECT_EQ(mdl.n_vars(), 0);
}

TEST(VariableTest, Defaults)
{
    for (const VariableKind knd : biyos::kVariableKinds)
    {
        const Variable free{.kind = knd, .lower = -kInfinity, .upper = kInfinity};
        const Variable wanted = knd == VariableKind::Binary ? binary(0.0, 1.0) : free;
        expect_same(biyos::widest(knd), wanted);
    }
}

TEST(VariableTest, Unchanged)
{
    const std::vector<Variable> first{continuous(0.0, 1.0)};
    Model mdl;
    mdl.add_vars(first);
    const std::vector<Variable> added{continuous(1.0, 2.0), continuous(kNan, 1.0)};
    EXPECT_NE(refusal([&] { mdl.add_vars(added); }), "");
    const std::vector<Index> idx{0, 0};
    const std::vector<Variable> changed{continuous(3.0, 4.0), binary(0.0, 2.0)};
    EXPECT_NE(refusal([&] { mdl.set_vars(idx, changed); }), "");
    const std::vector<Index> unknown{0, 1};
    const std::vector<Variable> valid{continuous(3.0, 4.0), continuous(5.0, 6.0)};
    EXPECT_NE(refusal([&] { mdl.set_vars(unknown, valid); }), "");
    ASSERT_EQ(mdl.n_vars(), 1);
    expect_same(mdl.variable(0), first[0]);
}

TEST(VariableTest, Reversed)
{
    const std::vector<Variable> kept{continuous(5.0, 1.0), binary(1.0, 0.0)};
    Model mdl;
    mdl.add_vars(kept);
    expect_same(mdl.variable(0), kept[0]);
    expect_same(mdl.variable(1), kept[1]);
}

TEST(VariableTest, UnknownKind)
{
    Model mdl;
    const Variable none{.kind = VariableKind{0}, .lower = 0.0, .upper = 1.0};
    const Variable sixth{.kind = VariableKind{6}, .lower = 0.0, .upper = 1.0};
    EXPECT_EQ(add_refusal(mdl, none), "invalid argument: variable 0: kind 0 is unknown");
    EXPECT_EQ(add_refusal(mdl, sixth), "invalid argument: variable 0: kind 6 is unknown");
    EXPECT_EQ(mdl.n_vars(), 0);
}

TEST(VariableTest, BadIndex)
{
    const std::vector<Variable> added{continuous(0.0, 1.0), continuous(0.0, 1.0)};
    Model mdl;
    mdl.add_vars(added);
    EXPECT_EQ(refusal([&] { mdl.variable(-1); }), "invalid argument: variable -1: unknown");
    EXPECT_EQ(refusal([&] { mdl.variable(2); }), "invalid argument: variable 2: unknown");
    EXPECT_EQ(set_refusal(mdl, 2, continuous(0.0, 1.0)), "invalid argument: variable 2: unknown");
}

TEST(VariableTest, LastWins)
{
    const std::vector<Variable> added{continuous(0.0, 1.0), continuous(0.0, 1.0)};
    Model mdl;
    mdl.add_vars(added);
    const std::vector<Index> idx{1, 0, 1};
    const std::vector<Variable> changed{continuous(1.0, 2.0),
                                        continuous(3.0, 4.0),
                                        continuous(5.0, 6.0)};
    mdl.set_vars(idx, changed);
    expect_same(mdl.variable(0), changed[1]);
    expect_same(mdl.variable(1), changed[2]);
}

TEST(VariableTest, ReadRange)
{
    const std::vector<Variable> added{continuous(0.0, 1.0),
                                      continuous(2.0, 3.0),
                                      continuous(4.0, 5.0)};
    Model mdl;
    mdl.add_vars(added);
    EXPECT_EQ(mdl.vars(0, 3).size(), 3U);
    EXPECT_TRUE(mdl.vars(3, 0).empty());
    const std::span<const Variable> middle = mdl.vars(1, 2);
    ASSERT_EQ(middle.size(), 2U);
    expect_same(middle[0], added[1]);
    expect_same(middle[1], added[2]);
    EXPECT_EQ(refusal([&] { mdl.vars(-1, 1); }), "invalid argument: variable -1: unknown");
    EXPECT_EQ(refusal([&] { mdl.vars(1, 3); }), "invalid argument: variable 3: unknown");
    EXPECT_EQ(refusal([&] { mdl.vars(4, 0); }), "invalid argument: variable 4: unknown");
    EXPECT_EQ(refusal([&] { mdl.vars(0, -1); }), "invalid argument: argument 'n': -1 is negative");
}

TEST(VariableTest, Mismatch)
{
    const std::vector<Variable> added{continuous(0.0, 1.0)};
    Model mdl;
    mdl.add_vars(added);
    const std::vector<Index> idx{0, 0};
    try
    {
        mdl.set_vars(idx, added);
        ADD_FAILURE() << "no error";
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), biyos::ErrorKind::Internal);
    }
    expect_same(mdl.variable(0), added[0]);
}
