#include "core/index.hpp"
#include "expr/canonical.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "expr/parents.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Index;
using biyos::kNoRemainder;
using biyos::LinearTerm;
using biyos::Offset;
using biyos::Operator;
using biyos::Parents;
using biyos::Parts;
using biyos::QuadraticTerm;
using biyos::test::refusal;

constexpr double kHuge = 1e200;
constexpr double kLargest = 1e308;
constexpr double kNearTwo = 2.0000000000000004;
constexpr double kNearOne = 1.0000000000000002;
constexpr double kAbsorber = 0x1p54;

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

std::vector<std::uint64_t> bits(std::span<const double> values)
{
    std::vector<std::uint64_t> words;
    for (const double value : values)
    {
        words.push_back(bits(value));
    }
    return words;
}

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

void expect_linear(const Parts &parts, const std::vector<LinearTerm> &wanted)
{
    ASSERT_EQ(parts.linear.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_EQ(parts.linear[k].variable, wanted[k].variable);
        EXPECT_DOUBLE_EQ(parts.linear[k].coefficient, wanted[k].coefficient);
    }
}

void expect_quadratic(const Parts &parts, const std::vector<QuadraticTerm> &wanted)
{
    ASSERT_EQ(parts.quadratic.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_EQ(parts.quadratic[k].first, wanted[k].first);
        EXPECT_EQ(parts.quadratic[k].second, wanted[k].second);
        EXPECT_DOUBLE_EQ(parts.quadratic[k].coefficient, wanted[k].coefficient);
    }
}

void expect_whole(const Parts &parts, Offset node)
{
    EXPECT_EQ(parts.remainder, node);
    EXPECT_DOUBLE_EQ(parts.constant, 0.0);
    EXPECT_TRUE(parts.linear.empty());
    EXPECT_TRUE(parts.quadratic.empty());
}

Parts parts_of(Graph &graph, Offset node)
{
    const Parents parents(graph);
    return biyos::split(graph, parents, node);
}

Offset constant(Graph &graph, double value)
{
    return graph.add(Operator::Constant, value, {});
}

Offset unary(Graph &graph, Operator op, Offset child)
{
    return graph.add(op, std::span(&child, 1));
}

Offset product(Graph &graph, double coefficient, const std::vector<Offset> &factors)
{
    return graph.add(Operator::Product, coefficient, factors);
}

Offset power(Graph &graph, Offset base, double exponent)
{
    return graph.add(Operator::ConstantPower, exponent, std::span(&base, 1));
}

Offset sum(Graph &graph,
           double constant,
           const std::vector<Offset> &terms,
           const std::vector<double> &weights)
{
    return graph.add_sum(constant, terms, weights);
}

std::vector<Offset> variables(Graph &graph, Index count)
{
    std::vector<Offset> nodes;
    for (Index k = 0; k < count; ++k)
    {
        nodes.push_back(graph.add_variable(k));
    }
    return nodes;
}

void expect_wrapped(Graph &graph, Offset x, Offset node)
{
    const Offset root = sum(graph, 0.0, {x, node}, {1.0, 1.0});
    const Parts parts = parts_of(graph, root);
    expect_linear(parts, {{.variable = graph.variable(x), .coefficient = 1.0}});
    ASSERT_NE(parts.remainder, kNoRemainder);
    EXPECT_NE(parts.remainder, node);
    EXPECT_EQ(ids(graph.children(parts.remainder)), std::vector<Offset>{node});
    const Offset size = graph.size();
    expect_whole(parts_of(graph, parts.remainder), parts.remainder);
    EXPECT_EQ(graph.size(), size);
}

}

