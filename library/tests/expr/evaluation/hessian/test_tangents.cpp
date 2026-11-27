#include "core/numbers.hpp"
#include "expr/evaluation/derivatives/partials.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/tangents.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"
#include "graphs.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace
{

using biyos::Bits;
using biyos::Block;
using biyos::evaluate;
using biyos::Frame;
using biyos::Graph;
using biyos::Index;
using biyos::Instruction;
using biyos::keep;
using biyos::Offset;
using biyos::Operator;
using biyos::partial_tangents;
using biyos::partials_of;
using biyos::place;
using biyos::Real;
using biyos::region;
using biyos::row;
using biyos::Tape;
using biyos::widest_product;
using biyos::Workspace;
using biyos::test::binary;
using biyos::test::constant;
using biyos::test::every_operator;
using biyos::test::power;
using biyos::test::sign_power;
using biyos::test::unary;

using Values = std::vector<Real>;

constexpr Real kStep = 1e-5;
constexpr Real kBound = 1e-6;
constexpr Real kRounding = 1e-12;
constexpr Real kNan = (std::numeric_limits<Real>::quiet_NaN)();
constexpr Real kInfinity = (std::numeric_limits<Real>::infinity)();
constexpr std::array kInterior{0.7, 1.9, -1.3, 0.3};
constexpr std::array kDirection{0.6, -0.4, 0.9, 0.5};

struct Linear
{
    Values partials;
    Values tangents;
};

void seed(const Instruction &instruction,
          std::span<const Real> direction,
          std::size_t lanes,
          std::span<Real> slots)
{
    ASSERT_TRUE(instruction.op == Operator::Variable || instruction.op == Operator::Constant);
    if (instruction.op == Operator::Variable)
    {
        const std::span<const Real> mine = row(direction, place(instruction.payload), lanes);
        std::ranges::copy(mine, row(slots, instruction.target, lanes).begin());
    }
}

Frame frame_of(const Block &block, std::span<const Real> point, Workspace &workspace)
{
    return {
        .point = point,
        .operands = block.operands,
        .weights = block.weights,
        .slots = workspace.slots,
        .result = workspace.result,
        .spare = workspace.spare,
        .lanes = workspace.lanes,
    };
}

Linear linear_at(const Graph &graph,
                 Offset root,
                 std::span<const Real> point,
                 std::span<const Real> direction,
                 std::size_t lanes = 1)
{
    const Tape tape(graph, std::span(&root, 1));
    Workspace workspace(tape);
    const Block &block = tape.blocks()[0];
    const Frame frame = frame_of(block, point, workspace);
    const std::span<Real> all = region(block, std::span(workspace.partials), 1);
    Values slots(tape.width() * lanes, 0.0);
    Values spare(lanes, 0.0);
    Values prefix(widest_product(tape), 0.0);
    const std::size_t last = block.instructions.size() - 1;
    for (std::size_t k = 0; k < last; ++k)
    {
        evaluate(block.instructions[k], frame);
        seed(block.instructions[k], direction, lanes, slots);
        keep(block.instructions[k], frame);
    }
    const Instruction &instruction = block.instructions[last];
    const std::size_t start = block.places[last];
    const std::span<Real> mine = all.subspan(start, block.places[last + 1] - start);
    Values out(mine.size() * lanes, 0.0);
    evaluate(instruction, frame);
    partials_of(instruction, frame, mine);
    partial_tangents(instruction,
                     frame,
                     {.slots = slots, .spare = spare, .prefix = prefix, .lanes = lanes},
                     mine,
                     out);
    return {{mine.begin(), mine.end()}, out};
}

Values shifted(std::span<const Real> point, std::span<const Real> direction, Real step)
{
    Values moved(point.begin(), point.end());
    for (std::size_t j = 0; j < moved.size(); ++j)
    {
        moved[j] += step * direction[j];
    }
    return moved;
}

Values differences(const Graph &graph,
                   Offset root,
                   std::span<const Real> point,
                   std::span<const Real> direction,
                   Real step)
{
    const Values ahead =
        linear_at(graph, root, shifted(point, direction, step), direction).partials;
    const Values behind =
        linear_at(graph, root, shifted(point, direction, -step), direction).partials;
    Values slopes(ahead.size(), 0.0);
    for (std::size_t k = 0; k < slopes.size(); ++k)
    {
        slopes[k] = (ahead[k] - behind[k]) / (2.0 * step);
    }
    return slopes;
}

Real tolerance(Real difference)
{
    return kBound * (1.0 + std::fabs(difference));
}

bool converged(Real fine, Real coarse)
{
    return std::isfinite(fine) && std::fabs(fine - coarse) <= tolerance(fine);
}

void expect_differences(const Graph &graph,
                        Offset root,
                        std::span<const Real> point,
                        std::span<const Real> direction)
{
    const Values found = linear_at(graph, root, point, direction).tangents;
    const Values fine = differences(graph, root, point, direction, kStep);
    const Values coarse = differences(graph, root, point, direction, 2.0 * kStep);
    std::size_t checked = 0;
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        SCOPED_TRACE(k);
        if (converged(fine[k], coarse[k]))
        {
            EXPECT_NEAR(found[k], fine[k], tolerance(fine[k]));
            ++checked;
        }
    }
    EXPECT_EQ(checked, found.size());
}

