#include "sampling.hpp"

#include "bounds.hpp"
#include "core/index.hpp"
#include "expr/enclosure.hpp"
#include "expr/graph.hpp"
#include "expr/interval.hpp"
#include "expr/node.hpp"
#include "reference.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <numbers>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Interval;
using biyos::Offset;
using biyos::Operator;
using biyos::test::Box;
using biyos::test::Check;
using biyos::test::Engine;
using biyos::test::interval_text;
using biyos::test::kVariables;
using biyos::test::Parameters;
using biyos::test::Point;
using biyos::test::text;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kHalfPi = std::numbers::pi / 2;
constexpr double kInverseE = 0x1.78b56362cef38p-2;
constexpr double kReach = 1000.0;
constexpr int kDraws = 24;
constexpr std::size_t kRounds = 4;
constexpr int kReported = 5;

constexpr std::array kSpecial{
    0.0,
    0.5,
    1.0,
    2.0,
    3.0,
    10.0,
    710.0,
    1e6,
    kInverseE,
    kHalfPi,
    std::numbers::pi,
    3 * kHalfPi,
    kInfinity,
};

constexpr std::array kExponents{
    0.0,
    1.0,
    2.0,
    3.0,
    4.0,
    7.0,
    -1.0,
    -2.0,
    -3.0,
    0.5,
    1.5,
    2.5,
    -0.5,
    -1.5,
    1.0 / 3,
    0.1,
    10.0,
};

constexpr std::array kPositive{0.1, 1.0 / 3, 0.5, 1.0, 1.5, 2.0, 3.0};

double unit(Engine &engine)
{
    return static_cast<double>(engine() >> 11) * 0x1p-53;
}

std::size_t pick(Engine &engine, std::size_t count)
{
    return static_cast<std::size_t>(engine() % count);
}

double endpoint(Engine &engine)
{
    const double sign = pick(engine, 2) == 0 ? 1.0 : -1.0;
    if (pick(engine, 3) == 0)
    {
        return sign * kSpecial[pick(engine, kSpecial.size())];
    }
    const double scale = std::pow(10.0, static_cast<double>(pick(engine, 8)) - 4.0);
    return sign * scale * unit(engine);
}

double finite_endpoint(Engine &engine)
{
    double value = endpoint(engine);
    while (!std::isfinite(value))
    {
        value = endpoint(engine);
    }
    return value;
}

Interval interval_of(Engine &engine)
{
    const double first = endpoint(engine);
    const bool point = pick(engine, 6) == 0;
    const double last = point ? first : endpoint(engine);
    return {(std::min)(first, last), (std::max)(first, last)};
}

void add_inside(std::vector<double> &points, Interval box, double value)
{
    if (std::isfinite(value) && box.lower() <= value && value <= box.upper())
    {
        points.push_back(value);
    }
}

double finite_lower(Interval box)
{
    return std::isfinite(box.lower()) ? box.lower() : (std::min)(box.upper(), 0.0) - kReach;
}

double finite_upper(Interval box)
{
    return std::isfinite(box.upper()) ? box.upper() : (std::max)(box.lower(), 0.0) + kReach;
}

std::vector<double> points_of(Interval box, Engine &engine)
{
    std::vector<double> points;
    add_inside(points, box, box.lower());
    add_inside(points, box, box.upper());
    add_inside(points, box, std::nextafter(box.lower(), kInfinity));
    add_inside(points, box, std::nextafter(box.upper(), -kInfinity));
    for (const double special : kSpecial)
    {
        add_inside(points, box, special);
        add_inside(points, box, -special);
    }
    const double low = finite_lower(box);
    const double high = finite_upper(box);
    for (int k = 0; k < kDraws; ++k)
    {
        add_inside(points, box, low + (high - low) * unit(engine));
    }
    return points;
}

bool encloses(Interval interval, double value)
{
    if (std::isnan(value))
    {
        return true;
    }
    return !interval.is_empty() && interval.lower() <= value && value <= interval.upper();
}

