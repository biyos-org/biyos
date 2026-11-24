#include "reading.hpp"
#include "samples.hpp"

#include "core/env.hpp"
#include "core/error.hpp"
#include "io/files.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using biyos::Constraint;
using biyos::LinearTerm;
using biyos::Model;
using biyos::ObjectiveSense;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::continuous;
using biyos::test::kInfinity;

namespace
{

Variable variable(VariableKind kind, double lower, double upper)
{
    return Variable{.kind = kind, .lower = lower, .upper = upper};
}

Constraint row(biyos::Index function, double lower, double upper)
{
    return Constraint{.function = function, .lower = lower, .upper = upper};
}

}

TEST(MpsTest, Linear)
{
    Model wanted;
    wanted.set_name("small");
    biyos::test::add_columns(wanted,
                             {continuous(0.0, kInfinity), continuous(0.0, kInfinity)},
                             {"x", "y"});
    biyos::test::add_rows(wanted,
                          {-4.0, 0.0, 0.0, 0.0},
                          {0, 2, 4, 5, 7},
                          {
                              LinearTerm{.variable = 0, .coefficient = 1.0},
                              LinearTerm{.variable = 1, .coefficient = -3.5},
                              LinearTerm{.variable = 0, .coefficient = 2.0},
                              LinearTerm{.variable = 1, .coefficient = 1.0},
                              LinearTerm{.variable = 1, .coefficient = 1.0},
                              LinearTerm{.variable = 0, .coefficient = 1.0},
                              LinearTerm{.variable = 1, .coefficient = -1.0},
                          });
    biyos::test::add_named(wanted,
                           {row(1, -kInfinity, 10.0), row(2, 2.0, kInfinity), row(3, 0.0, 0.0)},
                           {"limit", "floor", "balance"});
    biyos::test::add_goal(wanted, 0, ObjectiveSense::Minimize, "cost");
    EXPECT_EQ(biyos::difference(biyos::test::read_sample(biyos::test::kLpMpsFile), wanted), "");
}

TEST(MpsTest, Ranges)
{
    Model wanted;
    wanted.set_name("ranges");
    biyos::test::add_columns(wanted, {continuous(0.0, kInfinity)}, {"x"});
    std::vector<LinearTerm> terms(5, LinearTerm{.variable = 0, .coefficient = 1.0});
    biyos::test::add_rows(wanted, {0.0, 0.0, 0.0, 0.0, 0.0}, {0, 1, 2, 3, 4, 5}, terms);
    biyos::test::add_named(wanted,
                           {row(1, 2.5, 4.0), row(2, 1.0, 3.0), row(3, 2.0, 2.5), row(4, 2.5, 3.0)},
                           {"less", "more", "upward", "downward"});
    biyos::test::add_goal(wanted, 0, ObjectiveSense::Minimize, "obj");
    EXPECT_EQ(biyos::difference(biyos::test::read_sample(biyos::test::kRangesMpsFile), wanted), "");
}

TEST(MpsTest, Bounds)
{
    Model wanted;
    wanted.set_name("bounds");
    biyos::test::add_columns(
        wanted,
        {
            continuous(-2.0, kInfinity),
            continuous(0.0, 3.0),
            continuous(1.5, 1.5),
            continuous(-kInfinity, kInfinity),
            continuous(-kInfinity, kInfinity),
            continuous(0.0, kInfinity),
            variable(VariableKind::Binary, 0.0, 1.0),
            variable(VariableKind::Integer, -1.0, kInfinity),
            variable(VariableKind::Integer, 0.0, 7.0),
            variable(VariableKind::SemiContinuous, 0.0, 5.0),
            variable(VariableKind::SemiInteger, 0.0, 9.0),
            continuous(-kInfinity, -4.0),
            continuous(1.0, 2.0),
        },
        {"lo", "up", "fx", "fr", "mi", "pl", "bv", "li", "ui", "sc", "si", "neg", "pair"});
    std::vector<LinearTerm> terms;
    for (biyos::Index k = 0; k < 13; ++k)
    {
        terms.push_back(LinearTerm{.variable = k, .coefficient = 1.0});
    }
    biyos::test::add_rows(wanted, {0.0}, {0, 13}, terms);
    biyos::test::add_goal(wanted, 0, ObjectiveSense::Minimize, "obj");
    EXPECT_EQ(biyos::difference(biyos::test::read_sample(biyos::test::kBoundsMpsFile), wanted), "");
}