TEST(CanonicalTest, Constant)
{
    Graph graph;
    const Offset lone = constant(graph, 2.5);
    const Parts single = parts_of(graph, lone);
    EXPECT_DOUBLE_EQ(single.constant, 2.5);
    EXPECT_EQ(single.remainder, kNoRemainder);
    const Offset x = graph.add_variable(0);
    const Offset three = constant(graph, 3.0);
    const Offset root = sum(graph, 4.0, {three, x}, {2.0, 1.0});
    const Parts parts = parts_of(graph, root);
    EXPECT_DOUBLE_EQ(parts.constant, 10.0);
    expect_linear(parts, {{.variable = 0, .coefficient = 1.0}});
}

TEST(CanonicalTest, Linear)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset scaled = product(graph, 2.0, {x[1]});
    const Offset four = constant(graph, 4.0);
    const Offset factored = product(graph, 1.0, {four, x[2]});
    const Offset root = sum(graph, 0.0, {x[0], scaled, factored}, {3.0, 1.0, -1.0});
    const Offset size = graph.size();
    const Parts parts = parts_of(graph, root);
    expect_linear(parts,
                  {
                      {.variable = 0, .coefficient = 3.0},
                      {.variable = 1, .coefficient = 2.0},
                      {.variable = 2, .coefficient = -4.0},
                  });
    EXPECT_TRUE(parts.quadratic.empty());
    EXPECT_EQ(parts.remainder, kNoRemainder);
    EXPECT_EQ(graph.size(), size);
}

TEST(CanonicalTest, Quadratic)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 4);
    const Offset itself = product(graph, 1.0, {x[0], x[0]});
    const Offset pair = product(graph, 2.0, {x[0], x[1]});
    const Offset three = constant(graph, 3.0);
    const Offset reversed = product(graph, 1.0, {x[3], three, x[0]});
    const Offset square = power(graph, x[2], 2.0);
    const Offset root = sum(graph, 0.0, {itself, pair, reversed, square}, {-1.0, 1.0, 1.0, 5.0});
    const Parts parts = parts_of(graph, root);
    expect_quadratic(parts,
                     {
                         {.first = 0, .second = 0, .coefficient = -1.0},
                         {.first = 0, .second = 1, .coefficient = 2.0},
                         {.first = 0, .second = 3, .coefficient = 3.0},
                         {.first = 2, .second = 2, .coefficient = 5.0},
                     });
    EXPECT_TRUE(parts.linear.empty());
    EXPECT_EQ(parts.remainder, kNoRemainder);
}

TEST(CanonicalTest, Remainder)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset exponential = unary(graph, Operator::Exp, x);
    const Offset root = sum(graph, 0.0, {exponential, x}, {1.0, 3.0});
    const Offset size = graph.size();
    const Parts parts = parts_of(graph, root);
    expect_linear(parts, {{.variable = 0, .coefficient = 3.0}});
    EXPECT_EQ(parts.remainder, exponential);
    EXPECT_EQ(graph.size(), size);
}

TEST(CanonicalTest, SquareKept)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset inner = sum(graph, 0.0, {x[0], x[1]}, {1.0, 1.0});
    const Offset square = power(graph, inner, 2.0);
    const Offset cube = power(graph, x[0], 3.0);
    const Offset signed_square = graph.add(Operator::SignPower, 2.0, std::span(&x[0], 1));
    const Offset size = graph.size();
    expect_whole(parts_of(graph, square), square);
    expect_whole(parts_of(graph, cube), cube);
    expect_whole(parts_of(graph, signed_square), signed_square);
    EXPECT_EQ(graph.size(), size);
}

TEST(CanonicalTest, WideSquare)
{
    Graph graph;
    const Index count = 100;
    const std::vector<Offset> x = variables(graph, count);
    const std::vector<double> weights(x.size(), 0.5);
    const Offset inner = sum(graph, 1.0, x, weights);
    const Offset square = power(graph, inner, 2.0);
    const Offset twice = product(graph, 1.0, {inner, inner});
    const Offset y = graph.add_variable(count);
    const Offset root = sum(graph, 0.0, {square, twice, y}, {1.0, 1.0, 1.0});
    const Offset size = graph.size();
    const Parts parts = parts_of(graph, root);
    expect_linear(parts, {{.variable = count, .coefficient = 1.0}});
    EXPECT_TRUE(parts.quadratic.empty());
    ASSERT_EQ(graph.size(), size + 1);
    EXPECT_EQ(ids(graph.children(parts.remainder)), (std::vector<Offset>{square, twice}));
}

