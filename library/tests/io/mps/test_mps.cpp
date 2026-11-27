#include "core/numbers.hpp"
#include "reading.hpp"
#include "samples.hpp"

#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "io/files.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using biyos::Constraint;
using biyos::difference;
using biyos::Env;
using biyos::Error;
using biyos::ErrorKind;
using biyos::FileFormat;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::ObjectiveSense;
using biyos::read_file;
using biyos::Real;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::add_columns;
using biyos::test::add_goal;
using biyos::test::add_named;
using biyos::test::add_rows;
using biyos::test::continuous;
using biyos::test::kBoundsMpsFile;
using biyos::test::kConstraintsMpsFile;
using biyos::test::kInfinity;
using biyos::test::kLpMpsFile;
using biyos::test::kMarkersMpsFile;
using biyos::test::kMaximizeMpsFile;
using biyos::test::kRangesMpsFile;
using biyos::test::kRowsMpsFile;
using biyos::test::read_sample;
using biyos::test::read_text;

namespace
{

Variable variable(VariableKind kind, Real lower, Real upper)
{
    return Variable{.kind = kind, .lower = lower, .upper = upper};
}

Constraint row(Index function, Real lower, Real upper)
{
    return Constraint{.function = function, .lower = lower, .upper = upper};
}

}

