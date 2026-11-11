#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kSmallest = std::numeric_limits<double>::denorm_min();
constexpr int kRounds = 64;

constexpr std::array kUnary{
    Operator::Negation, Operator::Exp,   Operator::Log,     Operator::Sqrt, Operator::Sin,
    Operator::Cos,      Operator::Tan,   Operator::Abs,     Operator::Asin, Operator::Acos,
    Operator::Atan,     Operator::Sinh,  Operator::Cosh,    Operator::Tanh, Operator::Asinh,
    Operator::Acosh,    Operator::Atanh, Operator::Entropy, Operator::Erf,
};

constexpr std::array kExponents{Operator::ConstantPower, Operator::SignPower};

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
    return graph.add(op, std::span(&child, 1));
}

std::string refused(Operator op, const std::string &reason)
{
    const std::string name = "operator " + std::to_string(static_cast<int>(op));
    return "invalid argument: expression: " + name + " " + reason;
}

std::string constant_refusal(Graph &graph, double value)
{
    return refusal([&] { graph.add(Operator::Constant, value, {}); });
}

std::string sum_refusal(Graph &graph, double constant, double coefficient)
{
    const Offset child = 0;
    const std::span children(&child, 1);
    const std::span coefficients(&coefficient, 1);
    return refusal([&] { graph.add_sum(constant, children, coefficients); });
}

}

TEST(GraphTest, Payloads)
{
    Graph graph;
    const Offset two = graph.add(Operator::Constant, 2.5, {});
    const Offset x = graph.add_variable(3);
    const std::array terms{two, x};
    const std::array weights{2.0, -1.0};
    const Offset sum = graph.add_sum(1.5, terms, weights);
    const std::array factors{x, sum};
    const Offset product = graph.add(Operator::Product, 4.0, factors);
    const Offset power = graph.add(Operator::ConstantPower, 3.0, std::span(&product, 1));
    ASSERT_EQ(graph.size(), 5);
    EXPECT_DOUBLE_EQ(graph.constant(two), 2.5);
    EXPECT_EQ(graph.variable(x), 3);
    EXPECT_DOUBLE_EQ(graph.constant(sum), 1.5);
    EXPECT_EQ(ids(graph.children(sum)), ids(terms));
    expect_values(graph.coefficients(sum), {2.0, -1.0});
    EXPECT_DOUBLE_EQ(graph.constant(product), 4.0);
    EXPECT_EQ(ids(graph.children(product)), ids(factors));
    EXPECT_TRUE(graph.coefficients(product).empty());
    EXPECT_DOUBLE_EQ(graph.constant(power), 3.0);
    EXPECT_EQ(graph.op(power), Operator::ConstantPower);
    EXPECT_TRUE(graph.children(x).empty());
}

TEST(GraphTest, Order)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::array pair{y, x};
    const Offset ratio = graph.add(Operator::Division, pair);
    const Offset top = unary(graph, Operator::Exp, ratio);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            EXPECT_LT(child, node);
        }
    }
    EXPECT_EQ(top, graph.size() - 1);
}

TEST(GraphTest, SharedVariable)
{
    Graph graph;
    const Offset x = graph.add_variable(4);
    const Offset y = graph.add_variable(1);
    unary(graph, Operator::Exp, x);
    EXPECT_EQ(graph.add_variable(4), x);
    EXPECT_EQ(graph.add_variable(1), y);
    EXPECT_EQ(graph.add_variable(0), 3);
    EXPECT_EQ(graph.size(), 4);
    EXPECT_EQ(graph.variable(x), 4);
    EXPECT_EQ(graph.variable(y), 1);
}

