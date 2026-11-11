#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "expr/partials.hpp"
#include "expr/tape.hpp"
#include "expr/value.hpp"
#include "graphs.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace
{

using biyos::Block;
using biyos::Graph;
using biyos::Offset;
using biyos::Operator;
using biyos::Tape;
using biyos::Workspace;
using biyos::test::binary;
using biyos::test::constant;
using biyos::test::every_operator;
using biyos::test::listed;
using biyos::test::power;
using biyos::test::sign_power;
using biyos::test::unary;

using Partials = std::vector<double>;

constexpr double kInfinity = (std::numeric_limits<double>::infinity)();
constexpr double kNan = (std::numeric_limits<double>::quiet_NaN)();

Partials partials_at(const Graph &graph, Offset root, std::span<const double> point)
{
    const Tape tape(graph, std::span(&root, 1));
    Workspace workspace(tape);
    const Block &block = tape.blocks()[0];
    biyos::linearize(block, point, workspace);
    const std::span<const double> all = biyos::region(block, std::span(workspace.partials));
    const std::size_t last = block.instructions.size() - 1;
    const std::uint32_t first = block.places[last];
    const std::span<const double> mine = all.subspan(first, block.places[last + 1] - first);
    return {mine.begin(), mine.end()};
}

void expect_partials(const Partials &found, const Partials &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        SCOPED_TRACE(k);
        if (std::isnan(wanted[k]))
        {
            EXPECT_TRUE(std::isnan(found[k])) << found[k];
            continue;
        }
        EXPECT_DOUBLE_EQ(found[k], wanted[k]);
    }
}

std::vector<Partials> cmath_partials(double x, double y, double z, double w)
{
    const double secant = 1.0 / std::cos(x);
    const double tanh_secant = 1.0 / std::cosh(z);
    return {
        {},
        {},
        {1.0},
        {2.0 * y * z, 2.0 * x * z, 2.0 * x * y},
        {-1.0},
        {1.0 / y, -x / (y * y)},
        {2.5 * std::pow(x, 1.5)},
        {y * std::pow(x, y - 1.0), std::pow(x, y) * std::log(x)},
        {std::exp(z)},
        {1.0 / x},
        {0.5 / std::sqrt(y)},
        {std::cos(z)},
        {-std::sin(z)},
        {secant * secant},
        {-1.0},
        {1.0},
        {2.0},
        {1.0 / std::sqrt(1.0 - w * w)},
        {-1.0 / std::sqrt(1.0 - w * w)},
        {1.0 / (1.0 + z * z)},
        {std::cosh(z)},
        {std::sinh(z)},
        {tanh_secant * tanh_secant},
        {1.0 / std::sqrt(z * z + 1.0)},
        {1.0 / std::sqrt(y * y - 1.0)},
        {1.0 / (1.0 - w * w)},
        {std::log(x) + 1.0},
        {1.5 * std::sqrt(std::fabs(z))},
        {2.0 / std::sqrt(std::numbers::pi) * std::exp(-z * z)},
    };
}

void expect_each(const Graph &graph,
                 std::span<const Offset> roots,
                 std::span<const double> point,
                 const std::vector<Partials> &wanted)
{
    ASSERT_EQ(roots.size(), wanted.size());
    for (std::size_t k = 0; k < roots.size(); ++k)
    {
        SCOPED_TRACE(k);
        expect_partials(partials_at(graph, roots[k], point), wanted[k]);
    }
}

}

TEST(PartialTest, Operators)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    const std::array point{0.7, 1.9, -1.3, 0.3};
    expect_each(graph, roots, point, cmath_partials(point[0], point[1], point[2], point[3]));
}

TEST(PartialTest, Zeros)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset two = constant(graph, 2.0);
    const Offset minus_two = constant(graph, -2.0);
    const Offset zero = constant(graph, 0.0);
    const std::vector<Offset> roots{
        unary(graph, Operator::Sqrt, x),
        unary(graph, Operator::Log, x),
        power(graph, x, 0.5),
        power(graph, x, -2.0),
        power(graph, x, 0.0),
        unary(graph, Operator::Abs, x),
        binary(graph, Operator::Power, x, two),
        binary(graph, Operator::Power, x, minus_two),
        binary(graph, Operator::Power, x, zero),
        unary(graph, Operator::Entropy, x),
        sign_power(graph, x, 1.5),
        sign_power(graph, x, 0.5),
        sign_power(graph, x, 1.0),
    };
    const std::vector<Partials> wanted{
        {kInfinity},
        {kInfinity},
        {kInfinity},
        {-kInfinity},
        {0.0},
        {1.0},
        {0.0, 0.0},
        {-kInfinity, -kInfinity},
        {0.0, -kInfinity},
        {-kInfinity},
        {0.0},
        {kInfinity},
        {1.0},
    };
    for (const double nothing : {0.0, -0.0})
    {
        SCOPED_TRACE(nothing);
        expect_each(graph, roots, std::array{nothing}, wanted);
    }
    const std::array ratio{binary(graph, Operator::Division, two, x)};
    expect_each(graph, ratio, std::array{0.0}, {{kInfinity, -kInfinity}});
    expect_each(graph, ratio, std::array{-0.0}, {{-kInfinity, -kInfinity}});
}

