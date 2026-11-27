#include "draws.hpp"

#include "biyos/biyos.h"
#include "core/numbers.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "sampling.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Interval;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::test::Engine;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr std::size_t kOperators = BIYOS_OP_ERF - BIYOS_OP_CONST + 1;
constexpr std::size_t kRecent = 4;
constexpr std::size_t kWidest = 3;
constexpr std::array kEnds{-3.0, -2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 3.0};
constexpr std::array kWeights{-2.0, -1.0, -0.5, 0.5, 1.0, 2.0, 0.0};
constexpr std::array kConstants{0.0, 1.0, -1.0, 2.0, 0.5};
constexpr std::array kExponents{2.0, 3.0, 0.5, 1.5, -1.0, -2.0, -0.5, 1.0, 0.0, 4.0};
constexpr std::array kSignExponents{0.5, 1.5, 3.0, 1.0};
constexpr std::array kKinks{-2.0, 0.0, 1.0};
constexpr std::array kHeights{1.0, 0.0, 2.0};

std::size_t pick(Engine &engine, std::size_t n)
{
    return static_cast<std::size_t>(engine() % n);
}

Real unit(Engine &engine)
{
    return static_cast<Real>(engine() >> 11) * 0x1p-53;
}

template <typename T, std::size_t N> T one_of(const std::array<T, N> &values, Engine &engine)
{
    return values[pick(engine, N)];
}

Offset child_of(const Graph &graph, Engine &engine)
{
    const auto size = static_cast<std::size_t>(graph.size());
    if (pick(engine, 2) == 0)
    {
        return static_cast<Offset>(size - 1 - pick(engine, (std::min)(size, kRecent)));
    }
    return static_cast<Offset>(pick(engine, size));
}

std::size_t arity_of(Operator op, Engine &engine)
{
    switch (op)
    {
    case Operator::Constant:
    case Operator::Variable:
        return 0;
    case Operator::Sum:
    case Operator::Product:
    case Operator::Max:
    case Operator::Min:
        return 1 + pick(engine, kWidest);
    case Operator::Division:
    case Operator::Power:
    case Operator::Atan2:
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
        return 2;
    case Operator::Negation:
    case Operator::ConstantPower:
    case Operator::Exp:
    case Operator::Log:
    case Operator::Sqrt:
    case Operator::Sin:
    case Operator::Cos:
    case Operator::Tan:
    case Operator::Abs:
    case Operator::Asin:
    case Operator::Acos:
    case Operator::Atan:
    case Operator::Sinh:
    case Operator::Cosh:
    case Operator::Tanh:
    case Operator::Asinh:
    case Operator::Acosh:
    case Operator::Atanh:
    case Operator::Entropy:
    case Operator::SignPower:
    case Operator::Erf:
    case Operator::PiecewiseLinear:
        return 1;
    }
    return 1;
}

void grow(Graph &graph, Engine &engine)
{
    const auto op = static_cast<Operator>(BIYOS_OP_CONST + pick(engine, kOperators));
    std::vector<Offset> children(arity_of(op, engine));
    for (Offset &child : children)
    {
        child = child_of(graph, engine);
    }
    switch (biyos::payload_of(op))
    {
    case biyos::Payload::None:
        graph.add(op, children);
        break;
    case biyos::Payload::Variable:
        graph.add_variable(static_cast<biyos::Index>(pick(engine, biyos::test::kVariables)));
        break;
    case biyos::Payload::Coefficients:
    {
        std::vector<Real> weights(children.size());
        for (Real &weight : weights)
        {
            weight = one_of(kWeights, engine);
        }
        graph.add_sum(one_of(kConstants, engine), children, weights);
        break;
    }
    case biyos::Payload::Constant:
        if (op == Operator::Constant)
        {
            graph.add(op, one_of(kConstants, engine), {});
        }
        else if (op == Operator::Product)
        {
            graph.add(op, one_of(kWeights, engine), children);
        }
        else
        {
            const bool sign = op == Operator::SignPower;
            graph.add(op,
                      sign ? one_of(kSignExponents, engine) : one_of(kExponents, engine),
                      children);
        }
        break;
    case biyos::Payload::Points:
        graph.add_piecewise(graph.add_piecewise_linear(kKinks, kHeights), children);
        break;
    }
}

Real end_of(Engine &engine)
{
    switch (pick(engine, 10))
    {
    case 0:
        return -kInfinity;
    case 1:
        return kInfinity;
    case 2:
    case 3:
        return -3.0 + 6.0 * unit(engine);
    default:
        return one_of(kEnds, engine);
    }
}

Interval interval_of(Engine &engine)
{
    const Real first = end_of(engine);
    const Real last = pick(engine, 8) == 0 ? first : end_of(engine);
    const Interval interval((std::min)(first, last), (std::max)(first, last));
    return interval.is_empty() ? Interval(0.0, 1.0) : interval;
}

}

Graph biyos::test::draw_graph(Engine &engine, std::size_t size)
{
    Graph graph;
    for (std::size_t k = 0; k < kVariables; ++k)
    {
        graph.add_variable(static_cast<Index>(k));
    }
    while (static_cast<std::size_t>(graph.size()) < kVariables + size)
    {
        grow(graph, engine);
    }
    return graph;
}

biyos::test::Box biyos::test::draw_box(Engine &engine)
{
    Box box{Interval::empty(), Interval::empty(), Interval::empty()};
    for (Interval &interval : box)
    {
        interval = interval_of(engine);
    }
    return box;
}
