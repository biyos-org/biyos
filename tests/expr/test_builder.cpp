#include "core/index.hpp"
#include "expr/builder.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace
{

using biyos::Builder;
using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::test::refusal;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kHuge = 1e200;
constexpr double kTiny = 1e-200;
constexpr double kLargest = (std::numeric_limits<double>::max)();
constexpr double kSmallest = std::numeric_limits<double>::denorm_min();

std::vector<Offset> ids(std::span<const Offset> nodes)
{
    return {nodes.begin(), nodes.end()};
}

void expect_values(std::span<const double> found, const std::vector<double> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_DOUBLE_EQ(found[k], wanted[k]);
    }
}

Offset unary(Graph &graph, Operator op, Offset child)
{
    Builder build(graph);
    build.node(child);
    build.apply(op, 1);
    return build.finish();
}

Offset sum_of(Graph &graph, Offset first, Offset count, double constant)
{
    Builder build(graph);
    std::vector<double> coefficients;
    for (Offset k = 0; k < count; ++k)
    {
        build.node(first + k);
        coefficients.push_back(1.0);
    }
    build.sum(constant, coefficients);
    return build.finish();
}

void add_variables(Builder &build, int count)
{
    for (int k = 0; k < count; ++k)
    {
        build.variable(k);
    }
}

Offset nested_sum(Graph &graph, double constant, double weight, double outer, double scale)
{
    Builder build(graph);
    build.variable(0);
    build.sum(constant, std::span(&weight, 1));
    build.sum(outer, std::span(&scale, 1));
    return build.finish();
}

Offset nested_product(Graph &graph, double inner, double outer)
{
    Builder build(graph);
    build.variable(0);
    build.product(inner, 1);
    build.product(outer, 1);
    return build.finish();
}

}

TEST(GraphTest, BuiltOrder)
{
    Graph graph;
    Builder build(graph);
    add_variables(build, 2);
    build.product(2.0, 2);
    build.apply(Operator::Exp, 1);
    build.variable(2);
    const std::array weights{3.0, 4.0};
    build.sum(1.0, weights);
    build.apply(Operator::Sin, 1);
    const Offset root = build.finish();
    for (Offset node = 0; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            EXPECT_LT(child, node);
        }
    }
    EXPECT_EQ(graph.size(), 7);
    EXPECT_EQ(root, graph.size() - 1);
    EXPECT_EQ(graph.op(2), Operator::Product);
}

TEST(GraphTest, SharedNode)
{
    Graph graph;
    Builder build(graph);
    build.variable(0);
    build.apply(Operator::Exp, 1);
    const Offset shared = build.finish();
    const Offset sine = unary(graph, Operator::Sin, shared);
    const Offset cosine = unary(graph, Operator::Cos, shared);
    build.node(shared);
    build.node(shared);
    build.apply(Operator::Division, 2);
    const Offset ratio = build.finish();
    EXPECT_EQ(graph.size(), 5);
    EXPECT_EQ(ids(graph.children(sine)), std::vector<Offset>{shared});
    EXPECT_EQ(ids(graph.children(cosine)), std::vector<Offset>{shared});
    EXPECT_EQ(ids(graph.children(ratio)), (std::vector<Offset>{shared, shared}));
}

TEST(GraphTest, SumMerged)
{
    Graph graph;
    Builder build(graph);
    add_variables(build, 2);
    const std::array inner{2.0, 3.0};
    build.sum(1.0, inner);
    build.variable(2);
    const std::array outer{4.0, 5.0};
    build.sum(0.5, outer);
    const Offset root = build.finish();
    ASSERT_EQ(graph.size(), 4);
    EXPECT_EQ(graph.op(root), Operator::Sum);
    EXPECT_EQ(ids(graph.children(root)), (std::vector<Offset>{0, 1, 2}));
    expect_values(graph.coefficients(root), {8.0, 12.0, 5.0});
    EXPECT_DOUBLE_EQ(graph.constant(root), 4.5);
}

TEST(GraphTest, ProductMerged)
{
    Graph graph;
    Builder build(graph);
    add_variables(build, 2);
    build.product(2.0, 2);
    build.variable(2);
    build.product(3.0, 2);
    const Offset root = build.finish();
    ASSERT_EQ(graph.size(), 4);
    EXPECT_EQ(graph.op(root), Operator::Product);
    EXPECT_EQ(ids(graph.children(root)), (std::vector<Offset>{0, 1, 2}));
    EXPECT_DOUBLE_EQ(graph.constant(root), 6.0);
}

TEST(GraphTest, MixedKept)
{
    Graph graph;
    Builder build(graph);
    add_variables(build, 2);
    const std::array weights{1.0, 1.0};
    build.sum(0.0, weights);
    build.variable(2);
    build.product(1.0, 2);
    const Offset root = build.finish();
    ASSERT_EQ(graph.size(), 5);
    EXPECT_EQ(graph.op(3), Operator::Sum);
    EXPECT_EQ(ids(graph.children(root)), (std::vector<Offset>{3, 2}));
}