TEST(CanonicalTest, ProductKept)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 4);
    const Offset cubic = product(graph, 1.0, {x[0], x[1], x[2]});
    const Offset exponential = unary(graph, Operator::Exp, x[1]);
    const Offset mixed = product(graph, 2.0, {x[0], exponential});
    const Offset left = sum(graph, 0.0, {x[0], x[1]}, {1.0, 1.0});
    const Offset right = sum(graph, 0.0, {x[0], x[2]}, {1.0, 1.0});
    const Offset sums = product(graph, 1.0, {left, right});
    for (const Offset node : {cubic, mixed, sums})
    {
        expect_whole(parts_of(graph, node), node);
        const Offset root = sum(graph, 0.0, {x[3], node}, {1.0, 1.0});
        const Offset size = graph.size();
        const Parts parts = parts_of(graph, root);
        expect_linear(parts, {{.variable = 3, .coefficient = 1.0}});
        EXPECT_EQ(parts.remainder, node);
        EXPECT_EQ(graph.size(), size);
    }
}

TEST(CanonicalTest, UnaryKept)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const std::vector<Operator> unary_ops{
        Operator::Exp,
        Operator::Log,
        Operator::Sqrt,
        Operator::Sin,
        Operator::Cos,
        Operator::Tan,
        Operator::Abs,
        Operator::Asin,
        Operator::Acos,
        Operator::Atan,
        Operator::Sinh,
        Operator::Cosh,
        Operator::Tanh,
        Operator::Asinh,
        Operator::Acosh,
        Operator::Atanh,
        Operator::Entropy,
        Operator::Erf,
    };
    for (const Operator op : unary_ops)
    {
        const Offset node = unary(graph, op, x[0]);
        const Offset root = sum(graph, 0.0, {node, x[1]}, {1.0, 1.0});
        const Parts parts = parts_of(graph, root);
        expect_linear(parts, {{.variable = 1, .coefficient = 1.0}});
        EXPECT_EQ(parts.remainder, node);
    }
}

TEST(CanonicalTest, NoRemainder)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset pair = product(graph, 2.0, {x[1], x[2]});
    const Offset root = sum(graph, 3.0, {x[0], pair}, {1.0, 1.0});
    const Offset size = graph.size();
    const Parts parts = parts_of(graph, root);
    EXPECT_DOUBLE_EQ(parts.constant, 3.0);
    expect_linear(parts, {{.variable = 0, .coefficient = 1.0}});
    expect_quadratic(parts, {{.first = 1, .second = 2, .coefficient = 2.0}});
    EXPECT_EQ(parts.remainder, kNoRemainder);
    EXPECT_EQ(graph.size(), size);
}

TEST(CanonicalTest, NodeReturned)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset exponential = unary(graph, Operator::Exp, x[0]);
    const Offset sine = unary(graph, Operator::Sin, x[1]);
    const Offset root = sum(graph, 0.0, {exponential, sine}, {2.0, 1.0});
    const Offset size = graph.size();
    expect_whole(parts_of(graph, root), root);
    expect_whole(parts_of(graph, exponential), exponential);
    EXPECT_EQ(graph.size(), size);
}

TEST(CanonicalTest, SplitAgain)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset exponential = unary(graph, Operator::Exp, x[1]);
    const Offset sine = unary(graph, Operator::Sin, x[2]);
    const Offset root = sum(graph, 5.0, {x[0], exponential, sine}, {1.0, 2.0, -1.0});
    const Parts parts = parts_of(graph, root);
    ASSERT_NE(parts.remainder, kNoRemainder);
    EXPECT_EQ(ids(graph.children(parts.remainder)), (std::vector<Offset>{exponential, sine}));
    expect_values(graph.coefficients(parts.remainder), {2.0, -1.0});
    EXPECT_DOUBLE_EQ(graph.constant(parts.remainder), 0.0);
    const Offset size = graph.size();
    expect_whole(parts_of(graph, parts.remainder), parts.remainder);
    EXPECT_EQ(graph.size(), size);
}