TEST(GraphTest, OwnSpans)
{
    Graph graph;
    const std::array terms{graph.add_variable(0), graph.add_variable(1)};
    const std::array weights{2.0, 3.0};
    Offset sum = graph.add_sum(1.0, terms, weights);
    Offset product = graph.add(Operator::Product, 2.0, terms);
    Offset most = graph.add(Operator::Max, terms);
    for (int round = 0; round < kRounds; ++round)
    {
        sum = graph.add_sum(1.0, graph.children(sum), graph.coefficients(sum));
        product = graph.add(Operator::Product, 2.0, graph.children(product));
        most = graph.add(Operator::Max, graph.children(most));
    }
    EXPECT_EQ(ids(graph.children(sum)), ids(terms));
    expect_values(graph.coefficients(sum), {2.0, 3.0});
    EXPECT_EQ(ids(graph.children(product)), ids(terms));
    EXPECT_EQ(ids(graph.children(most)), ids(terms));
}

TEST(GraphTest, MissingChild)
{
    Graph graph;
    graph.add_variable(0);
    const Offset ahead = 1;
    const Offset negative = -1;
    EXPECT_EQ(refusal([&] { graph.add(Operator::Exp, std::span(&ahead, 1)); }),
              "invalid argument: expression: node 1 is unknown");
    EXPECT_EQ(refusal([&] { graph.add(Operator::Exp, std::span(&negative, 1)); }),
              "invalid argument: expression: node -1 is unknown");
    EXPECT_EQ(graph.size(), 1);
    EXPECT_EQ(unary(graph, Operator::Exp, 0), 1);
}

TEST(GraphTest, NonfiniteConstant)
{
    Graph graph;
    EXPECT_EQ(constant_refusal(graph, kNan), "invalid argument: expression: constant is NaN");
    EXPECT_EQ(constant_refusal(graph, kInfinity),
              "invalid argument: expression: constant is infinity");
    EXPECT_EQ(constant_refusal(graph, -kInfinity),
              "invalid argument: expression: constant is minus infinity");
    const Offset x = graph.add_variable(0);
    EXPECT_EQ(sum_refusal(graph, kNan, 1.0), "invalid argument: expression: constant is NaN");
    EXPECT_EQ(refusal([&] { graph.add(Operator::Product, kInfinity, std::span(&x, 1)); }),
              "invalid argument: expression: constant is infinity");
    EXPECT_EQ(refusal([&] { graph.add(Operator::ConstantPower, kNan, std::span(&x, 1)); }),
              "invalid argument: expression: constant is NaN");
    EXPECT_EQ(graph.size(), 1);
}

TEST(GraphTest, NonfiniteCoefficient)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::array terms{x, x};
    const std::array weights{1.0, -kInfinity};
    EXPECT_EQ(refusal([&] { graph.add_sum(0.0, terms, weights); }),
              "invalid argument: expression: coefficient 1 is minus infinity");
    EXPECT_EQ(sum_refusal(graph, 0.0, kNan), "invalid argument: expression: coefficient 0 is NaN");
    EXPECT_EQ(graph.size(), 1);
}

TEST(GraphTest, Arity)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::array two{x, x};
    EXPECT_EQ(refusal([&] { graph.add(Operator::Exp, two); }),
              "invalid argument: expression: operator 9 takes 1 child, not 2");
    EXPECT_EQ(refusal([&] { graph.add(Operator::Power, std::span(&x, 1)); }),
              "invalid argument: expression: operator 8 takes 2 children, not 1");
    EXPECT_EQ(refusal([&] { graph.add(Operator::Max, {}); }),
              "invalid argument: expression: operator 16 takes 1 child or more, not 0");
    EXPECT_EQ(refusal([&] { graph.add(Operator::Constant, 1.0, std::span(&x, 1)); }),
              "invalid argument: expression: operator 1 takes 0 children, not 1");
    EXPECT_EQ(refusal([&] { graph.add_sum(0.0, {}, {}); }),
              "invalid argument: expression: operator 3 takes 1 child or more, not 0");
    EXPECT_EQ(graph.size(), 1);
}

