#include "reading.hpp"
#include "samples.hpp"
#include "writing.hpp"

#include "core/env.hpp"
#include "core/index.hpp"
#include "core/settings.hpp"
#include "core/text.hpp"
#include "expr/terms.hpp"
#include "io/files.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

using biyos::Constraint;
using biyos::Family;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Objective;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::continuous;
using biyos::test::kInfinity;
using biyos::test::kMpsFiles;
using biyos::test::printed;

namespace
{

class Generator
{
  public:
    explicit Generator(unsigned seed) : engine_(seed) {}

    int below(int n) { return static_cast<int>(engine_() % static_cast<unsigned>(n)); }

    double number()
    {
        const double whole = below(2001) - 1000;
        return whole / static_cast<double>(below(97) + 1);
    }

    double width() { return std::fabs(number()) + 1.0; }

  private:
    std::mt19937 engine_;
};

double lower_bound(Generator &random)
{
    switch (random.below(3))
    {
    case 0:
        return -kInfinity;
    case 1:
        return 0.0;
    }
    return random.number();
}

double upper_bound(Generator &random, double lower)
{
    switch (random.below(3))
    {
    case 0:
        return kInfinity;
    case 1:
        return std::isinf(lower) ? random.number() : lower;
    }
    return std::isinf(lower) ? random.number() : lower + random.width();
}

Variable semi(Generator &random, VariableKind kind)
{
    const double lower = random.below(2) == 0 ? 0.0 : random.width();
    return Variable{.kind = kind, .lower = lower, .upper = lower + random.width()};
}

Variable variable(Generator &random)
{
    const VariableKind kinds[] = {
        VariableKind::Continuous,
        VariableKind::Integer,
        VariableKind::Binary,
        VariableKind::SemiContinuous,
        VariableKind::SemiInteger,
    };
    const VariableKind kind = kinds[random.below(5)];
    if (kind == VariableKind::Binary)
    {
        return Variable{.kind = kind, .lower = 0.0, .upper = 1.0};
    }
    if (kind == VariableKind::SemiContinuous || kind == VariableKind::SemiInteger)
    {
        return semi(random, kind);
    }
    const double lower = lower_bound(random);
    return Variable{.kind = kind, .lower = lower, .upper = upper_bound(random, lower)};
}

Constraint constraint(Generator &random, Index function)
{
    const double value = random.number();
    switch (random.below(5))
    {
    case 0:
        return Constraint{.function = function, .lower = -kInfinity, .upper = kInfinity};
    case 1:
        return Constraint{.function = function, .lower = -kInfinity, .upper = value};
    case 2:
        return Constraint{.function = function, .lower = value, .upper = kInfinity};
    case 3:
        return Constraint{.function = function, .lower = value, .upper = value};
    }
    return Constraint{.function = function, .lower = value, .upper = value + random.width()};
}

double coefficient(Generator &random)
{
    const double value = random.number();
    return std::fpclassify(value) == FP_ZERO ? 1.0 : value;
}

void add_functions(Model &model, Generator &random, Index n_functions)
{
    std::vector<double> constants;
    std::vector<Offset> starts{0};
    std::vector<LinearTerm> terms;
    for (Index f = 0; f < n_functions; ++f)
    {
        constants.push_back(f == 0 ? random.number() : 0.0);
        for (Index v = 0; v < model.n_variables(); ++v)
        {
            if (random.below(3) == 0)
            {
                terms.push_back(LinearTerm{.variable = v, .coefficient = coefficient(random)});
            }
        }
        starts.push_back(static_cast<Offset>(terms.size()));
    }
    model.add_functions(constants, starts, std::move(terms));
}

void free_constants(Model &model, Generator &random)
{
    for (Index k = 0; k < model.n_constraints(); ++k)
    {
        const Constraint &bounds = model.constraint(k);
        if (std::isinf(bounds.lower) && std::isinf(bounds.upper))
        {
            const Index function = bounds.function;
            const double value = random.number();
            model.set_constants(std::span(&function, 1), std::span(&value, 1));
        }
    }
}

void name_all(Model &model, Family family, std::string_view prefix)
{
    std::vector<std::string> names;
    for (Index k = 0; k < model.n_of(family); ++k)
    {
        names.push_back(std::string(prefix) + biyos::text_of(k));
    }
    const std::vector<std::string_view> views(names.begin(), names.end());
    model.set_names(family, biyos::test::indices(names.size()), views);
}

Model generated(unsigned seed)
{
    Generator random(seed);
    Model model;
    model.set_name("seed " + biyos::text_of(seed));
    std::vector<Variable> variables;
    for (int k = random.below(6) + 1; k > 0; --k)
    {
        variables.push_back(variable(random));
    }
    model.add_variables(variables);
    const Index n_constraints = random.below(6);
    add_functions(model, random, n_constraints + 1);
    std::vector<Constraint> constraints;
    for (Index k = 0; k < n_constraints; ++k)
    {
        constraints.push_back(constraint(random, k + 1));
    }
    model.add_constraints(constraints);
    free_constants(model, random);
    const ObjectiveSense sense =
        random.below(2) == 0 ? ObjectiveSense::Minimize : ObjectiveSense::Maximize;
    biyos::test::add_goal(model, 0, sense, "goal");
    name_all(model, Family::Variable, "x");
    name_all(model, Family::Constraint, "c");
    return model;
}

Model linear(const std::vector<Constraint> &constraints, const std::vector<double> &constants)
{
    Model model;
    model.add_variables(std::vector{continuous(0.0, kInfinity), continuous(-1.0, 2.0)});
    std::vector<Offset> starts{0};
    std::vector<LinearTerm> terms;
    for (std::size_t k = 0; k < constants.size(); ++k)
    {
        terms.push_back(LinearTerm{.variable = 0, .coefficient = 1.0 + static_cast<double>(k)});
        terms.push_back(LinearTerm{.variable = 1, .coefficient = -2.5});
        starts.push_back(static_cast<Offset>(terms.size()));
    }
    model.add_functions(constants, starts, std::move(terms));
    model.add_constraints(constraints);
    return model;
}

Model reread(const Model &model)
{
    biyos::Env env;
    return biyos::test::read_text(env, printed(model));
}

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

const std::string *name_of(const Model &model, Family family, Index index)
{
    return model.name_of(family, index);
}

}

