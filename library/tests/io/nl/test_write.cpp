#include "building.hpp"
#include "files.hpp"
#include "loading.hpp"
#include "written.hpp"

#include "core/env/env.hpp"
#include "core/numbers.hpp"
#include "core/options/settings.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/node.hpp"
#include "io/files.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::Constraint;
using biyos::default_objective;
using biyos::difference;
using biyos::Env;
using biyos::FileFormat;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Objective;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::Operator;
using biyos::QuadraticTerm;
using biyos::read_file;
using biyos::Real;
using biyos::Settings;
using biyos::Variable;
using biyos::VariableKind;
using biyos::write_file;
using biyos::test::built;
using biyos::test::nl::add_goal;
using biyos::test::nl::continuous;
using biyos::test::nl::count_of;
using biyos::test::nl::expect_kept;
using biyos::test::nl::kDepth;
using biyos::test::nl::kInfinity;
using biyos::test::nl::kKindsNlFile;
using biyos::test::nl::kLinearNlFile;
using biyos::test::nl::kNonlinearNlFile;
using biyos::test::nl::kObjectivesNlFile;
using biyos::test::nl::kOrderNlFile;
using biyos::test::nl::less_of;
using biyos::test::nl::log10_of;
using biyos::test::nl::node;
using biyos::test::nl::outside_of;
using biyos::test::nl::pieces_of;
using biyos::test::nl::printed;
using biyos::test::nl::read_sample;
using biyos::test::nl::reread;
using biyos::test::nl::square_of;
using biyos::test::nl::sum_of;
using biyos::test::nl::tiny_wanted;
using biyos::test::nl::window_of;
using biyos::test::nl::writable;

namespace
{

constexpr std::array kPoints{-1.5, -0.25, 0.5, 3.0};
constexpr std::array kUnary{
    Operator::Negation,
    Operator::Sin,
    Operator::Cos,
    Operator::Atan,
    Operator::Tanh,
    Operator::Abs,
    Operator::Asinh,
};
constexpr std::array kListed{Operator::Max, Operator::Min};
constexpr unsigned kSeeds = 64;
constexpr std::string_view kNegatedNlFile = "g3 1 1 0\n"
                                            " 1 1 0 0 0\n"
                                            " 1 0\n"
                                            " 0 0\n"
                                            " 1 0 0\n"
                                            " 0 0 0 1\n"
                                            " 0 0 0 0 0\n"
                                            " 1 0\n"
                                            " 0 0\n"
                                            " 0 0 0 0 0\n"
                                            "C0\n"
                                            "o16\n"
                                            "o41\n"
                                            "v0\n"
                                            "r\n"
                                            "1 2\n"
                                            "b\n"
                                            "3\n"
                                            "k0\n"
                                            "J0 1\n"
                                            "0 0\n";
constexpr std::string_view kScaledNlFile = "g3 1 1 0\n"
                                           " 1 1 0 0 0\n"
                                           " 1 0\n"
                                           " 0 0\n"
                                           " 1 0 0\n"
                                           " 0 0 0 1\n"
                                           " 0 0 0 0 0\n"
                                           " 1 0\n"
                                           " 0 0\n"
                                           " 0 0 0 0 0\n"
                                           "C0\n"
                                           "o0\n"
                                           "o2\n"
                                           "n2\n"
                                           "o41\n"
                                           "v0\n"
                                           "o2\n"
                                           "n3\n"
                                           "o42\n"
                                           "v0\n"
                                           "r\n"
                                           "1 2\n"
                                           "b\n"
                                           "3\n"
                                           "k0\n"
                                           "J0 1\n"
                                           "0 0\n";

using Build = Offset (*)(Model &, Offset);

void expect_points(const Model &model)
{
    for (const Real x : kPoints)
    {
        expect_kept(model, std::vector{x});
    }
}

Offset power_of(Model &model, Offset x)
{
    const Offset three = node(model, Operator::Constant, {}, {3.0});
    return node(model, Operator::Power, {x, three}, {});
}

Offset shared_of(Model &model, Offset x)
{
    const Offset sine = node(model, Operator::Sin, {x}, {});
    const Offset cosine = node(model, Operator::Cos, {sine}, {});
    return node(model, Operator::Product, {sine, cosine}, {2.0});
}

Offset lifted_of(Model &model, Offset x)
{
    const std::vector<Real> xs{1.0, 2.0, 4.0};
    const std::vector<Real> ys{3.0, 5.0, 3.0};
    return model.add_piecewise(model.add_piecewise_linear(xs, ys), x);
}

Offset spread_of(Model &model, Offset x)
{
    const Offset sine = node(model, Operator::Sin, {x}, {});
    return node(model, Operator::Sum, {x, sine}, {0.5, 2.0, -1.0});
}

Offset deep_of(Model &model, Offset x)
{
    Offset top = x;
    for (Index k = 0; k < kDepth; ++k)
    {
        top = node(model, Operator::Sin, {top}, {});
    }
    return top;
}

class Generator
{
  public:
    explicit Generator(unsigned seed) : engine_(seed) {}