std::string report(std::span<const Interval> box, const Point &point, Offset node)
{
    std::string message = "node " + std::to_string(node) + " at";
    for (std::size_t k = 0; k < kVariables; ++k)
    {
        message +=
            " x" + std::to_string(k) + " = " + text(point[k]) + " in " + interval_text(box[k]);
    }
    return message;
}

void expect_point(const Check &check, const Point &point)
{
    std::vector<double> values(check.nodes.size());
    biyos::test::evaluate(check.graph, point, values);
    for (std::size_t node = 0; node < values.size(); ++node)
    {
        if (encloses(check.nodes[node], values[node]) || check.reported++ >= kReported)
        {
            continue;
        }
        ADD_FAILURE() << report(check.box, point, static_cast<Offset>(node)) << ": "
                      << text(values[node]) << " outside " << interval_text(check.nodes[node]);
    }
}

}

biyos::test::Parameters biyos::test::parameters_of(Operator op, Engine &engine)
{
    Parameters parameters{};
    parameters.constant = finite_endpoint(engine);
    for (double &weight : parameters.weights)
    {
        weight = pick(engine, 4) == 0 ? 0.0 : finite_endpoint(engine);
    }
    const bool sign = op == Operator::SignPower;
    const std::size_t choice = pick(engine, sign ? kPositive.size() : kExponents.size());
    parameters.exponent = sign ? kPositive[choice] : kExponents[choice];
    return parameters;
}

Graph biyos::test::operator_graph(Operator op, const Parameters &parameters)
{
    Graph graph;
    std::array<Offset, kVariables> x{};
    for (std::size_t k = 0; k < kVariables; ++k)
    {
        x[k] = graph.add_variable(static_cast<biyos::Index>(k));
    }
    const std::span<const Offset> one(x.data(), 1);
    const std::span<const Offset> two(x.data(), 2);
    switch (op)
    {
    case Operator::Constant:
        graph.add(op, parameters.constant, {});
        break;
    case Operator::Variable:
        break;
    case Operator::Sum:
        graph.add_sum(parameters.constant, x, parameters.weights);
        break;
    case Operator::Product:
        graph.add(op, parameters.constant, x);
        break;
    case Operator::ConstantPower:
    case Operator::SignPower:
        graph.add(op, parameters.exponent, one);
        break;
    case Operator::Division:
    case Operator::Power:
        graph.add(op, two);
        break;
    case Operator::Max:
    case Operator::Min:
        graph.add(op, x);
        break;
    case Operator::Negation:
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
    case Operator::Erf:
        graph.add(op, one);
        break;
    }
    return graph;
}

std::vector<Interval> biyos::test::enclosures(const Graph &graph, std::span<const Interval> box)
{
    std::vector<Interval> nodes(static_cast<std::size_t>(graph.size()), Interval::empty());
    biyos::enclose(graph, box, nodes);
    return nodes;
}

std::string biyos::test::text(double value)
{
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

void biyos::test::expect_sampled(const Check &check, Engine &engine)
{
    std::array<std::vector<double>, kVariables> candidates;
    std::size_t largest = 0;
    for (std::size_t k = 0; k < kVariables; ++k)
    {
        candidates[k] = points_of(check.box[k], engine);
        if (candidates[k].empty())
        {
            return;
        }
        largest = (std::max)(largest, candidates[k].size());
    }
    for (std::size_t round = 0; round < kRounds * largest; ++round)
    {
        Point point{};
        for (std::size_t k = 0; k < kVariables; ++k)
        {
            const std::vector<double> &mine = candidates[k];
            const std::size_t index = round < largest ? round : pick(engine, largest);
            point[k] = mine[index % mine.size()];
        }
        expect_point(check, point);
    }
}

biyos::test::Box biyos::test::box_of(Engine &engine)
{
    Box box{Interval::empty(), Interval::empty(), Interval::empty()};
    for (Interval &interval : box)
    {
        interval = interval_of(engine);
    }
    return box;
}
