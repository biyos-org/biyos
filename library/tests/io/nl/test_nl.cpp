#include "building.hpp"
#include "files.hpp"
#include "loading.hpp"

#include "core/env/env.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "model/compare/compare.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::Constraint;
using biyos::difference;
using biyos::Env;
using biyos::Evaluation;
using biyos::Index;
using biyos::kNoRoot;
using biyos::LinearTerm;
using biyos::Lookup;
using biyos::Model;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::nl::add_goal;
using biyos::test::nl::continuous;
using biyos::test::nl::count_of;
using biyos::test::nl::kDefinedHeader;
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
using biyos::test::nl::read_sample;
using biyos::test::nl::square_of;
using biyos::test::nl::sum_of;
using biyos::test::nl::tiny_wanted;
using biyos::test::nl::tiny_with;
using biyos::test::nl::window_of;

namespace
{

constexpr Real kNan = std::numeric_limits<Real>::quiet_NaN();

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

constexpr std::string_view kLessTail = R"(C0
n0
O0 0
o6
v0
n1
r
3
b
3
)";

constexpr std::string_view kAtan2Tail = R"(C0
n0
O0 0
o48
n1
v0
r
3
b
3
)";

constexpr std::string_view kWindowTail = R"(C0
n0
O0 0
o35
o21
o23
v0
n1
o28
v0
n-1
v0
n0
r
3
b
3
)";

constexpr std::string_view kOutsideTail = R"(C0
n0
O0 0
o35
o71
3
o22
v0
n-1
o29
v0
n2
o20
o24
v0
n0.5
o34
o30
v0
n3
n1
n0
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

std::vector<VariableKind> kinds_of(const Model &model)
{
    std::vector<VariableKind> kinds;
    for (const Variable &variable : model.variables(0, model.n_variables()))
    {
        kinds.push_back(variable.kind);
    }
    return kinds;
}

Real objective_at(const Model &model, Real x)
{
    Env env;
    Evaluation evaluation(model, env.stream(), env.pool());
    std::vector<Real> f(static_cast<std::size_t>(model.n_functions()));
    evaluation.values(Lookup(env.options()), std::vector{x}, f);
    return f[1];
}

Offset atan2_of(Model &model, Offset x)
{
    const Offset one = node(model, Operator::Constant, {}, {1.0});
    return node(model, Operator::Atan2, {one, x}, {});
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

TEST(NlTest, Less)
{
    const Model model = read_sample(tiny_with(kLessTail));
    EXPECT_EQ(difference(model, tiny_wanted(less_of)), "");
    EXPECT_DOUBLE_EQ(objective_at(model, 3.5), 2.5);
    EXPECT_DOUBLE_EQ(objective_at(model, -2.0), 0.0);
    EXPECT_TRUE(std::isnan(objective_at(model, kNan)));
}

TEST(NlTest, Atan2)
{
    const Model model = read_sample(tiny_with(kAtan2Tail));
    EXPECT_EQ(difference(model, tiny_wanted(atan2_of)), "");
    EXPECT_DOUBLE_EQ(objective_at(model, 1.0), std::numbers::pi / 4.0);
    EXPECT_DOUBLE_EQ(objective_at(model, -1.0), 3.0 * std::numbers::pi / 4.0);
}

TEST(NlTest, Window)
{
    const Model model = read_sample(tiny_with(kWindowTail));
    EXPECT_EQ(difference(model, tiny_wanted(window_of)), "");
    EXPECT_DOUBLE_EQ(objective_at(model, 0.5), 0.5);
    EXPECT_DOUBLE_EQ(objective_at(model, -1.5), 0.0);
}

TEST(NlTest, Outside)
{
    const Model model = read_sample(tiny_with(kOutsideTail));
    EXPECT_EQ(difference(model, tiny_wanted(outside_of)), "");
    EXPECT_DOUBLE_EQ(objective_at(model, -1.5), 1.0);
    EXPECT_DOUBLE_EQ(objective_at(model, 0.5), 1.0);
    EXPECT_DOUBLE_EQ(objective_at(model, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(objective_at(model, 3.0), 1.0);
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
