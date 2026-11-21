#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "model/model.hpp"
#include "model/nodes.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::Operator;
using biyos::Variable;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

struct Node
{
    int code;
    std::vector<Offset> children;
    std::vector<double> values;
};

Model with_variables(std::size_t n)
{
    Model model;
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    model.add_variables(std::vector<Variable>(n, free));
    return model;
}

Offset add(Model &model, const Node &node)
{
    return model.add_node(node.code, node.children, node.values);
}

std::vector<Offset> ids(std::span<const Offset> nodes)
{
    return {nodes.begin(), nodes.end()};
}

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

std::string refused(Model &model, const Node &node)
{
    return refusal([&] { add(model, node); });
}

}

TEST(ExpressionTest, NodeByNode)
{
    Model model = with_variables(2);
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {}});
    const Offset y = model.add_variable_node(1);
    const Offset two = add(model, {BIYOS_OP_CONST, {}, {2.0}});
    const Offset shifted = add(model, {BIYOS_OP_SUM, {y, two}, {-0.5, 1.0, 3.0}});
    const Offset sine = add(model, {BIYOS_OP_SIN, {shifted}, {}});
    const Offset product = add(model, {BIYOS_OP_PROD, {exponential, sine}, {4.0}});
    const Offset power = add(model, {BIYOS_OP_CPOW, {product}, {1.5}});
    const Offset signed_power = add(model, {BIYOS_OP_SPOW, {power}, {0.5}});
    const Graph &graph = model.graph();
    EXPECT_EQ(graph.size(), 9);
    EXPECT_EQ(signed_power, 8);
    EXPECT_EQ(graph.op(x), Operator::Variable);
    EXPECT_EQ(graph.variable(y), 1);
    EXPECT_EQ(graph.op(exponential), Operator::Exp);
    EXPECT_EQ(ids(graph.children(exponential)), std::vector<Offset>{x});
    EXPECT_EQ(bits(graph.constant(two)), bits(2.0));
    EXPECT_EQ(graph.op(shifted), Operator::Sum);
    EXPECT_EQ(ids(graph.children(shifted)), (std::vector<Offset>{y, two}));
    EXPECT_EQ(bits(graph.constant(shifted)), bits(-0.5));
    EXPECT_EQ(bits(graph.coefficients(shifted)[0]), bits(1.0));
    EXPECT_EQ(bits(graph.coefficients(shifted)[1]), bits(3.0));
    EXPECT_EQ(graph.op(product), Operator::Product);
    EXPECT_EQ(ids(graph.children(product)), (std::vector<Offset>{exponential, sine}));
    EXPECT_EQ(bits(graph.constant(product)), bits(4.0));
    EXPECT_EQ(graph.op(power), Operator::ConstantPower);
    EXPECT_EQ(bits(graph.constant(power)), bits(1.5));
    EXPECT_EQ(graph.op(signed_power), Operator::SignPower);
    EXPECT_EQ(bits(graph.constant(signed_power)), bits(0.5));
}

TEST(ExpressionTest, SharedId)
{
    Model model = with_variables(1);
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {}});
    const Offset ratio = add(model, {BIYOS_OP_DIV, {exponential, exponential}, {}});
    const Offset most = add(model, {BIYOS_OP_MAX, {exponential, ratio, x}, {}});
    const Graph &graph = model.graph();
    EXPECT_EQ(graph.size(), 4);
    EXPECT_EQ(ids(graph.children(ratio)), (std::vector<Offset>{exponential, exponential}));
    EXPECT_EQ(ids(graph.children(most)), (std::vector<Offset>{exponential, ratio, x}));
}

TEST(ExpressionTest, VariableNode)
{
    Model model = with_variables(3);
    const Offset y = model.add_variable_node(1);
    EXPECT_EQ(model.add_variable_node(1), y);
    EXPECT_EQ(model.graph().size(), 1);
    EXPECT_EQ(refusal([&] { model.add_variable_node(3); }),
              "invalid argument: variable 3: unknown");
    EXPECT_EQ(refusal([&] { model.add_variable_node(-1); }),
              "invalid argument: variable -1: unknown");
    EXPECT_EQ(model.graph().size(), 1);
    EXPECT_EQ(model.add_variable_node(2), 1);
}