void expect_values(const Values &found, const Values &wanted)
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
        EXPECT_EQ(std::bit_cast<Bits>(found[k]), std::bit_cast<Bits>(wanted[k]))
            << found[k] << " against " << wanted[k];
    }
}

Real pairwise(Real c, std::span<const Real> x, std::span<const Real> t, std::size_t k)
{
    Real sum = 0.0;
    for (std::size_t j = 0; j < x.size(); ++j)
    {
        Real term = j == k ? 0.0 : c * t[j];
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            term *= i == k || i == j ? 1.0 : x[i];
        }
        sum += term;
    }
    return sum;
}

}

TEST(TangentTest, Operators)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    for (std::size_t k = 0; k < roots.size(); ++k)
    {
        SCOPED_TRACE(k);
        expect_differences(graph, roots[k], kInterior, kDirection);
    }
}

TEST(TangentTest, Edges)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const std::vector<Offset> roots{
        unary(graph, Operator::Log, x),
        unary(graph, Operator::Sqrt, x),
        unary(graph, Operator::Entropy, x),
        power(graph, x, 0.5),
        power(graph, x, -1.5),
        binary(graph, Operator::Power, x, y),
        binary(graph, Operator::Division, y, x),
        sign_power(graph, x, 1.5),
        sign_power(graph, x, 2.5),
    };
    const std::vector<Offset> arcs{
        unary(graph, Operator::Asin, x),
        unary(graph, Operator::Acos, x),
        unary(graph, Operator::Atanh, x),
        unary(graph, Operator::Acosh, y),
    };
    const std::array direction{0.8, 0.3};
    for (const Real near : {0.1, 0.05})
    {
        SCOPED_TRACE(near);
        for (const Offset root : roots)
        {
            expect_differences(graph, root, std::array{near, 1.0 + near}, direction);
        }
        for (const Offset root : arcs)
        {
            expect_differences(graph, root, std::array{1.0 - near, 1.0 + near}, direction);
        }
    }
    for (const Offset root : std::span(roots).last(2))
    {
        expect_differences(graph, root, std::array{-0.3, 1.0}, direction);
    }
}

TEST(TangentTest, Product)
{
    Graph graph;
    const std::array x{0.9, -1.3, 0.0, 2.1, 0.4, -0.7, 1.6};
    const std::array t{0.5, 0.0, -0.8, 1.2, -0.3, 0.7, 0.0};
    std::vector<Offset> factors;
    for (std::size_t k = 0; k < x.size(); ++k)
    {
        factors.push_back(graph.add_variable(static_cast<Index>(k)));
    }
    const Offset root = graph.add(Operator::Product, -1.5, factors);
    const Values found = linear_at(graph, root, x, t).tangents;
    ASSERT_EQ(found.size(), x.size());
    for (std::size_t k = 0; k < x.size(); ++k)
    {
        SCOPED_TRACE(k);
        const Real wanted = pairwise(-1.5, x, t, k);
        EXPECT_NEAR(found[k], wanted, kRounding * (1.0 + std::fabs(wanted)));
    }
}

