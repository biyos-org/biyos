#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/tape.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <utility>
#include <vector>

namespace biyos::test
{

using Ranges = std::vector<std::pair<Offset, Offset>>;

constexpr Index kVariables = 4;
constexpr Index kWide = 20;
constexpr Offset kNodes = 80;
constexpr std::size_t kRoots = 12;
constexpr std::size_t kBatchRoots = 150;
constexpr std::array kBudgets{Offset{1}, Offset{3}, Offset{8}, Offset{30}, kBudget};
constexpr std::array kUnary{
    Operator::Negation, Operator::Exp,   Operator::Log,     Operator::Sqrt, Operator::Sin,
    Operator::Cos,      Operator::Tan,   Operator::Abs,     Operator::Asin, Operator::Acos,
    Operator::Atan,     Operator::Sinh,  Operator::Cosh,    Operator::Tanh, Operator::Asinh,
    Operator::Acosh,    Operator::Atanh, Operator::Entropy, Operator::Erf,
};
constexpr std::array kPowers{Operator::ConstantPower, Operator::SignPower};
constexpr std::array kBinary{Operator::Division, Operator::Power, Operator::Atan2};
constexpr std::array kListed{Operator::Max, Operator::Min};
constexpr std::array kComparisons{
    Operator::LessEqual,
    Operator::GreaterEqual,
    Operator::Equal,
    Operator::Less,
    Operator::Greater,
    Operator::NotEqual,
};
constexpr std::array kConnectives{Operator::And, Operator::Or, Operator::Not};
constexpr std::array kExponents{2.0, 3.0, 0.5, -1.0, 1.5};
constexpr std::array kKinks{-1.0, 0.25, 1.5};
constexpr std::array kHeights{2.0, -0.5, 1.0};
constexpr std::array kSteps{-3.0, -2.0, 0.5, 4.0};
constexpr std::array kLevels{1.0, 1.0, -2.0, 3.0};
constexpr Offset kPiecewiseLinears = 2;
constexpr std::array kSpecial{
    0.0,
    -0.0,
    (std::numeric_limits<Real>::infinity)(),
    -(std::numeric_limits<Real>::infinity)(),
    (std::numeric_limits<Real>::quiet_NaN)(),
    -1.0,
};

inline Bits bits(Real value)
{
    return std::bit_cast<Bits>(value);
}

inline std::vector<Bits> bits(std::span<const Real> values)
{
    std::vector<Bits> words;
    for (const Real value : values)
    {
        words.push_back(bits(value));
    }
    return words;
}

inline std::vector<Offset> ids(std::span<const Offset> nodes)
{
    return {nodes.begin(), nodes.end()};
}

inline bool ordered(const Graph &graph)
{
    for (Offset node = 0; node < graph.size(); ++node)
    {
        for (const Offset child : graph.children(node))
        {
            if (child >= node)
            {
                return false;
            }
        }
    }
    return true;
}

inline std::vector<Offset> cone_of(const Graph &graph, Offset root)
{
    std::vector<bool> seen(to_size(graph.size()), false);
    std::vector<Offset> stack{root};
    std::vector<Offset> cone;
    while (!stack.empty())
    {
        const Offset node = stack.back();
        stack.pop_back();
        if (seen[to_size(node)])
        {
            continue;
        }
        seen[to_size(node)] = true;
        cone.push_back(node);
        const auto children = graph.children(node);
        stack.insert(stack.end(), children.begin(), children.end());
    }
    return cone;
}

inline Ranges pairs_of(std::span<const Range> runs)
{
    Ranges pairs;
    for (const Range &run : runs)
    {
        pairs.emplace_back(run.begin, run.end);
    }
    return pairs;
}

inline Offset constant(Graph &graph, Real value)
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

inline Offset branch(Graph &graph, Offset condition, Offset then, Offset otherwise)
{
    return graph.add(Operator::If, std::array{condition, then, otherwise});
}

inline Offset power(Graph &graph, Offset base, Real exponent)
{
    return graph.add(Operator::ConstantPower, exponent, std::span(&base, 1));
}

inline Offset sign_power(Graph &graph, Offset base, Real exponent)
{
    return graph.add(Operator::SignPower, exponent, std::span(&base, 1));
}

inline Offset listed(Graph &graph, Operator op, const std::vector<Offset> &children)
{
    return graph.add(op, children);
}

inline std::vector<Offset> variables(Graph &graph, Index size)
{
    std::vector<Offset> nodes;
    for (Index k = 0; k < size; ++k)
    {
        nodes.push_back(graph.add_variable(k));
    }
    return nodes;
}

inline Offset waves_of(Graph &graph, const std::vector<Offset> &x)
{
    std::vector<Offset> waves;
    for (const Offset variable : x)
    {
        waves.push_back(unary(graph, Operator::Sin, variable));
    }
    return graph.add_sum(0.0, waves, std::vector<Real>(waves.size(), 1.0));
}

inline Offset arrow(Graph &graph, const std::vector<Offset> &x)
{
    const Offset grown = unary(graph, Operator::Exp, x[0]);
    return graph.add(Operator::Product, 1.0, std::array{grown, waves_of(graph, x)});
}

inline Offset clique(Graph &graph, const std::vector<Offset> &x, Offset tail, Index wide)
{
    const std::vector<Offset> head(x.begin(), x.begin() + wide);
    const Offset total = graph.add_sum(0.0, head, std::vector<Real>(head.size(), 1.0));
    const std::array terms{unary(graph, Operator::Exp, total), tail};
    const Offset both = graph.add_sum(0.0, terms, std::vector<Real>(terms.size(), 1.0));
    return unary(graph, Operator::Abs, both);
}

inline Offset band(Graph &graph, const std::vector<Offset> &x)
{
    std::vector<Offset> waves;
    for (std::size_t k = 0; k + 2 < x.size(); ++k)
    {
        const std::array three{x[k], x[k + 1], x[k + 2]};
        const Offset window = graph.add_sum(0.0, three, std::vector<Real>(3, 1.0));
        waves.push_back(unary(graph, Operator::Sin, window));
    }
    const Offset total = graph.add_sum(0.0, waves, std::vector<Real>(waves.size(), 1.0));
    return unary(graph, Operator::Abs, total);
}

inline void add_pwls(Graph &graph)
{
    graph.add_piecewise_linear(kKinks, kHeights);
    graph.add_piecewise_linear(kSteps, kLevels);
}

inline Offset piecewise(Graph &graph, Offset pwl, Offset child)
{
    return graph.add_piecewise(pwl, std::span(&child, 1));
}

inline std::array<Offset, 3> connectives(Graph &graph, Offset x, Offset y, Offset z)
{
    const Offset below = binary(graph, Operator::Less, y, x);
    const Offset above = binary(graph, Operator::Greater, x, z);
    const std::array conditions{below, above};
    return {
        graph.add(Operator::And, conditions),
        graph.add(Operator::Or, conditions),
        unary(graph, Operator::Not, below),
    };
}

inline std::vector<Offset> every_operator(Graph &graph)
{
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset z = graph.add_variable(2);
    const Offset w = graph.add_variable(3);
    const Offset pwl = graph.add_piecewise_linear(kKinks, kHeights);
    const std::array xy{x, y};
    const std::array xyz{x, y, z};
    std::vector<Offset> roots{
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
        piecewise(graph, pwl, y),
        binary(graph, Operator::Atan2, z, x),
        binary(graph, Operator::LessEqual, x, y),
        binary(graph, Operator::GreaterEqual, x, y),
        binary(graph, Operator::Equal, x, x),
        binary(graph, Operator::Less, z, x),
        binary(graph, Operator::Greater, z, x),
        binary(graph, Operator::NotEqual, y, z),
    };
    const std::array logic = connectives(graph, x, y, z);
    roots.insert(roots.end(), logic.begin(), logic.end());
    const Offset above = binary(graph, Operator::Greater, x, y);
    roots.push_back(branch(graph, above, x, z));
    return roots;
}

class Random
{
  public:
    explicit Random(std::uint64_t seed) : engine_(seed) {}