TEST(GraphTest, NodeKept)
{
    Graph graph;
    graph.add_variable(0);
    graph.add_variable(1);
    const Offset inner = sum_of(graph, 0, 2, 1.0);
    Builder build(graph);
    build.node(inner);
    build.variable(2);
    const std::array weights{2.0, 3.0};
    build.sum(0.0, weights);
    const Offset root = build.finish();
    ASSERT_EQ(graph.size(), 5);
    EXPECT_EQ(ids(graph.children(root)), (std::vector<Offset>{inner, 3}));
    expect_values(graph.coefficients(root), {2.0, 3.0});
    EXPECT_DOUBLE_EQ(graph.constant(root), 0.0);
    add_variables(build, 2);
    build.product(2.0, 2);
    const Offset factors = build.finish();
    build.node(factors);
    build.variable(2);
    build.product(3.0, 2);
    const Offset outer = build.finish();
    ASSERT_EQ(graph.size(), 7);
    EXPECT_EQ(ids(graph.children(outer)), (std::vector<Offset>{factors, 3}));
    EXPECT_DOUBLE_EQ(graph.constant(outer), 3.0);
}

TEST(GraphTest, RepeatedVariable)
{
    Graph graph;
    Builder build(graph);
    build.variable(0);
    build.variable(0);
    build.product(1.0, 2);
    const Offset square = build.finish();
    build.variable(0);
    build.apply(Operator::Exp, 1);
    build.finish();
    EXPECT_EQ(graph.size(), 3);
    EXPECT_EQ(ids(graph.children(square)), (std::vector<Offset>{0, 0}));
}

TEST(GraphTest, MergeOverflow)
{
    Graph graph;
    const Offset weights = nested_sum(graph, 0.0, kHuge, 0.0, kHuge);
    const Offset scaled = nested_sum(graph, kHuge, 1.0, 0.0, kHuge);
    const Offset added = nested_sum(graph, kLargest, 1.0, kLargest, 1.0);
    const Offset product = nested_product(graph, kHuge, kHuge);
    ASSERT_EQ(graph.size(), 9);
    EXPECT_EQ(ids(graph.children(weights)), std::vector<Offset>{1});
    EXPECT_EQ(ids(graph.children(scaled)), std::vector<Offset>{3});
    EXPECT_EQ(ids(graph.children(added)), std::vector<Offset>{5});
    EXPECT_EQ(ids(graph.children(product)), std::vector<Offset>{7});
    EXPECT_DOUBLE_EQ(graph.constant(product), kHuge);
}

TEST(GraphTest, MergeUnderflow)
{
    Graph graph;
    const Offset weights = nested_sum(graph, 0.0, kTiny, 0.0, kTiny);
    const Offset scaled = nested_sum(graph, kTiny, 1.0, 0.0, kTiny);
    const Offset product = nested_product(graph, kTiny, kTiny);
    const Offset zero = nested_sum(graph, 1.0, 2.0, 0.0, 0.0);
    ASSERT_EQ(graph.size(), 8);
    EXPECT_EQ(ids(graph.children(weights)), std::vector<Offset>{1});
    EXPECT_EQ(ids(graph.children(scaled)), std::vector<Offset>{3});
    EXPECT_EQ(ids(graph.children(product)), std::vector<Offset>{5});
    EXPECT_EQ(ids(graph.children(zero)), std::vector<Offset>{0});
    expect_values(graph.coefficients(zero), {0.0});
}

TEST(GraphTest, SignPower)
{
    Graph graph;
    Builder build(graph);
    EXPECT_EQ(refusal([&] { build.sign_power(1.5); }),
              "invalid argument: expression: 1 operand taken, 0 given");
    add_variables(build, 2);
    const std::array weights{1.0, 1.0};
    build.sum(0.0, weights);
    EXPECT_EQ(refusal([&] { build.sign_power(kNan); }),
              "invalid argument: expression: constant is NaN");
    EXPECT_EQ(refusal([&] { build.sign_power(kInfinity); }),
              "invalid argument: expression: constant is infinity");
    EXPECT_EQ(refusal([&] { build.sign_power(0.0); }),
              "invalid argument: expression: operator 28 takes a positive constant, not 0");
    EXPECT_EQ(refusal([&] { build.sign_power(-1.0); }),
              "invalid argument: expression: operator 28 takes a positive constant, not -1");
    EXPECT_EQ(refusal([&] { build.sign_power(-0.0); }),
              "invalid argument: expression: operator 28 takes a positive constant, not -0");
    EXPECT_EQ(graph.size(), 2);
    build.sign_power(1.5);
    const Offset root = build.finish();
    ASSERT_EQ(graph.size(), 4);
    EXPECT_EQ(graph.op(root), Operator::SignPower);
    EXPECT_DOUBLE_EQ(graph.constant(root), 1.5);
    EXPECT_EQ(ids(graph.children(root)), std::vector<Offset>{2});
    build.node(root);
    build.sign_power(kSmallest);
    const Offset smallest = build.finish();
    EXPECT_DOUBLE_EQ(graph.constant(smallest), kSmallest);
    build.node(root);
    build.power(-1.0);
    EXPECT_EQ(graph.op(build.finish()), Operator::ConstantPower);
}