TEST(MpsTest, Samples)
{
    const std::string path = testing::TempDir() + "biyos_written.mps";
    for (const std::string_view text : kMpsFiles)
    {
        const Model read = biyos::test::read_sample(text);
        biyos::Env env;
        biyos::write_file(env, read, biyos::Settings(), path, biyos::FileFormat::Mps);
        const Model back = biyos::read_file(env, path, biyos::FileFormat::Mps);
        EXPECT_EQ(biyos::difference(read, back), "") << text;
    }
}

TEST(MpsTest, Generated)
{
    for (unsigned seed = 0; seed < 64; ++seed)
    {
        const Model model = generated(seed);
        EXPECT_EQ(biyos::difference(model, reread(model)), "") << printed(model);
    }
}

TEST(MpsTest, Printed)
{
    Model model = linear({{.function = 1, .lower = 1.0, .upper = 4.5}}, {0.5, 0.0});
    biyos::test::add_goal(model, 0, ObjectiveSense::Maximize, "cost");
    model.set_name("small one");
    EXPECT_EQ(printed(model),
              "NAME small one\n"
              "OBJSENSE MAX\n"
              "ROWS\n"
              " N  cost\n"
              " G  R0\n"
              "COLUMNS\n"
              "    C0  cost  1\n"
              "    C0  R0  2\n"
              "    C1  cost  -2.5\n"
              "    C1  R0  -2.5\n"
              "RHS\n"
              "    RHS  cost  -0.5\n"
              "    RHS  R0  1\n"
              "RANGES\n"
              "    RNG  R0  3.5\n"
              "BOUNDS\n"
              " LO BND  C1  -1\n"
              " UP BND  C1  2\n"
              "ENDATA\n");
}