TEST(CanonicalTest, SharedAlone)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset exponential = unary(graph, Operator::Exp, x[2]);
    const Offset shared = sum(graph, 0.0, {x[1], exponential}, {1.0, 1.0});
    const Offset negated_sum = sum(graph, 0.0, {x[1], exponential}, {1.0, 1.0});
    const Offset negated = unary(graph, Operator::Negation, negated_sum);
    const Offset scaled_sum = sum(graph, 0.0, {x[1], exponential}, {1.0, 1.0});
    const Offset scaled = product(graph, 3.0, {scaled_sum});
    for (const Offset node : {shared, negated, scaled})
    {
        unary(graph, Operator::Sin, node);
        expect_wrapped(graph, x[0], node);
    }
}

TEST(CanonicalTest, NearSquare)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset near = power(graph, x[0], kNearTwo);
    const Offset root = sum(graph, 0.0, {near, x[1]}, {1.0, 1.0});
    const Parts parts = parts_of(graph, root);
    expect_linear(parts, {{.variable = 1, .coefficient = 1.0}});
    EXPECT_TRUE(parts.quadratic.empty());
    EXPECT_EQ(parts.remainder, near);
}

TEST(CanonicalTest, NearUnit)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset exponential = unary(graph, Operator::Exp, x[0]);
    const Offset root = sum(graph, 0.0, {exponential, x[1]}, {kNearOne, 1.0});
    const Parts parts = parts_of(graph, root);
    ASSERT_NE(parts.remainder, kNoRemainder);
    EXPECT_NE(parts.remainder, exponential);
    EXPECT_EQ(ids(graph.children(parts.remainder)), std::vector<Offset>{exponential});
    EXPECT_EQ(bits(graph.coefficients(parts.remainder)), std::vector{bits(kNearOne)});
}

TEST(CanonicalTest, Overflow)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset huge = constant(graph, kHuge);
    const Offset scaled = product(graph, kHuge, {x[0]});
    const Offset pair = product(graph, kHuge, {x[0], x[1]});
    const Offset exponential = unary(graph, Operator::Exp, x[0]);
    for (const Offset term : {huge, scaled, pair})
    {
        const Offset root = sum(graph, 0.0, {term, exponential, x[1]}, {kHuge, 1.0, 1.0});
        const Parts parts = parts_of(graph, root);
        EXPECT_DOUBLE_EQ(parts.constant, 0.0);
        expect_linear(parts, {{.variable = 1, .coefficient = 1.0}});
        EXPECT_TRUE(parts.quadratic.empty());
        ASSERT_NE(parts.remainder, kNoRemainder);
        EXPECT_EQ(ids(graph.children(parts.remainder)), (std::vector<Offset>{term, exponential}));
        expect_values(graph.coefficients(parts.remainder), {kHuge, 1.0});
    }
    const Offset largest = constant(graph, kLargest);
    const Offset root = sum(graph, kLargest, {largest, exponential, x[1]}, {1.0, 1.0, 1.0});
    const Offset size = graph.size();
    expect_whole(parts_of(graph, root), root);
    EXPECT_EQ(graph.size(), size);
}

TEST(CanonicalTest, MissingNode)
{
    Graph graph;
    graph.add_variable(0);
    EXPECT_EQ(refusal([&] { parts_of(graph, -1); }),
              "invalid argument: expression: node -1 is unknown");
    EXPECT_EQ(refusal([&] { parts_of(graph, 1); }),
              "invalid argument: expression: node 1 is unknown");
    EXPECT_EQ(graph.size(), 1);
}