TEST(TangentTest, Zeros)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset y = graph.add_variable(1);
    const Offset two = constant(graph, 2.0);
    const std::vector<Offset> roots{
        unary(graph, Operator::Sqrt, x),
        unary(graph, Operator::Log, x),
        power(graph, x, 0.5),
        power(graph, x, 1.0),
        power(graph, x, 0.0),
        binary(graph, Operator::Power, x, two),
        binary(graph, Operator::Power, x, y),
        sign_power(graph, x, 1.0),
        sign_power(graph, x, 0.5),
        unary(graph, Operator::Entropy, x),
        power(graph, x, -1.0),
        binary(graph, Operator::Power, x, constant(graph, -1.0)),
    };
    const std::vector<Values> still{{0.0},
                                    {0.0},
                                    {0.0},
                                    {0.0},
                                    {0.0},
                                    {0.0, 0.0},
                                    {0.0, 0.0},
                                    {0.0},
                                    {0.0},
                                    {0.0},
                                    {0.0},
                                    {0.0, 0.0}};
    const std::vector<Values> moving{
        {-kInfinity},
        {-kInfinity},
        {-kInfinity},
        {0.0},
        {0.0},
        {2.0, 0.0},
        {2.0, 0.0},
        {0.0},
        {-kInfinity},
        {kInfinity},
        {kInfinity},
        {kInfinity, kInfinity},
    };
    for (const Real zero : {0.0, -0.0})
    {
        const std::array point{zero, 2.0};
        for (std::size_t k = 0; k < roots.size(); ++k)
        {
            SCOPED_TRACE(testing::Message() << zero << ", " << k);
            for (const std::array direction : {std::array{0.0, 0.0}, std::array{0.0, 1.0}})
            {
                expect_values(linear_at(graph, roots[k], point, direction).tangents, still[k]);
            }
            expect_values(linear_at(graph, roots[k], point, std::array{1.0, 0.0}).tangents,
                          moving[k]);
        }
    }
}

TEST(TangentTest, Undefined)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset one = constant(graph, 1.0);
    const std::vector<Offset> roots{
        unary(graph, Operator::Abs, x),
        unary(graph, Operator::Negation, x),
        graph.add(Operator::Product, 1.0, std::array{x, one}),
        binary(graph, Operator::Division, one, x),
        unary(graph, Operator::Exp, x),
        unary(graph, Operator::Log, x),
    };
    const std::vector<Values> wanted{{kNan}, {kNan}, {kNan, kNan}, {kNan, kNan}, {kNan}, {kNan}};
    for (std::size_t k = 0; k < roots.size(); ++k)
    {
        SCOPED_TRACE(k);
        expect_values(linear_at(graph, roots[k], std::array{kNan}, std::array{0.0}).tangents,
                      wanted[k]);
    }
    expect_values(linear_at(graph, roots[5], std::array{-1.0}, std::array{1.0}).tangents, {kNan});
}

TEST(TangentTest, Lanes)
{
    Graph graph;
    const std::vector<Offset> roots = every_operator(graph);
    const std::array turned{-0.2, 0.7, 0.0, -0.9};
    Values direction(2 * kDirection.size());
    for (std::size_t j = 0; j < kDirection.size(); ++j)
    {
        direction[2 * j] = kDirection[j];
        direction[2 * j + 1] = turned[j];
    }
    for (std::size_t k = 0; k < roots.size(); ++k)
    {
        SCOPED_TRACE(k);
        const Values both = linear_at(graph, roots[k], kInterior, direction, 2).tangents;
        const Values first = linear_at(graph, roots[k], kInterior, kDirection).tangents;
        const Values second = linear_at(graph, roots[k], kInterior, turned).tangents;
        Values wanted(both.size());
        for (std::size_t p = 0; p < first.size(); ++p)
        {
            wanted[2 * p] = first[p];
            wanted[2 * p + 1] = second[p];
        }
        expect_values(both, wanted);
    }
}