    std::size_t below(std::size_t n) { return static_cast<std::size_t>(engine_() % n); }

    Real uniform(Real low, Real high)
    {
        const Real unit = static_cast<Real>(engine_() >> 11) * 0x1p-53;
        return low + (high - low) * unit;
    }

  private:
    std::mt19937_64 engine_;
};

inline Offset products(Graph &graph, const std::vector<Offset> &x, Random &random)
{
    std::vector<Offset> terms;
    for (std::size_t k = 0; k < x.size(); ++k)
    {
        const Offset wave = unary(graph, Operator::Sin, x[random.below(x.size())]);
        const std::array factors{wave, x[random.below(x.size())], x[random.below(x.size())]};
        terms.push_back(graph.add(Operator::Product, 1.0, factors));
    }
    const Offset total = graph.add_sum(0.0, terms, std::vector<Real>(terms.size(), 1.0));
    return unary(graph, Operator::Abs, total);
}

inline std::vector<Offset> picks(Random &random, const Graph &graph, std::size_t n)
{
    std::vector<Offset> nodes;
    for (std::size_t k = 0; k < n; ++k)
    {
        nodes.push_back(static_cast<Offset>(random.below(static_cast<std::size_t>(graph.size()))));
    }
    return nodes;
}

inline Offset sum(Graph &graph, Random &random, Real constant, const std::vector<Offset> &terms)
{
    std::vector<Real> weights;
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        weights.push_back(random.uniform(-2.0, 2.0));
    }
    return graph.add_sum(constant, terms, weights);
}

inline Offset powered(Graph &graph, Random &random, Offset base)
{
    const Operator op = kPowers[random.below(kPowers.size())];
    const Real exponent = kExponents[random.below(kExponents.size())];
    const Real allowed = op == Operator::SignPower ? std::fabs(exponent) : exponent;
    return graph.add(op, allowed, std::span(&base, 1));
}

inline Offset grow(Graph &graph, Random &random)
{
    const std::vector<Offset> children = picks(random, graph, 1 + random.below(4));
    const std::size_t kind = random.below(9);
    const Real number = random.uniform(-2.0, 2.0);
    switch (kind)
    {
    case 0:
        return constant(graph, number);
    case 1:
        return unary(graph, kUnary[random.below(kUnary.size())], children[0]);
    case 2:
        return powered(graph, random, children[0]);
    case 3:
        return binary(graph,
                      kBinary[random.below(kBinary.size())],
                      children.front(),
                      children.back());
    case 4:
        return sum(graph, random, number, children);
    case 5:
        return graph.add(Operator::Product, number, children);
    case 6:
        return listed(graph, kListed[random.below(2)], children);
    case 7:
        return binary(graph,
                      kComparisons[random.below(kComparisons.size())],
                      children.front(),
                      children.back());
    default:
        return piecewise(graph, static_cast<Offset>(random.below(kPiecewiseLinears)), children[0]);
    }
}

inline Graph random_graph(Random &random)
{
    Graph graph;
    add_pwls(graph);
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

inline Real coordinate(Random &random)
{
    if (random.below(4) == 0)
    {
        return kSpecial[random.below(kSpecial.size())];
    }
    return random.uniform(-0.5, 2.0);
}

inline std::vector<Real> random_point(Random &random)
{
    std::vector<Real> point;
    for (Index k = 0; k < kVariables; ++k)
    {
        point.push_back(coordinate(random));
    }
    return point;
}

}
