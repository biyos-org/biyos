#include "files.hpp"
#include "loading.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::Constraint;
using biyos::default_objective;
using biyos::difference;
using biyos::Index;
using biyos::kNoRoot;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Objective;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::nl::kDefinedHeader;
using biyos::test::nl::kKindsNlFile;
using biyos::test::nl::kLinearNlFile;
using biyos::test::nl::kNonlinearNlFile;
using biyos::test::nl::kObjectivesNlFile;
using biyos::test::nl::kOrderNlFile;
using biyos::test::nl::read_sample;
using biyos::test::nl::tiny_with;

namespace
{

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Index kDepth = 100000;

constexpr std::string_view kBareTail = R"(V1 0 0
o41
v0
C0
v1
O0 0
v1
r
3
b
3
)";

constexpr std::string_view kQuadraticTail = R"(C0
n0
O0 0
o5
v0
n2
r
3
b
3
)";

constexpr std::string_view kLog10Tail = R"(C0
n0
O0 0
o42
v0
r
3
b
3
)";

constexpr std::string_view kPiecesTail = R"(C0
n0
O0 0
o64
3
n3
n1
n4
n2
n5
v0
r
3
b
3
)";

constexpr std::string_view kSumTail = R"(C0
n0
O0 0
o54
3
v0
n1
o16
v0
r
3
b
3
)";

constexpr std::string_view kDeepHead = R"(C0
n0
O0 0
)";

constexpr std::string_view kDeepEnd = R"(v0
r
3
b
3
)";

Variable continuous(Real lower, Real upper)
{
    return Variable{.kind = VariableKind::Continuous, .lower = lower, .upper = upper};
}

std::vector<VariableKind> kinds_of(const Model &model)
{
    std::vector<VariableKind> kinds;
    for (const Variable &variable : model.variables(0, model.n_variables()))
    {
        kinds.push_back(variable.kind);
    }
    return kinds;
}

Offset node(Model &model,
            Operator op,
            std::initializer_list<Offset> children,
            std::initializer_list<Real> values)
{
    return model.add_node(static_cast<int>(op), children, values);
}

void add_goal(Model &model, Index function, ObjectiveSense sense)
{
    Objective objective = default_objective(function, 0);
    objective.sense = sense;
    model.add_objectives(std::span(&objective, 1));
}

Model tiny_wanted(Offset (*build)(Model &, Offset))
{
    Model model;
    model.add_variables(std::vector{continuous(-kInfinity, kInfinity)});
    model.add_functions(std::vector{0.0, 0.0}, std::vector<Offset>{0, 0, 0}, {});
    const Offset root = build(model, model.add_variable_node(0));
    const Index function = 1;
    model.attach_expressions(std::span(&function, 1), std::span(&root, 1));
    const Constraint free{.function = 0, .lower = -kInfinity, .upper = kInfinity};
    model.add_constraints(std::span(&free, 1));
    add_goal(model, 1, ObjectiveSense::Minimize);
    return model;
}

Offset square_of(Model &model, Offset x)
{
    return node(model, Operator::ConstantPower, {x}, {2.0});
}

Offset log10_of(Model &model, Offset x)
{
    const Offset log = node(model, Operator::Log, {x}, {});
    return node(model, Operator::Product, {log}, {std::numbers::log10e});
}

Offset pieces_of(Model &model, Offset x)
{
    const std::vector<Real> xs{-1.0, 0.0, 1.0, 2.0, 4.0};
    const std::vector<Real> ys{-3.0, 0.0, 3.0, 7.0, 17.0};
    return model.add_piecewise(model.add_piecewise_linear(xs, ys), x);
}

Offset sum_of(Model &model, Offset x)
{
    const Offset one = node(model, Operator::Constant, {}, {1.0});
    const Offset minus = node(model, Operator::Negation, {x}, {});
    return node(model, Operator::Sum, {x, one, minus}, {0.0, 1.0, 1.0, 1.0});
}

Model curved()
{
    Model model;
    model.add_variables(std::vector(2, continuous(-kInfinity, kInfinity)));
    const Offset x0 = model.add_variable_node(0);
    const Offset x1 = model.add_variable_node(1);
    const Offset sine = node(model, Operator::Sin, {x0}, {});
    const Offset shared = node(model, Operator::Sum, {x0, sine}, {0.0, 2.0, 1.0});
    const Offset ring = node(model, Operator::Product, {shared, x1}, {1.0});
    const Offset gain = node(model, Operator::Sum, {shared, square_of(model, x0)}, {0.0, 1.0, 1.0});
    model.add_functions(std::vector{0.0, 0.0},
                        std::vector<Offset>{0, 2, 4},
                        {
                            LinearTerm{.variable = 0, .coefficient = 0.0},
                            LinearTerm{.variable = 1, .coefficient = 0.0},
                            LinearTerm{.variable = 0, .coefficient = 0.0},
                            LinearTerm{.variable = 1, .coefficient = 0.0},
                        });
    model.attach_expressions(std::vector<Index>{0, 1}, std::vector{ring, gain});
    const Constraint cap{.function = 0, .lower = -kInfinity, .upper = 4.0};
    model.add_constraints(std::span(&cap, 1));
    add_goal(model, 1, ObjectiveSense::Maximize);
    return model;
}