    std::size_t below(std::size_t n) { return static_cast<std::size_t>(engine_() % n); }

    Index index_below(Index n) { return static_cast<Index>(below(biyos::to_size(n))); }

    Real number()
    {
        const Real whole = static_cast<Real>(below(2001)) - 1000.0;
        return whole / static_cast<Real>(below(97) + 1);
    }

    Real width() { return static_cast<Real>(below(9) + 1) / 4.0; }

  private:
    std::mt19937 engine_;
};

Variable variable(Generator &random)
{
    switch (random.below(4))
    {
    case 0:
        return Variable{.kind = VariableKind::Binary, .lower = 0.0, .upper = 1.0};
    case 1:
        return Variable{.kind = VariableKind::Integer, .lower = -3.0, .upper = 5.0};
    case 2:
        return continuous(-kInfinity, kInfinity);
    }
    const Real lower = random.number();
    return continuous(lower, lower + random.width());
}

Constraint constraint(Generator &random, Index function)
{
    const Real value = random.number();
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

Offset pick(Generator &random, const std::vector<Offset> &nodes)
{
    return nodes[random.below(nodes.size())];
}

Offset grown(Model &model, Generator &random, const std::vector<Offset> &nodes)
{
    const Offset a = pick(random, nodes);
    const Offset b = pick(random, nodes);
    switch (random.below(7))
    {
    case 0:
        return node(model, kUnary[random.below(kUnary.size())], {a}, {});
    case 1:
        return node(model, Operator::ConstantPower, {a}, {static_cast<Real>(random.below(3) + 1)});
    case 2:
        return power_of(model, a);
    case 3:
        return node(model, Operator::Sum, {a, b}, {random.number(), random.number(), 1.0});
    case 4:
        return node(model, Operator::Product, {a, b}, {random.number()});
    case 5:
        return node(model, kListed[random.below(kListed.size())], {a, b}, {});
    }
    return random.below(2) == 0 ? pieces_of(model, a) : lifted_of(model, a);
}

std::vector<Offset> grow(Model &model, Generator &random)
{
    std::vector<Offset> nodes;
    for (Index k = 0; k < model.n_variables(); ++k)
    {
        nodes.push_back(model.add_variable_node(k));
    }
    const std::size_t count = nodes.size() + random.below(10);
    while (nodes.size() < count)
    {
        nodes.push_back(grown(model, random, nodes));
    }
    return nodes;
}

void add_functions(Model &model, Generator &random, Index n_functions)
{
    std::vector<Real> constants;
    std::vector<Offset> starts{0};
    std::vector<LinearTerm> terms;
    for (Index f = 0; f < n_functions; ++f)
    {
        constants.push_back(random.number());
        for (Index v = 0; v < model.n_variables(); ++v)
        {
            if (random.below(3) == 0)
            {
                terms.push_back(LinearTerm{.variable = v, .coefficient = random.number()});
            }
        }
        starts.push_back(biyos::to_offset(terms.size()));
    }
    model.add_functions(constants, starts, std::move(terms));
}

void add_squares(Model &model, Generator &random)
{
    std::vector<Index> functions;
    std::vector<Offset> starts{0};
    std::vector<QuadraticTerm> terms;
    for (Index f = 0; f < model.n_functions(); ++f)
    {
        if (random.below(3) == 0)
        {
            const Index row = random.index_below(model.n_variables());
            const Index column = random.index_below(row + 1);
            functions.push_back(f);
            terms.push_back(QuadraticTerm{.row = row, .column = column, .coefficient = 1.5});
            starts.push_back(biyos::to_offset(terms.size()));
        }
    }
    model.add_quadratic_terms(functions, starts, std::move(terms));
}

void add_roots(Model &model, Generator &random)
{
    const std::vector<Offset> nodes = grow(model, random);
    std::vector<Index> functions;
    std::vector<Offset> roots;
    for (Index f = 0; f < model.n_functions(); ++f)
    {
        if (random.below(3) != 0)
        {
            functions.push_back(f);
            roots.push_back(pick(random, nodes));
        }
    }
    model.attach_expressions(functions, roots);
}

Index function_of(Generator &random, Index own)
{
    return random.below(4) == 0 ? 0 : own;
}

void add_records(Model &model, Generator &random, Index n_constraints)
{
    std::vector<Constraint> constraints;
    for (Index k = 0; k < n_constraints; ++k)
    {
        constraints.push_back(constraint(random, function_of(random, k)));
    }
    model.add_constraints(constraints);
    std::vector<Objective> objectives;
    for (Index k = n_constraints; k < model.n_functions() - 1; ++k)
    {
        Objective objective = default_objective(function_of(random, k), 0);
        objective.sense =
            random.below(2) == 0 ? ObjectiveSense::Minimize : ObjectiveSense::Maximize;
        objective.priority = static_cast<int>(objectives.size());
        objectives.push_back(objective);
    }
    model.add_objectives(objectives);
}

Model generated(unsigned seed)
{
    Generator random(seed);
    Model model;
    std::vector<Variable> variables;
    const std::size_t n_variables = random.below(5) + 1;
    while (variables.size() < n_variables)
    {
        variables.push_back(variable(random));
    }
    model.add_variables(variables);
    const Index n_constraints = random.index_below(4);
    add_functions(model, random, n_constraints + random.index_below(3) + 1);
    add_squares(model, random);
    add_roots(model, random);
    add_records(model, random, n_constraints);
    return model;
}

std::vector<Real> point(const Model &model, Real x)
{
    std::vector<Real> values;
    for (Index k = 0; k < model.n_variables(); ++k)
    {
        values.push_back(x + 0.375 * static_cast<Real>(k));
    }
    return values;
}

}

TEST(NlTest, Samples)
{
    for (const std::string_view text : {kLinearNlFile, kNonlinearNlFile, kObjectivesNlFile})
    {
        const Model model = read_sample(text);
        EXPECT_EQ(difference(model, reread(model)), "") << printed(model);
    }
}

TEST(NlTest, Bare)
{
    for (const std::string_view text : {kNegatedNlFile, kScaledNlFile})
    {
        const Model model = read_sample(text);
        EXPECT_EQ(difference(model, reread(model)), "") << printed(model);
    }
}

TEST(NlTest, Permuted)
{
    for (const std::string_view text : {kKindsNlFile, kOrderNlFile})
    {
        const Model back = reread(read_sample(text));
        EXPECT_EQ(difference(back, reread(back)), "") << printed(back);
    }
}

TEST(NlTest, Spread)
{
    const Model model = built(writable());
    EXPECT_EQ(reread(model).n_functions(), model.n_constraints() + model.n_objectives());
    for (const Real x : kPoints)
    {
        expect_kept(model, point(model, x));
    }
}

TEST(NlTest, Forms)
{
    const std::vector<Build> builds{
        square_of,
        log10_of,
        less_of,
        pieces_of,
        sum_of,
        power_of,
        shared_of,
        lifted_of,
        spread_of,
        window_of,
        outside_of,
    };
    for (const Build build : builds)
    {
        expect_points(tiny_wanted(build));
    }
}

TEST(NlTest, Chain)
{
    const Model back = reread(tiny_wanted(deep_of));
    EXPECT_EQ(count_of(back, Operator::Sin), kDepth);
}

TEST(NlTest, Generated)
{
    for (unsigned seed = 0; seed < kSeeds; ++seed)
    {
        const Model model = generated(seed);
        for (const Real x : kPoints)
        {
            expect_kept(model, point(model, x));
        }
    }
}

TEST(NlTest, File)
{
    const std::string path = testing::TempDir() + "biyos_written.nl";
    const Model model = read_sample(kNonlinearNlFile);
    Env env;
    write_file(env.stream(), model, Settings(), path, FileFormat::Nl);
    EXPECT_EQ(difference(model, read_file(env, path, FileFormat::Nl)), "");
}

TEST(NlTest, Printed)
{
    Model model;
    model.add_variables(std::vector{continuous(0.0, 0.1), continuous(-kInfinity, 3.0)});
    model.add_functions(std::vector{1.0 / 3.0, 0.0},
                        std::vector<Offset>{0, 1, 2},
                        {
                            LinearTerm{.variable = 1, .coefficient = 2.0},
                            LinearTerm{.variable = 0, .coefficient = -0.5},
                        });
    const Offset x = model.add_variable_node(0);
    const Offset root = node(model, Operator::Exp, {x}, {});
    const Index function = 0;
    model.attach_expressions(std::span(&function, 1), std::span(&root, 1));
    const Constraint cap{.function = 0, .lower = 1.0, .upper = 2.0};
    model.add_constraints(std::span(&cap, 1));
    add_goal(model, 1, ObjectiveSense::Maximize);
    EXPECT_EQ(printed(model),
              "g3 1 1 0\n"
              " 2 1 1 1 0\n"
              " 1 0\n"
              " 0 0\n"
              " 1 0 0\n"
              " 0 0 0 1\n"
              " 0 0 0 0 0\n"
              " 2 1\n"
              " 0 0\n"
              " 0 0 0 0 0\n"
              "C0\n"
              "o0\n"
              "n0.3333333333333333\n"
              "o44\n"
              "v0\n"
              "O0 1\n"
              "n0\n"
              "r\n"
              "0 1 2\n"
              "b\n"
              "0 0 0.1\n"
              "1 3\n"
              "k1\n"
              "1\n"
              "J0 2\n"
              "0 0\n"
              "1 2\n"
              "G0 1\n"
              "0 -0.5\n");
}