TEST(CanonicalTest, Negation)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset exponential = unary(graph, Operator::Exp, x[1]);
    const Offset inner = sum(graph, 0.0, {x[0], exponential}, {1.0, 1.0});
    const Offset negated = unary(graph, Operator::Negation, inner);
    const Parts parts = parts_of(graph, negated);
    expect_linear(parts, {{.variable = 0, .coefficient = -1.0}});
    ASSERT_NE(parts.remainder, kNoRemainder);
    EXPECT_EQ(graph.op(parts.remainder), Operator::Sum);
    EXPECT_EQ(ids(graph.children(parts.remainder)), std::vector<Offset>{exponential});
    expect_values(graph.coefficients(parts.remainder), {-1.0});
    const Offset once = unary(graph, Operator::Negation, x[2]);
    const Offset twice = unary(graph, Operator::Negation, once);
    const Offset root = sum(graph, 0.0, {x[0], twice}, {1.0, 3.0});
    expect_linear(parts_of(graph, root),
                  {
                      {.variable = 0, .coefficient = 1.0},
                      {.variable = 2, .coefficient = 3.0},
                  });
}

TEST(CanonicalTest, ScaledSum)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset exponential = unary(graph, Operator::Exp, x[1]);
    const Offset inner = sum(graph, 1.0, {x[0], exponential}, {1.0, 1.0});
    const Offset two = constant(graph, 2.0);
    const Offset scaled = product(graph, 1.5, {two, inner});
    const Parts parts = parts_of(graph, scaled);
    EXPECT_DOUBLE_EQ(parts.constant, 3.0);
    expect_linear(parts, {{.variable = 0, .coefficient = 3.0}});
    ASSERT_NE(parts.remainder, kNoRemainder);
    EXPECT_EQ(ids(graph.children(parts.remainder)), std::vector<Offset>{exponential});
    expect_values(graph.coefficients(parts.remainder), {3.0});
}

TEST(CanonicalTest, InnerSum)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset exponential = unary(graph, Operator::Exp, x[2]);
    const Offset inner = sum(graph, 2.0, {x[1], exponential}, {4.0, 1.0});
    const Offset root = sum(graph, 1.0, {x[0], inner}, {1.0, 0.5});
    const Parts parts = parts_of(graph, root);
    EXPECT_DOUBLE_EQ(parts.constant, 2.0);
    expect_linear(parts,
                  {
                      {.variable = 0, .coefficient = 1.0},
                      {.variable = 1, .coefficient = 2.0},
                  });
    ASSERT_NE(parts.remainder, kNoRemainder);
    EXPECT_EQ(ids(graph.children(parts.remainder)), std::vector<Offset>{exponential});
    expect_values(graph.coefficients(parts.remainder), {0.5});
}

TEST(CanonicalTest, SharedSum)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset exponential = unary(graph, Operator::Exp, x[2]);
    const Offset shared = sum(graph, 1.0, {x[1], exponential}, {1.0, 1.0});
    unary(graph, Operator::Sin, shared);
    const Offset negated = unary(graph, Operator::Negation, x[1]);
    unary(graph, Operator::Cos, negated);
    const Offset single = unary(graph, Operator::Negation, x[2]);
    const Offset inner = sum(graph, 0.0, {x[0], exponential}, {1.0, 1.0});
    const Offset scaled = product(graph, 3.0, {inner});
    unary(graph, Operator::Tan, scaled);
    const std::vector<Offset> terms{shared, x[0], negated, single, scaled};
    const Offset root = sum(graph, 0.0, terms, {2.0, 1.0, 1.0, 1.0, 1.0});
    const Parts parts = parts_of(graph, root);
    EXPECT_DOUBLE_EQ(parts.constant, 0.0);
    expect_linear(parts,
                  {
                      {.variable = 0, .coefficient = 1.0},
                      {.variable = 2, .coefficient = -1.0},
                  });
    ASSERT_NE(parts.remainder, kNoRemainder);
    const std::vector<Offset> kept{shared, negated, scaled};
    EXPECT_EQ(ids(graph.children(parts.remainder)), kept);
    expect_values(graph.coefficients(parts.remainder), {2.0, 1.0, 1.0});
}