Index count_of(const Model &model, Operator op)
{
    Index count = 0;
    for (Offset k = 0; k < model.graph().size(); ++k)
    {
        count += model.graph().op(k) == op ? 1 : 0;
    }
    return count;
}

}

TEST(NlTest, Linear)
{
    Model wanted;
    wanted.add_variables(std::vector{continuous(0.0, 10.0), continuous(0.0, kInfinity)});
    wanted.add_functions(std::vector{0.0, 0.0, 1.0},
                         std::vector<Offset>{0, 2, 4, 6},
                         {
                             LinearTerm{.variable = 0, .coefficient = 1.0},
                             LinearTerm{.variable = 1, .coefficient = 1.0},
                             LinearTerm{.variable = 0, .coefficient = 1.0},
                             LinearTerm{.variable = 1, .coefficient = -1.0},
                             LinearTerm{.variable = 0, .coefficient = 2.0},
                             LinearTerm{.variable = 1, .coefficient = 3.0},
                         });
    wanted.add_constraints(std::vector{
        Constraint{.function = 0, .lower = 1.0, .upper = kInfinity},
        Constraint{.function = 1, .lower = 0.0, .upper = 0.0},
    });
    add_goal(wanted, 2, ObjectiveSense::Minimize);
    EXPECT_EQ(difference(read_sample(kLinearNlFile), wanted), "");
}

TEST(NlTest, Nonlinear)
{
    EXPECT_EQ(difference(read_sample(kNonlinearNlFile), curved()), "");
}

TEST(NlTest, SharedDefined)
{
    EXPECT_EQ(count_of(read_sample(kNonlinearNlFile), Operator::Sin), 1);
}

TEST(NlTest, BareDefined)
{
    const std::string file = std::string(kDefinedHeader) + std::string(kBareTail);
    EXPECT_EQ(count_of(read_sample(file), Operator::Sin), 1);
}

TEST(NlTest, ZeroTerms)
{
    EXPECT_EQ(read_sample(kNonlinearNlFile).linear_terms(0, 1).size(), 2);
}

TEST(NlTest, Quadratic)
{
    const Model model = read_sample(tiny_with(kQuadraticTail));
    EXPECT_EQ(model.quadratic_terms(1, 2).size(), 1);
    EXPECT_EQ(model.roots(1, 2).front(), kNoRoot);
    EXPECT_EQ(difference(model, tiny_wanted(square_of)), "");
}

TEST(NlTest, Log10)
{
    EXPECT_EQ(difference(read_sample(tiny_with(kLog10Tail)), tiny_wanted(log10_of)), "");
}

TEST(NlTest, Pieces)
{
    const Model model = read_sample(tiny_with(kPiecesTail));
    EXPECT_EQ(difference(model, tiny_wanted(pieces_of)), "");
}

TEST(NlTest, Sum)
{
    const Model model = read_sample(tiny_with(kSumTail));
    EXPECT_EQ(difference(model, tiny_wanted(sum_of)), "");
}

TEST(NlTest, Deep)
{
    std::string file = tiny_with(kDeepHead);
    for (Index k = 0; k < kDepth; ++k)
    {
        file += "o16\n";
    }
    file += kDeepEnd;
    EXPECT_EQ(count_of(read_sample(file), Operator::Negation), kDepth);
}

TEST(NlTest, Kinds)
{
    const Model model = read_sample(kKindsNlFile);
    const std::vector<VariableKind> wanted{
        VariableKind::Continuous,
        VariableKind::Integer,
        VariableKind::Integer,
        VariableKind::Binary,
        VariableKind::Integer,
    };
    EXPECT_EQ(kinds_of(model), wanted);
}

TEST(NlTest, Order)
{
    const Model model = read_sample(kOrderNlFile);
    const std::vector<VariableKind> wanted{
        VariableKind::Continuous,
        VariableKind::Integer,
        VariableKind::Continuous,
        VariableKind::Integer,
        VariableKind::Continuous,
        VariableKind::Binary,
    };
    EXPECT_EQ(kinds_of(model), wanted);
}

TEST(NlTest, Objectives)
{
    const Model model = read_sample(kObjectivesNlFile);
    ASSERT_EQ(model.n_objectives(), 2);
    EXPECT_EQ(model.objective(0).priority, 0);
    EXPECT_EQ(model.objective(1).priority, 1);
    EXPECT_EQ(model.objective(0).sense, ObjectiveSense::Minimize);
    EXPECT_EQ(model.objective(1).sense, ObjectiveSense::Maximize);
    EXPECT_EQ(model.objective(1).function, 1);
    EXPECT_EQ(model.linear_terms(1, 2).size(), 1);
}