TEST(GraphTest, UnaryShape)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::array two{x, x};
    for (const Operator op : kUnary)
    {
        const Offset node = unary(graph, op, x);
        EXPECT_EQ(graph.op(node), op);
        EXPECT_EQ(ids(graph.children(node)), std::vector<Offset>{x});
        EXPECT_EQ(refusal([&] { graph.add(op, two); }), refused(op, "takes 1 child, not 2"));
        EXPECT_EQ(refusal([&] { graph.add(op, 1.0, std::span(&x, 1)); }),
                  refused(op, "takes no payload"));
    }
    EXPECT_EQ(graph.size(), static_cast<Offset>(kUnary.size()) + 1);
}

TEST(GraphTest, ExponentShape)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::span child(&x, 1);
    const std::array two{x, x};
    for (const Operator op : kExponents)
    {
        const Offset node = graph.add(op, 1.5, child);
        EXPECT_EQ(graph.op(node), op);
        EXPECT_DOUBLE_EQ(graph.constant(node), 1.5);
        EXPECT_EQ(ids(graph.children(node)), std::vector<Offset>{x});
        EXPECT_EQ(refusal([&] { graph.add(op, child); }), refused(op, "takes a constant"));
        EXPECT_EQ(refusal([&] { graph.add(op, 1.5, two); }), refused(op, "takes 1 child, not 2"));
        EXPECT_EQ(refusal([&] { graph.add(op, kNan, child); }),
                  "invalid argument: expression: constant is NaN");
        EXPECT_EQ(refusal([&] { graph.add(op, -kInfinity, child); }),
                  "invalid argument: expression: constant is minus infinity");
    }
    EXPECT_EQ(graph.size(), 3);
    const Operator sign = Operator::SignPower;
    EXPECT_EQ(refusal([&] { graph.add(sign, 0.0, child); }),
              refused(sign, "takes a positive constant, not 0"));
    EXPECT_EQ(refusal([&] { graph.add(sign, -1.0, child); }),
              refused(sign, "takes a positive constant, not -1"));
    EXPECT_EQ(refusal([&] { graph.add(sign, -0.0, child); }),
              refused(sign, "takes a positive constant, not -0"));
    EXPECT_EQ(graph.size(), 3);
    const Offset smallest = graph.add(sign, kSmallest, child);
    EXPECT_DOUBLE_EQ(graph.constant(smallest), kSmallest);
    const std::array exponents{0.0, -1.0, -0.0};
    for (const double exponent : exponents)
    {
        EXPECT_EQ(graph.op(graph.add(Operator::ConstantPower, exponent, child)),
                  Operator::ConstantPower);
    }
    EXPECT_EQ(graph.size(), 7);
}

TEST(GraphTest, Form)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    EXPECT_EQ(refusal([&] { graph.add(Operator::Sum, std::span(&x, 1)); }),
              "invalid argument: expression: operator 3 takes coefficients");
    EXPECT_EQ(refusal([&] { graph.add(Operator::Exp, 2.0, std::span(&x, 1)); }),
              "invalid argument: expression: operator 9 takes no payload");
    EXPECT_EQ(refusal([&] { graph.add(Operator::Variable, 0.0, {}); }),
              "invalid argument: expression: operator 2 takes a variable");
    EXPECT_EQ(refusal([&] { graph.add(static_cast<Operator>(99), std::span(&x, 1)); }),
              "invalid argument: expression: operator 99 is unknown");
    EXPECT_EQ(graph.size(), 1);
}

TEST(GraphTest, Counts)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::array terms{x, x};
    const double weight = 1.0;
    EXPECT_EQ(refusal([&] { graph.add_sum(0.0, terms, std::span(&weight, 1)); }),
              "invalid argument: expression: 2 children and 1 coefficient");
    EXPECT_EQ(refusal([&] { graph.add_variable(-1); }),
              "invalid argument: expression: variable -1 is unknown");
    EXPECT_EQ(graph.size(), 1);
}