TEST(CanonicalTest, PushedAgain)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset exponential = unary(graph, Operator::Exp, x[1]);
    const Offset shared = sum(graph, 0.0, {x[2], exponential}, {1.0, 1.0});
    unary(graph, Operator::Sin, shared);
    const Offset inner = sum(graph, 0.0, {x[0], exponential, shared}, {1.0, 1.0, 1.0});
    const Offset negated = unary(graph, Operator::Negation, inner);
    const Parts parts = parts_of(graph, negated);
    expect_linear(parts, {{.variable = 0, .coefficient = -1.0}});
    ASSERT_NE(parts.remainder, kNoRemainder);
    const Offset size = graph.size();
    expect_whole(parts_of(graph, parts.remainder), parts.remainder);
    EXPECT_EQ(graph.size(), size);
}

TEST(CanonicalTest, ScaledOverflow)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset exponential = unary(graph, Operator::Exp, x[0]);
    const Offset inner = sum(graph, 0.0, {x[0], exponential}, {1.0, 1.0});
    const Offset scaled = product(graph, kHuge, {inner});
    const Offset wide = sum(graph, 0.0, {x[0], exponential}, {kHuge, 1.0});
    const Offset offset = sum(graph, kHuge, {exponential}, {1.0});
    for (const Offset term : {scaled, wide, offset})
    {
        const Offset negated = unary(graph, Operator::Negation, x[2]);
        const Offset root = sum(graph, 0.0, {term, x[1], negated}, {kHuge, 1.0, 1.0});
        const Parts parts = parts_of(graph, root);
        expect_linear(parts,
                      {
                          {.variable = 1, .coefficient = 1.0},
                          {.variable = 2, .coefficient = -1.0},
                      });
        ASSERT_NE(parts.remainder, kNoRemainder);
        EXPECT_EQ(ids(graph.children(parts.remainder)), std::vector<Offset>{term});
        expect_values(graph.coefficients(parts.remainder), {kHuge});
    }
}

TEST(CanonicalTest, SharedRoot)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset shared = sum(graph, -1.0, {x[0], x[1]}, {1.0, 1.0});
    unary(graph, Operator::Sin, shared);
    unary(graph, Operator::Cos, shared);
    const Parts parts = parts_of(graph, shared);
    EXPECT_DOUBLE_EQ(parts.constant, -1.0);
    expect_linear(parts,
                  {
                      {.variable = 0, .coefficient = 1.0},
                      {.variable = 1, .coefficient = 1.0},
                  });
    EXPECT_EQ(parts.remainder, kNoRemainder);
    const Offset pair = product(graph, 2.0, {x[0], x[1]});
    unary(graph, Operator::Cos, pair);
    const Parts square = parts_of(graph, pair);
    expect_quadratic(square, {{.first = 0, .second = 1, .coefficient = 2.0}});
    EXPECT_EQ(square.remainder, kNoRemainder);
}

TEST(CanonicalTest, CallOrder)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset exponential = unary(graph, Operator::Exp, x[1]);
    const Offset root = sum(graph, -1.0, {x[0], exponential}, {2.0, 1.0});
    const Parts before = parts_of(graph, root);
    unary(graph, Operator::Sin, root);
    const Parts after = parts_of(graph, root);
    expect_linear(before, {{.variable = 0, .coefficient = 2.0}});
    EXPECT_EQ(before.remainder, exponential);
    EXPECT_DOUBLE_EQ(after.constant, before.constant);
    expect_linear(after, before.linear);
    EXPECT_EQ(after.remainder, before.remainder);
}