TEST(GraphTest, UnaryCall)
{
    Graph graph;
    Builder build(graph);
    build.variable(0);
    build.apply(Operator::Entropy, 1);
    build.apply(Operator::Erf, 1);
    const Offset root = build.finish();
    ASSERT_EQ(graph.size(), 3);
    EXPECT_EQ(graph.op(root), Operator::Erf);
    EXPECT_EQ(graph.op(1), Operator::Entropy);
    EXPECT_EQ(refusal([&] { build.apply(Operator::Atanh, 1); }),
              "invalid argument: expression: 1 operand taken, 0 given");
}

TEST(GraphTest, MissingNode)
{
    Graph graph;
    Builder build(graph);
    EXPECT_EQ(refusal([&] { build.node(0); }), "invalid argument: expression: node 0 is unknown");
    graph.add_variable(0);
    EXPECT_EQ(refusal([&] { build.node(-1); }), "invalid argument: expression: node -1 is unknown");
    build.node(0);
    EXPECT_EQ(build.finish(), 0);
    EXPECT_EQ(graph.size(), 1);
}

TEST(GraphTest, FewOperands)
{
    Graph graph;
    Builder build(graph);
    EXPECT_EQ(refusal([&] { build.finish(); }),
              "invalid argument: expression: 0 operands left, not 1");
    build.variable(0);
    const std::array weights{1.0, 1.0};
    EXPECT_EQ(refusal([&] { build.sum(0.0, weights); }),
              "invalid argument: expression: 2 operands taken, 1 given");
    EXPECT_EQ(refusal([&] { build.apply(Operator::Power, 2); }),
              "invalid argument: expression: 2 operands taken, 1 given");
    build.variable(1);
    EXPECT_EQ(refusal([&] { build.finish(); }),
              "invalid argument: expression: 2 operands left, not 1");
    build.apply(Operator::Max, 2);
    EXPECT_EQ(graph.op(build.finish()), Operator::Max);
}

TEST(GraphTest, RefusedCall)
{
    Graph graph;
    Builder build(graph);
    add_variables(build, 2);
    const std::array weights{1.0, 1.0};
    build.sum(0.0, weights);
    EXPECT_EQ(refusal([&] { build.sum(kNan, std::span(weights.data(), 1)); }),
              "invalid argument: expression: constant is NaN");
    EXPECT_EQ(refusal([&] { build.power(kNan); }), "invalid argument: expression: constant is NaN");
    EXPECT_EQ(refusal([&] { build.sum(0.0, {}); }),
              "invalid argument: expression: operator 3 takes 1 child or more, not 0");
    EXPECT_EQ(refusal([&] { build.product(1.0, 0); }),
              "invalid argument: expression: operator 4 takes 1 child or more, not 0");
    EXPECT_EQ(refusal([&] { build.product(1.0, -1); }),
              "invalid argument: expression: operator 4 takes 1 child or more, not -1");
    EXPECT_EQ(graph.size(), 2);
    const std::array bad{1.0, kInfinity};
    build.variable(2);
    EXPECT_EQ(refusal([&] { build.sum(0.0, bad); }),
              "invalid argument: expression: coefficient 1 is infinity");
    EXPECT_EQ(refusal([&] { build.product(-kInfinity, 2); }),
              "invalid argument: expression: constant is minus infinity");
    EXPECT_EQ(refusal([&] { build.power(kNan); }), "invalid argument: expression: constant is NaN");
    EXPECT_EQ(refusal([&] { build.apply(Operator::Exp, 2); }),
              "invalid argument: expression: operator 9 takes 1 child, not 2");
    EXPECT_EQ(refusal([&] { build.apply(Operator::Sum, 2); }),
              "invalid argument: expression: operator 3 takes coefficients");
    EXPECT_EQ(refusal([&] { build.variable(-1); }),
              "invalid argument: expression: variable -1 is unknown");
    EXPECT_EQ(graph.size(), 3);
    build.sum(0.0, weights);
    const Offset root = build.finish();
    ASSERT_EQ(graph.size(), 4);
    EXPECT_EQ(ids(graph.children(root)), (std::vector<Offset>{0, 1, 2}));
}