TEST(ExpressionTest, RefusedNode)
{
    Model model = with_variables(1);
    const Offset x = model.add_variable_node(0);
    const std::string bad = "invalid argument: expression: ";
    EXPECT_EQ(refused(model, {0, {x}, {}}), bad + "operator 0 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_ERF + 1, {x}, {}}), bad + "operator 30 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_VAR, {}, {}}), bad + "operator 2 takes a variable");
    EXPECT_EQ(refused(model, {BIYOS_OP_EXP, {1}, {}}), bad + "node 1 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_EXP, {-1}, {}}), bad + "node -1 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_EXP, {x, x}, {}}), bad + "operator 9 takes 1 child, not 2");
    EXPECT_EQ(refused(model, {BIYOS_OP_SUM, {}, {0.0}}),
              bad + "operator 3 takes 1 child or more, not 0");
    EXPECT_EQ(refused(model, {BIYOS_OP_CONST, {x}, {1.0}}),
              bad + "operator 1 takes 0 children, not 1");
    EXPECT_EQ(refused(model, {BIYOS_OP_CONST, {}, {kNan}}), bad + "constant is NaN");
    EXPECT_EQ(refused(model, {BIYOS_OP_SUM, {x}, {0.0, kInfinity}}),
              bad + "coefficient 0 is infinity");
    EXPECT_EQ(refused(model, {BIYOS_OP_SUM, {x}, {kNan, 1.0}}), bad + "constant is NaN");
    EXPECT_EQ(refused(model, {BIYOS_OP_SPOW, {x}, {-2.0}}),
              bad + "operator 28 takes a positive constant, not -2");
    EXPECT_EQ(refused(model, {BIYOS_OP_PROD, {x}, {}}),
              "invalid argument: argument 'val': holds 0 values, not 1");
    EXPECT_EQ(refused(model, {BIYOS_OP_SUM, {x, x}, {0.0, 1.0}}),
              "invalid argument: argument 'val': holds 2 values, not 3");
    EXPECT_EQ(model.graph().size(), 1);
}

TEST(ExpressionTest, ValueCount)
{
    Model model = with_variables(1);
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {7.0, 8.0}});
    const Offset sum = add(model, {BIYOS_OP_SUM, {x, exponential}, {1.0, 2.0, 3.0, 4.0}});
    const Graph &graph = model.graph();
    EXPECT_EQ(graph.op(exponential), Operator::Exp);
    EXPECT_EQ(graph.coefficients(sum).size(), 2U);
    EXPECT_EQ(bits(graph.coefficients(sum)[1]), bits(3.0));
}

TEST(ExpressionTest, ValueNeeds)
{
    constexpr Offset kMost = (std::numeric_limits<Offset>::max)();
    EXPECT_EQ(biyos::n_values(BIYOS_OP_CONST, 0), 1);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_SUM, 3), 4);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_SUM, kMost), kMost);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_SUM, -1), 0);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_PROD, 2), 1);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_CPOW, 1), 1);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_SPOW, 1), 1);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_EXP, 1), 0);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_MAX, 3), 0);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_VAR, 4), 0);
    EXPECT_EQ(biyos::n_values(0, 1), 0);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_ERF + 1, 1), 0);
}

TEST(ExpressionTest, NoRoots)
{
    Model model = with_variables(1);
    model.add_functions(std::vector<double>(3, 0.0), std::vector<Offset>(4, 0), {});
    EXPECT_EQ(ids(model.roots(0, 3)), std::vector<Offset>(3, biyos::kNoRoot));
    EXPECT_TRUE(model.roots(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.roots(2, 2); }), "invalid argument: function 3: unknown");
}
