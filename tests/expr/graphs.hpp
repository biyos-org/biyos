#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "expr/tape.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <vector>

namespace biyos::test
{

constexpr Index kVariables = 4;
constexpr Offset kNodes = 80;
constexpr std::size_t kRoots = 12;
constexpr std::array kBudgets{Offset{1}, Offset{3}, Offset{8}, Offset{30}, kBudget};
constexpr std::array kUnary{
    Operator::Negation, Operator::Exp,   Operator::Log,     Operator::Sqrt, Operator::Sin,
    Operator::Cos,      Operator::Tan,   Operator::Abs,     Operator::Asin, Operator::Acos,
    Operator::Atan,     Operator::Sinh,  Operator::Cosh,    Operator::Tanh, Operator::Asinh,
    Operator::Acosh,    Operator::Atanh, Operator::Entropy, Operator::Erf,
};
constexpr std::array kPowers{Operator::ConstantPower, Operator::SignPower};
constexpr std::array kBinary{Operator::Division, Operator::Power};
constexpr std::array kListed{Operator::Max, Operator::Min};
constexpr std::array kExponents{2.0, 3.0, 0.5, -1.0, 1.5};
constexpr std::array kSpecial{
    0.0,
    -0.0,
    (std::numeric_limits<double>::infinity)(),
    -(std::numeric_limits<double>::infinity)(),
    (std::numeric_limits<double>::quiet_NaN)(),
    -1.0,
};

inline Offset constant(Graph &graph, double value)
{
    return graph.add(Operator::Constant, value, {});
}

inline Offset unary(Graph &graph, Operator op, Offset child)
{
    return graph.add(op, std::span(&child, 1));
}

inline Offset binary(Graph &graph, Operator op, Offset left, Offset right)
{
    const std::array children{left, right};
    return graph.add(op, children);
}

inline Offset power(Graph &graph, Offset base, double exponent)
{
    return graph.add(Operator::ConstantPower, exponent, std::span(&base, 1));
}

inline Offset sign_power(Graph &graph, Offset base, double exponent)
{
    return graph.add(Operator::SignPower, exponent, std::span(&base, 1));
}

inline Offset listed(Graph &graph, Operator op, const std::vector<Offset> &children)
{
    return graph.add(op, children);
}

inline std::vector<Offset> every_operator(Graph &graph)
{
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset z = graph.add_variable(2);
    const Offset w = graph.add_variable(3);
    const std::array xy{x, y};
    const std::array xyz{x, y, z};
    return {
        constant(graph, 2.5),
        x,
        graph.add_sum(1.5, xy, std::array{2.0, -3.0}),
        graph.add(Operator::Product, 2.0, xyz),
        unary(graph, Operator::Negation, z),
        binary(graph, Operator::Division, x, y),
        power(graph, x, 2.5),
        binary(graph, Operator::Power, x, y),
        unary(graph, Operator::Exp, z),
        unary(graph, Operator::Log, x),
        unary(graph, Operator::Sqrt, y),
        unary(graph, Operator::Sin, z),
        unary(graph, Operator::Cos, z),
        unary(graph, Operator::Tan, x),
        unary(graph, Operator::Abs, z),
        graph.add(Operator::Max, xyz),
        graph.add(Operator::Min, xyz),
        unary(graph, Operator::Asin, w),
        unary(graph, Operator::Acos, w),
        unary(graph, Operator::Atan, z),
        unary(graph, Operator::Sinh, z),
        unary(graph, Operator::Cosh, z),
        unary(graph, Operator::Tanh, z),
        unary(graph, Operator::Asinh, z),
        unary(graph, Operator::Acosh, y),
        unary(graph, Operator::Atanh, w),
        unary(graph, Operator::Entropy, x),
        sign_power(graph, z, 1.5),
        unary(graph, Operator::Erf, z),
    };
}

class Random
{
  public:
    explicit Random(std::uint64_t seed) : engine_(seed) {}

    std::size_t below(std::size_t count) { return static_cast<std::size_t>(engine_() % count); }

    double uniform(double low, double high)
    {
        const double unit = static_cast<double>(engine_() >> 11) * 0x1p-53;
        return low + (high - low) * unit;
    }

  private:
    std::mt19937_64 engine_;
};

inline std::vector<Offset> picks(Random &random, const Graph &graph, std::size_t count)
{
    std::vector<Offset> nodes;
    for (std::size_t k = 0; k < count; ++k)
    {
        nodes.push_back(static_cast<Offset>(random.below(static_cast<std::size_t>(graph.size()))));
    }
    return nodes;
}

inline Offset sum(Graph &graph, Random &random, double constant, const std::vector<Offset> &terms)
{
    std::vector<double> weights;
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        weights.push_back(random.uniform(-2.0, 2.0));
    }
    return graph.add_sum(constant, terms, weights);
}

inline Offset powered(Graph &graph, Random &random, Offset base)
{
    const Operator op = kPowers[random.below(kPowers.size())];
    const double exponent = kExponents[random.below(kExponents.size())];
    const double allowed = op == Operator::SignPower ? std::fabs(exponent) : exponent;
    return graph.add(op, allowed, std::span(&base, 1));
}

inline Offset grow(Graph &graph, Random &random)
{
    const std::vector<Offset> children = picks(random, graph, 1 + random.below(4));
    const std::size_t kind = random.below(7);
    const double number = random.uniform(-2.0, 2.0);
    switch (kind)
    {
    case 0:
        return constant(graph, number);
    case 1:
        return unary(graph, kUnary[random.below(kUnary.size())], children[0]);
    case 2:
        return powered(graph, random, children[0]);
    case 3:
        return binary(graph, kBinary[random.below(2)], children.front(), children.back());
    case 4:
        return sum(graph, random, number, children);
    case 5:
        return graph.add(Operator::Product, number, children);
    default:
        return listed(graph, kListed[random.below(2)], children);
    }
}

inline Graph random_graph(Random &random)
{
    Graph graph;
    for (Index k = 0; k < kVariables; ++k)
    {
        graph.add_variable(k);
    }
    while (graph.size() < kNodes)
    {
        grow(graph, random);
    }
    return graph;
}

inline double coordinate(Random &random)
{
    if (random.below(4) == 0)
    {
        return kSpecial[random.below(kSpecial.size())];
    }
    return random.uniform(-0.5, 2.0);
}

inline std::vector<double> random_point(Random &random)
{
    std::vector<double> point;
    for (Index k = 0; k < kVariables; ++k)
    {
        point.push_back(coordinate(random));
    }
    return point;
}

}