TEST(PartialTest, Domain)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset two = constant(graph, 2.0);
    const Offset zero = constant(graph, 0.0);
    const std::vector<Offset> roots{
        unary(graph, Operator::Log, x),
        unary(graph, Operator::Sqrt, x),
        binary(graph, Operator::Power, x, two),
        power(graph, x, 0.0),
        binary(graph, Operator::Power, x, zero),
        unary(graph, Operator::Atanh, x),
        unary(graph, Operator::Acosh, x),
        unary(graph, Operator::Entropy, x),
    };
    const std::vector<Partials> below{
        {kNan},
        {kNan},
        {-4.0, kNan},
        {0.0},
        {0.0, kNan},
        {kNan},
        {kNan},
        {kNan},
    };
    expect_each(graph, roots, std::array{-2.0}, below);
    expect_each(graph, std::span(roots).subspan(6, 1), std::array{-1.0}, {{kNan}});
    expect_each(graph, std::span(roots).subspan(3, 2), std::array{kNan}, {{0.0}, {0.0, kNan}});

    const std::vector<Offset> edges{
        unary(graph, Operator::Asin, x),
        unary(graph, Operator::Acos, x),
        unary(graph, Operator::Acosh, x),
        unary(graph, Operator::Atanh, x),
        unary(graph, Operator::Entropy, x),
    };
    expect_each(graph,
                edges,
                std::array{1.0},
                {{kInfinity}, {-kInfinity}, {kInfinity}, {kInfinity}, {1.0}});
    expect_each(graph,
                edges,
                std::array{1.5},
                {{kNan}, {kNan}, {1.0 / std::sqrt(1.25)}, {kNan}, {std::log(1.5) + 1.0}});
    expect_each(graph,
                std::span(edges).subspan(2),
                std::array{0.5},
                {{kNan}, {4.0 / 3.0}, {std::log(0.5) + 1.0}});
}

TEST(PartialTest, Undefined)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset one = constant(graph, 1.0);
    const std::vector<Offset> roots{
        unary(graph, Operator::Abs, x),
        sign_power(graph, x, 1.5),
        unary(graph, Operator::Negation, x),
        graph.add_sum(0.0, std::array{x, one}, std::array{1.0, 1.0}),
        graph.add(Operator::Product, 1.0, std::array{x, one}),
        listed(graph, Operator::Max, {one, x}),
        unary(graph, Operator::Sin, x),
    };
    const std::vector<Partials>
        wanted{{kNan}, {kNan}, {kNan}, {kNan}, {kNan, kNan}, {kNan}, {kNan}};
    expect_each(graph, roots, std::array{kNan}, wanted);
}

TEST(PartialTest, Accuracy)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::array waves{unary(graph, Operator::Tanh, x)};
    for (const double far : {15.0, 19.0, 20.0, -20.0})
    {
        SCOPED_TRACE(far);
        const double scale = std::cosh(far);
        expect_each(graph, waves, std::array{far}, {{1.0 / (scale * scale)}});
    }
    const std::array arcs{unary(graph, Operator::Acosh, x)};
    for (const double huge : {1e200, 1e300})
    {
        SCOPED_TRACE(huge);
        expect_each(graph, arcs, std::array{huge}, {{1.0 / huge}});
    }
}

TEST(PartialTest, Extrema)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset z = graph.add_variable(2);
    const Offset nan = unary(graph, Operator::Log, y);
    const std::vector<Offset> roots{
        listed(graph, Operator::Max, {x, y, z}),
        listed(graph, Operator::Min, {x, y, z}),
        listed(graph, Operator::Max, {y}),
        listed(graph, Operator::Max, {x, x}),
        listed(graph, Operator::Max, {nan, z}),
        listed(graph, Operator::Max, {z, nan}),
        listed(graph, Operator::Min, {x, nan, y}),
        listed(graph, Operator::Max, {z, x, z}),
    };
    expect_each(graph,
                roots,
                std::array{0.5, -2.0, 3.0},
                {{2.0}, {1.0}, {0.0}, {0.0}, {kNan}, {kNan}, {kNan}, {0.0}});
    expect_each(graph, std::span(roots).first(2), std::array{1.0, 1.0, 1.0}, {{0.0}, {0.0}});
}