TEST(MpsTest, Markers)
{
    Model wanted;
    wanted.set_name("markers");
    biyos::test::add_columns(wanted,
                             {
                                 continuous(0.0, kInfinity),
                                 variable(VariableKind::Integer, 0.0, 1.0),
                                 variable(VariableKind::Integer, 0.0, 8.0),
                                 continuous(1.0, kInfinity),
                             },
                             {"a", "b", "c", "d"});
    std::vector<LinearTerm> terms;
    for (const double coefficient : {1.0, 2.0, 3.0, 4.0})
    {
        terms.push_back(LinearTerm{.variable = static_cast<biyos::Index>(terms.size()),
                                   .coefficient = coefficient});
    }
    for (biyos::Index k = 0; k < 4; ++k)
    {
        terms.push_back(LinearTerm{.variable = k, .coefficient = 1.0});
    }
    biyos::test::add_rows(wanted, {0.0, 0.0}, {0, 4, 8}, terms);
    biyos::test::add_named(wanted, {row(1, -kInfinity, 5.0)}, {"cap"});
    biyos::test::add_goal(wanted, 0, ObjectiveSense::Minimize, "obj");
    EXPECT_EQ(biyos::difference(biyos::test::read_sample(biyos::test::kMarkersMpsFile), wanted),
              "");
}

TEST(MpsTest, Maximize)
{
    Model wanted;
    wanted.set_name("maximize");
    biyos::test::add_columns(wanted, {continuous(0.0, kInfinity)}, {"x"});
    biyos::test::add_rows(wanted,
                          {0.0, 0.0},
                          {0, 1, 2},
                          {
                              LinearTerm{.variable = 0, .coefficient = 2.0},
                              LinearTerm{.variable = 0, .coefficient = 1.0},
                          });
    biyos::test::add_named(wanted, {row(1, -kInfinity, 3.0)}, {"cap"});
    biyos::test::add_goal(wanted, 0, ObjectiveSense::Maximize, "profit");
    EXPECT_EQ(biyos::difference(biyos::test::read_sample(biyos::test::kMaximizeMpsFile), wanted),
              "");
}

TEST(MpsTest, NoObjective)
{
    Model wanted;
    wanted.set_name("constraints");
    biyos::test::add_columns(wanted,
                             {continuous(0.0, kInfinity), continuous(0.0, kInfinity)},
                             {"x", "y"});
    biyos::test::add_rows(wanted,
                          {0.0, 0.0},
                          {0, 2, 3},
                          {
                              LinearTerm{.variable = 0, .coefficient = 1.0},
                              LinearTerm{.variable = 1, .coefficient = 1.0},
                              LinearTerm{.variable = 0, .coefficient = 1.0},
                          });
    biyos::test::add_named(wanted, {row(0, -kInfinity, 4.0), row(1, 1.0, 1.0)}, {"cap", "fix"});
    EXPECT_EQ(biyos::difference(biyos::test::read_sample(biyos::test::kConstraintsMpsFile), wanted),
              "");
}

TEST(MpsTest, FreeRows)
{
    Model wanted;
    biyos::test::add_columns(wanted, {continuous(0.0, kInfinity)}, {"x"});
    biyos::test::add_rows(wanted,
                          {0.0, 0.0, -5.0, 0.0},
                          {0, 1, 2, 3, 4},
                          {
                              LinearTerm{.variable = 0, .coefficient = 1.0},
                              LinearTerm{.variable = 0, .coefficient = 1.0},
                              LinearTerm{.variable = 0, .coefficient = 2.0},
                              LinearTerm{.variable = 0, .coefficient = 3.0},
                          });
    biyos::test::add_named(
        wanted,
        {row(1, -kInfinity, 1.0), row(2, -kInfinity, kInfinity), row(3, -kInfinity, kInfinity)},
        {"cap", "second", "third"});
    biyos::test::add_goal(wanted, 0, ObjectiveSense::Minimize, "first");
    EXPECT_EQ(biyos::difference(biyos::test::read_sample(biyos::test::kRowsMpsFile), wanted), "");
}

TEST(MpsTest, SenseLine)
{
    biyos::Env env;
    const Model model =
        biyos::test::read_text(env,
                               "NAME t\nOBJSENSE MAX\nROWS\n N f\nCOLUMNS\n x f 1\nRHS\nENDATA\n");
    EXPECT_EQ(model.objective(0).sense, ObjectiveSense::Maximize);
}

TEST(MpsTest, ShortSense)
{
    biyos::Env env;
    const Model model =
        biyos::test::read_text(env,
                               "NAME t\nOBJSEN\n MIN\nROWS\n N f\nCOLUMNS\n x f 1\nRHS\nENDATA\n");
    EXPECT_EQ(model.objective(0).sense, ObjectiveSense::Minimize);
}

TEST(MpsTest, Missing)
{
    biyos::Env env;
    const std::string path = testing::TempDir() + "biyos_absent/t.mps";
    try
    {
        biyos::read_file(env, path, biyos::FileFormat::Mps);
        FAIL() << "the file was read";
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), biyos::ErrorKind::FileAccess);
        EXPECT_EQ(error.object(), "file '" + path + "'");
    }
}