TEST(MpsTest, Linear)
{
    Model wanted;
    wanted.set_name("small");
    add_columns(wanted, {continuous(0.0, kInfinity), continuous(0.0, kInfinity)}, {"x", "y"});
    add_rows(wanted,
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
    add_named(wanted,
              {row(1, -kInfinity, 10.0), row(2, 2.0, kInfinity), row(3, 0.0, 0.0)},
              {"limit", "floor", "balance"});
    add_goal(wanted, 0, ObjectiveSense::Minimize, "cost");
    EXPECT_EQ(difference(read_sample(kLpMpsFile), wanted), "");
}

TEST(MpsTest, Ranges)
{
    Model wanted;
    wanted.set_name("ranges");
    add_columns(wanted, {continuous(0.0, kInfinity)}, {"x"});
    std::vector<LinearTerm> terms(5, LinearTerm{.variable = 0, .coefficient = 1.0});
    add_rows(wanted, {0.0, 0.0, 0.0, 0.0, 0.0}, {0, 1, 2, 3, 4, 5}, terms);
    add_named(wanted,
              {row(1, 2.5, 4.0), row(2, 1.0, 3.0), row(3, 2.0, 2.5), row(4, 2.5, 3.0)},
              {"less", "more", "upward", "downward"});
    add_goal(wanted, 0, ObjectiveSense::Minimize, "obj");
    EXPECT_EQ(difference(read_sample(kRangesMpsFile), wanted), "");
}

TEST(MpsTest, Bounds)
{
    Model wanted;
    wanted.set_name("bounds");
    add_columns(wanted,
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
    for (Index k = 0; k < 13; ++k)
    {
        terms.push_back(LinearTerm{.variable = k, .coefficient = 1.0});
    }
    add_rows(wanted, {0.0}, {0, 13}, terms);
    add_goal(wanted, 0, ObjectiveSense::Minimize, "obj");
    EXPECT_EQ(difference(read_sample(kBoundsMpsFile), wanted), "");
}

TEST(MpsTest, Markers)
{
    Model wanted;
    wanted.set_name("markers");
    add_columns(wanted,
                {
                    continuous(0.0, kInfinity),
                    variable(VariableKind::Integer, 0.0, 1.0),
                    variable(VariableKind::Integer, 0.0, 8.0),
                    continuous(1.0, kInfinity),
                },
                {"a", "b", "c", "d"});
    std::vector<LinearTerm> terms;
    for (const Real coefficient : {1.0, 2.0, 3.0, 4.0})
    {
        terms.push_back(
            LinearTerm{.variable = static_cast<Index>(terms.size()), .coefficient = coefficient});
    }
    for (Index k = 0; k < 4; ++k)
    {
        terms.push_back(LinearTerm{.variable = k, .coefficient = 1.0});
    }
    add_rows(wanted, {0.0, 0.0}, {0, 4, 8}, terms);
    add_named(wanted, {row(1, -kInfinity, 5.0)}, {"cap"});
    add_goal(wanted, 0, ObjectiveSense::Minimize, "obj");
    EXPECT_EQ(difference(read_sample(kMarkersMpsFile), wanted), "");
}

TEST(MpsTest, Maximize)
{
    Model wanted;
    wanted.set_name("maximize");
    add_columns(wanted, {continuous(0.0, kInfinity)}, {"x"});
    add_rows(wanted,
             {0.0, 0.0},
             {0, 1, 2},
             {
                 LinearTerm{.variable = 0, .coefficient = 2.0},
                 LinearTerm{.variable = 0, .coefficient = 1.0},
             });
    add_named(wanted, {row(1, -kInfinity, 3.0)}, {"cap"});
    add_goal(wanted, 0, ObjectiveSense::Maximize, "profit");
    EXPECT_EQ(difference(read_sample(kMaximizeMpsFile), wanted), "");
}

TEST(MpsTest, NoObjective)
{
    Model wanted;
    wanted.set_name("constraints");
    add_columns(wanted, {continuous(0.0, kInfinity), continuous(0.0, kInfinity)}, {"x", "y"});
    add_rows(wanted,
             {0.0, 0.0},
             {0, 2, 3},
             {
                 LinearTerm{.variable = 0, .coefficient = 1.0},
                 LinearTerm{.variable = 1, .coefficient = 1.0},
                 LinearTerm{.variable = 0, .coefficient = 1.0},
             });
    add_named(wanted, {row(0, -kInfinity, 4.0), row(1, 1.0, 1.0)}, {"cap", "fix"});
    EXPECT_EQ(difference(read_sample(kConstraintsMpsFile), wanted), "");
}

TEST(MpsTest, FreeRows)
{
    Model wanted;
    add_columns(wanted, {continuous(0.0, kInfinity)}, {"x"});
    add_rows(wanted,
             {0.0, 0.0, -5.0, 0.0},
             {0, 1, 2, 3, 4},
             {
                 LinearTerm{.variable = 0, .coefficient = 1.0},
                 LinearTerm{.variable = 0, .coefficient = 1.0},
                 LinearTerm{.variable = 0, .coefficient = 2.0},
                 LinearTerm{.variable = 0, .coefficient = 3.0},
             });
    add_named(
        wanted,
        {row(1, -kInfinity, 1.0), row(2, -kInfinity, kInfinity), row(3, -kInfinity, kInfinity)},
        {"cap", "second", "third"});
    add_goal(wanted, 0, ObjectiveSense::Minimize, "first");
    EXPECT_EQ(difference(read_sample(kRowsMpsFile), wanted), "");
}

TEST(MpsTest, SenseLine)
{
    Env env;
    const Model model =
        read_text(env, "NAME t\nOBJSENSE MAX\nROWS\n N f\nCOLUMNS\n x f 1\nRHS\nENDATA\n");
    EXPECT_EQ(model.objective(0).sense, ObjectiveSense::Maximize);
}

TEST(MpsTest, ShortSense)
{
    Env env;
    const Model model =
        read_text(env, "NAME t\nOBJSEN\n MIN\nROWS\n N f\nCOLUMNS\n x f 1\nRHS\nENDATA\n");
    EXPECT_EQ(model.objective(0).sense, ObjectiveSense::Minimize);
}

TEST(MpsTest, Missing)
{
    Env env;
    const std::string path = testing::TempDir() + "biyos_absent/t.mps";
    try
    {
        read_file(env, path, FileFormat::Mps);
        FAIL() << "the file was read";
    }
    catch (const Error &error)
    {
        EXPECT_EQ(error.kind(), ErrorKind::FileAccess);
        EXPECT_EQ(error.object(), "file '" + path + "'");
    }
}