TEST(CanonicalTest, OldParents)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Parents parents(graph);
    const Offset exponential = unary(graph, Operator::Exp, x);
    EXPECT_EQ(refusal([&] { biyos::split(graph, parents, exponential); }),
              "invalid argument: expression: parents are older than node 1");
    EXPECT_EQ(graph.size(), 2);
}

TEST(CanonicalTest, Sorted)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 3);
    const Offset pair = product(graph, 1.0, {x[2], x[1]});
    const Offset square = power(graph, x[0], 2.0);
    const Offset root =
        sum(graph, 0.0, {x[2], pair, x[0], square, x[1]}, {1.0, 2.0, 3.0, 4.0, 5.0});
    const Parts parts = parts_of(graph, root);
    expect_linear(parts,
                  {
                      {.variable = 0, .coefficient = 3.0},
                      {.variable = 1, .coefficient = 5.0},
                      {.variable = 2, .coefficient = 1.0},
                  });
    expect_quadratic(parts,
                     {
                         {.first = 0, .second = 0, .coefficient = 4.0},
                         {.first = 1, .second = 2, .coefficient = 2.0},
                     });
}

TEST(CanonicalTest, PairAdded)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset pair = product(graph, 2.0, {x[0], x[1]});
    const Offset reversed = product(graph, 3.0, {x[1], x[0]});
    const Offset square = power(graph, x[0], 2.0);
    const Offset itself = product(graph, 1.0, {x[0], x[0]});
    const Offset root = sum(graph, 0.0, {pair, square, reversed, itself}, {1.0, 1.0, 1.0, -0.5});
    const Parts parts = parts_of(graph, root);
    expect_quadratic(parts,
                     {
                         {.first = 0, .second = 0, .coefficient = 0.5},
                         {.first = 0, .second = 1, .coefficient = 5.0},
                     });
}

TEST(CanonicalTest, VariableAdded)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset scaled = product(graph, 4.0, {x[0]});
    const Offset root = sum(graph, 0.0, {x[0], x[1], scaled, x[0]}, {1.0, 2.0, 1.0, -5.0});
    const Parts parts = parts_of(graph, root);
    expect_linear(parts,
                  {
                      {.variable = 0, .coefficient = 0.0},
                      {.variable = 1, .coefficient = 2.0},
                  });
}

TEST(CanonicalTest, AddedOverflow)
{
    Graph graph;
    const std::vector<Offset> x = variables(graph, 2);
    const Offset exponential = unary(graph, Operator::Exp, x[0]);
    const Offset linear = sum(graph, 0.0, {x[0], exponential, x[0]}, {kLargest, 1.0, kLargest});
    expect_whole(parts_of(graph, linear), linear);
    const Offset pair = product(graph, kLargest, {x[0], x[1]});
    const Offset again = product(graph, kLargest, {x[1], x[0]});
    const Offset quadratic = sum(graph, 0.0, {pair, exponential, again}, {1.0, 1.0, 1.0});
    expect_whole(parts_of(graph, quadratic), quadratic);
}

TEST(CanonicalTest, StableOrder)
{
    Graph graph;
    const Index count = 16;
    const std::vector<Offset> x = variables(graph, count);
    const std::array<double, 5> absorbed{kAbsorber, 1.0, 1.0, 1.0, -kAbsorber};
    std::vector<Offset> terms;
    std::vector<double> weights;
    for (Index k = 1; k < count; ++k)
    {
        terms.push_back(x[static_cast<std::size_t>(k)]);
        weights.push_back(1.0);
        if (k % 3 == 0)
        {
            terms.push_back(x[0]);
            weights.push_back(absorbed[static_cast<std::size_t>(k / 3 - 1)]);
        }
    }
    const Offset root = sum(graph, 0.0, terms, weights);
    const Parts parts = parts_of(graph, root);
    ASSERT_EQ(parts.linear.size(), x.size());
    EXPECT_EQ(parts.linear[0].variable, 0);
    EXPECT_EQ(bits(parts.linear[0].coefficient), bits(0.0));
}