TEST(MpsTest, Unnamed)
{
    Model model = linear({{.function = 1, .lower = -kInfinity, .upper = 3.0}}, {0.0, 0.0});
    biyos::test::add_goal(model, 0, ObjectiveSense::Minimize, "C0");
    const Model back = reread(model);
    EXPECT_EQ(*name_of(back, Family::Objective, 0), "C0");
    EXPECT_EQ(*name_of(back, Family::Constraint, 0), "R0");
    EXPECT_EQ(*name_of(back, Family::Variable, 0), "C0");
    EXPECT_EQ(*name_of(back, Family::Variable, 1), "C1");
}

TEST(MpsTest, Suffix)
{
    Model model = linear({{.function = 0, .lower = 0.0, .upper = 0.0},
                          {.function = 1, .lower = 0.0, .upper = 0.0},
                          {.function = 2, .lower = 0.0, .upper = 0.0}},
                         {0.0, 0.0, 0.0});
    const std::vector<std::string_view> names{"R0", "R0_1"};
    model.set_names(Family::Constraint, std::vector<Index>{1, 2}, names);
    const std::vector<std::string_view> columns{"C0"};
    model.set_names(Family::Variable, std::vector<Index>{1}, columns);
    const Model back = reread(model);
    EXPECT_EQ(*name_of(back, Family::Constraint, 0), "R0_2");
    EXPECT_EQ(*name_of(back, Family::Variable, 0), "C0_1");
    EXPECT_EQ(*name_of(back, Family::Variable, 1), "C0");
}

TEST(MpsTest, Constants)
{
    const Model model = linear({{.function = 0, .lower = 1.0, .upper = 5.0},
                                {.function = 1, .lower = -kInfinity, .upper = kInfinity}},
                               {2.0, -3.0});
    const Model back = reread(model);
    EXPECT_EQ(bits(back.constant(1)), bits(0.0));
    EXPECT_EQ(bits(back.constraint(0).lower), bits(-1.0));
    EXPECT_EQ(bits(back.constraint(0).upper), bits(3.0));
    EXPECT_EQ(bits(back.constant(2)), bits(-3.0));
    EXPECT_EQ(bits(back.constraint(1).lower), bits(-kInfinity));
}

TEST(MpsTest, Shared)
{
    Model model = linear({{.function = 0, .lower = -kInfinity, .upper = 1.0},
                          {.function = 0, .lower = 0.0, .upper = kInfinity}},
                         {0.0});
    biyos::test::add_goal(model, 0, ObjectiveSense::Minimize, "cost");
    const Model back = reread(model);
    EXPECT_EQ(back.n_functions(), 3);
    EXPECT_EQ(back.objective(0).function, 0);
    EXPECT_EQ(back.constraint(0).function, 1);
    EXPECT_EQ(back.constraint(1).function, 2);
    EXPECT_EQ(back.linear_terms(1, 1).size(), 2U);
    EXPECT_EQ(back.linear_terms(2, 1).size(), 2U);
}

TEST(MpsTest, Goalless)
{
    Model bounded = linear({{.function = 0, .lower = 0.0, .upper = 1.0}}, {0.0});
    name_all(bounded, Family::Variable, "C");
    name_all(bounded, Family::Constraint, "R");
    EXPECT_EQ(biyos::difference(bounded, reread(bounded)), "");
    const Model free = linear({{.function = 0, .lower = -kInfinity, .upper = kInfinity}}, {0.0});
    const Model back = reread(free);
    ASSERT_EQ(back.n_objectives(), 1);
    EXPECT_EQ(*name_of(back, Family::Objective, 0), "OBJ");
    EXPECT_TRUE(back.linear_terms(back.objective(0).function, 1).empty());
    EXPECT_EQ(bits(back.constant(back.objective(0).function)), bits(0.0));
    EXPECT_EQ(back.n_constraints(), 1);
}

TEST(MpsTest, Nothing)
{
    const Model model;
    EXPECT_EQ(biyos::difference(model, reread(model)), "");
}
