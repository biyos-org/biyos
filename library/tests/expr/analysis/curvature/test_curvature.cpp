#include "biyos/biyos.h"
#include "chords.hpp"
#include "core/numbers.hpp"
#include "draws.hpp"
#include "expr/analysis/curvature/curvature.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "refusal.hpp"
#include "sampling.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Curvature;
using biyos::Graph;
using biyos::Interval;
using biyos::Monotonicity;
using biyos::Offset;
using biyos::Operator;
using biyos::Real;
using biyos::Shapes;
using biyos::to_size;
using biyos::test::Box;
using biyos::test::box_of;
using biyos::test::draw_box;
using biyos::test::draw_graph;
using biyos::test::enclosures;
using biyos::test::Engine;
using biyos::test::expect_monotonicities;
using biyos::test::operator_graph;
using biyos::test::parameters_of;
using biyos::test::refusal;
using biyos::test::sampled_codes;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr std::uint64_t kSeed = 20261111;
constexpr int kBoxes = 200;
constexpr int kGraphs = 400;
constexpr std::size_t kSize = 6;
constexpr int kLevels = 64;
constexpr Interval kZero{0.0, 0.0};

constexpr Curvature kAffine = Curvature::Affine;
constexpr Curvature kConvex = Curvature::Convex;
constexpr Curvature kConcave = Curvature::Concave;
constexpr Curvature kNeither = Curvature::Unknown;

std::span<const Offset> one(const Offset &node)
{
    return {&node, 1};
}

Shapes classified(const Graph &graph, std::span<const Interval> box)
{
    const std::vector<Interval> nodes = enclosures(graph, box);
    Shapes shapes(graph);
    shapes.classify(graph, nodes);
    return shapes;
}

std::vector<Curvature> curvatures_of(const Graph &graph, const Shapes &shapes)
{
    std::vector<Curvature> curvatures(to_size(graph.size()));
    for (Offset node = 0; node < graph.size(); ++node)
    {
        curvatures[to_size(node)] = shapes.curvature(node);
    }
    return curvatures;
}

Shapes expect_chords(const Graph &graph,
                     std::span<const Interval> box,
                     Engine &engine,
                     int &reported)
{
    Shapes shapes = classified(graph, box);
    const std::vector<Curvature> curvatures = curvatures_of(graph, shapes);
    biyos::test::expect_chords(
        {.graph = graph, .box = box, .curvatures = curvatures, .reported = reported},
        engine);
    return shapes;
}

void expect_operator(const Graph &graph,
                     std::span<const Interval> box,
                     Engine &engine,
                     int &reported)
{
    const Shapes shapes = expect_chords(graph, box, engine, reported);
    const Offset root = graph.size() - 1;
    std::vector<Monotonicity> monotonicities(graph.children(root).size());
    for (std::size_t k = 0; k < monotonicities.size(); ++k)
    {
        monotonicities[k] = shapes.monotonicity(root, k);
    }
    expect_monotonicities(
        {.graph = graph, .box = box, .monotonicities = monotonicities, .reported = reported},
        engine);
}

Curvature root_of(const Graph &graph, std::span<const Interval> box)
{
    Engine engine(kSeed);
    int reported = 0;
    return expect_chords(graph, box, engine, reported).curvature(graph.size() - 1);
}

Curvature over(const Graph &graph, Interval x)
{
    const std::array box{x, kZero, kZero};
    return root_of(graph, box);
}

Offset unary(Graph &graph, Operator op, Offset child)
{
    return graph.add(op, one(child));
}

Offset square(Graph &graph, Offset child)
{
    return graph.add(Operator::ConstantPower, 2.0, one(child));
}

Offset affine(Graph &graph, Real constant, Offset child, Real weight)
{
    const std::array weights{weight};
    return graph.add_sum(constant, one(child), weights);
}

Graph composed(Operator op, Offset (*inner)(Graph &, Offset))
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    unary(graph, op, inner(graph, x));
    return graph;
}

Offset squared(Graph &graph, Offset x)
{
    return square(graph, x);
}

Offset cap(Graph &graph, Offset x)
{
    return affine(graph, 4.0, square(graph, x), -1.0);
}

Offset lifted(Graph &graph, Offset x)
{
    return affine(graph, 1.0, square(graph, x), 1.0);
}

Offset disc(Graph &graph, Offset x)
{
    return affine(graph, 1.0, square(graph, x), -1.0);
}

Offset ring(Graph &graph, Offset x)
{
    return affine(graph, -1.0, square(graph, x), 1.0);
}

Offset wave(Graph &graph, Offset x)
{
    return unary(graph, Operator::Sin, x);
}

}

TEST(CurvatureTest, Composed)
{
    EXPECT_EQ(over(composed(Operator::Exp, squared), {-1.0, 2.0}), kConvex);
    EXPECT_EQ(over(composed(Operator::Log, cap), {-1.0, 1.0}), kConcave);
    EXPECT_EQ(over(composed(Operator::Log, cap), {-3.0, 3.0}), kConcave);
    EXPECT_EQ(over(composed(Operator::Sqrt, lifted), {-1.0, 2.0}), kNeither);
    EXPECT_EQ(over(composed(Operator::Sqrt, disc), {-2.0, 2.0}), kConcave);
    EXPECT_EQ(over(composed(Operator::Sqrt, ring), {-2.0, 2.0}), kNeither);
    EXPECT_EQ(over(composed(Operator::Atanh, squared), {-1.0, 1.0}), kConvex);
    EXPECT_EQ(over(composed(Operator::Sqrt, wave), {0.0, 1.5}), kConcave);
    EXPECT_EQ(over(composed(Operator::Abs, wave), {0.0, 1.5}), kNeither);
    EXPECT_EQ(over(composed(Operator::Exp, wave), {0.0, 1.5}), kNeither);
    EXPECT_EQ(over(composed(Operator::Negation, wave), {0.0, 1.5}), kConvex);
}

TEST(CurvatureTest, Domains)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset quarter = affine(graph, -0.25, square(graph, x), 1.0);
    graph.add(Operator::ConstantPower, 1.5, one(quarter));
    EXPECT_EQ(over(graph, {-1.0, 1.0}), kNeither);
    EXPECT_EQ(over(graph, {0.6, 1.0}), kConvex);
    Graph ratio;
    const Offset y = ratio.add_variable(0);
    const std::array terms{ratio.add(Operator::Constant, 1.0, {}), unary(ratio, Operator::Sqrt, y)};
    ratio.add(Operator::Division, terms);
    EXPECT_EQ(over(ratio, {0.0, 4.0}), kConvex);
    Graph zero;
    const Offset z = zero.add_variable(0);
    const Offset nothing = zero.add(Operator::Constant, 0.0, {});
    const std::array parts{nothing, z};
    zero.add(Operator::Division, parts);
    EXPECT_EQ(over(zero, {0.0, 2.0}), kAffine);
    const std::array kinked{nothing, unary(zero, Operator::Abs, z)};
    zero.add(Operator::Division, kinked);
    EXPECT_EQ(over(zero, {-1.0, 2.0}), kNeither);
    const std::array peaked{nothing, unary(zero, Operator::Negation, kinked[1])};
    zero.add(Operator::Division, peaked);
    EXPECT_EQ(over(zero, {-1.0, 2.0}), kNeither);
    Graph weighted;
    const Offset w = weighted.add_variable(0);
    const std::array masked{w, unary(weighted, Operator::Sqrt, ring(weighted, w))};
    const std::array weights{1.0, 0.0};
    weighted.add_sum(0.0, masked, weights);
    EXPECT_EQ(over(weighted, {-2.0, 2.0}), kNeither);
    Graph rim;
    const Offset r = rim.add_variable(0);
    unary(rim, Operator::Acosh, unary(rim, Operator::Abs, r));
    EXPECT_EQ(over(rim, {-1.0, 1.0}), kNeither);
}

TEST(CurvatureTest, Coefficients)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const std::array terms{square(graph, x), unary(graph, Operator::Log, x)};
    const std::array convex{1.0, -1.0};
    const Offset first = graph.add_sum(0.0, terms, convex);
    const std::array concave{-1.0, 2.0};
    graph.add_sum(3.0, terms, concave);
    const std::array box{Interval(1.0, 2.0), kZero, kZero};
    const Shapes shapes = classified(graph, box);
    EXPECT_EQ(shapes.curvature(first), kConvex);
    EXPECT_EQ(shapes.curvature(graph.size() - 1), kConcave);
    EXPECT_EQ(shapes.monotonicity(first, 1), Monotonicity::Nonincreasing);
    const std::array mixed{1.0, 1.0};
    graph.add_sum(0.0, terms, mixed);
    EXPECT_EQ(root_of(graph, box), kNeither);
}

TEST(CurvatureTest, Factors)
{
    Graph graph;
    const std::array x{graph.add_variable(0), graph.add_variable(1)};
    const std::array factors{square(graph, x[0]), x[1]};
    graph.add(Operator::Product, 1.0, factors);
    const auto with = [](Interval y) { return Box{Interval(-1.0, 2.0), y, kZero}; };
    EXPECT_EQ(root_of(graph, with({2.0, 2.0})), kConvex);
    EXPECT_EQ(root_of(graph, with({-2.0, -2.0})), kConcave);
    EXPECT_EQ(root_of(graph, with({0.0, 0.0})), kAffine);
    EXPECT_EQ(root_of(graph, with({1.0, 2.0})), kNeither);
    Graph scaled;
    const Offset z = scaled.add_variable(0);
    scaled.add(Operator::Product, -3.0, one(square(scaled, z)));
    EXPECT_EQ(over(scaled, {-1.0, 2.0}), kConcave);
    Graph pair;
    const std::array y{pair.add_variable(0), pair.add_variable(1)};
    pair.add(Operator::Product, 1.0, y);
    EXPECT_EQ(root_of(pair, Box{Interval(1.0, 2.0), Interval(1.0, 2.0), kZero}), kNeither);
    EXPECT_EQ(root_of(pair, Box{Interval(1.0, 2.0), Interval(3.0, 3.0), kZero}), kAffine);
}

TEST(CurvatureTest, Shared)
{
    Graph graph;
    Offset node = graph.add_variable(0);
    const std::array weights{1.0, 1.0};
    for (int level = 0; level < kLevels; ++level)
    {
        const std::array terms{node, node};
        node = graph.add_sum(0.0, terms, weights);
    }
    const std::array box{Interval(1.0, 2.0)};
    const Shapes shapes = classified(graph, box);
    for (Offset level = 0; level <= kLevels; ++level)
    {
        EXPECT_EQ(shapes.curvature(level), kAffine) << level;
    }
    Graph twice;
    const Offset x = twice.add_variable(0);
    const Offset common = square(twice, x);
    const std::array terms{common, common};
    const std::array same{1.0, 1.0};
    const Offset doubled = twice.add_sum(0.0, terms, same);
    const std::array opposite{1.0, -1.0};
    const Offset cancelled = twice.add_sum(0.0, terms, opposite);
    const Shapes found = classified(twice, std::array{Interval(-1.0, 2.0)});
    EXPECT_EQ(found.curvature(common), kConvex);
    EXPECT_EQ(found.curvature(doubled), kConvex);
    EXPECT_EQ(found.curvature(cancelled), kNeither);
}

TEST(CurvatureTest, Unbounded)
{
    const Interval all = Interval::entire();
    EXPECT_EQ(over(composed(Operator::Exp, squared), all), kConvex);
    EXPECT_EQ(over(composed(Operator::Negation, squared), all), kConcave);
    EXPECT_EQ(over(composed(Operator::Log, cap), all), kConcave);
    EXPECT_EQ(over(composed(Operator::Sin, squared), all), kNeither);
    EXPECT_EQ(over(composed(Operator::Exp, lifted), {0.0, kInfinity}), kConvex);
    Graph graph;
    const Offset x = graph.add_variable(0);
    unary(graph, Operator::Sqrt, x);
    EXPECT_EQ(over(graph, all), kConcave);
}

TEST(CurvatureTest, Empty)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    const Offset logarithm = unary(graph, Operator::Log, x);
    const Offset grown = unary(graph, Operator::Exp, logarithm);
    const std::array terms{grown, x, graph.add(Operator::Constant, 2.0, {})};
    const std::array weights{1.0, 1.0, 1.0};
    const Offset sum = graph.add_sum(0.0, terms, weights);
    const Shapes negative = classified(graph, std::array{Interval(-2.0, -1.0)});
    EXPECT_EQ(negative.curvature(x), kAffine);
    EXPECT_EQ(negative.curvature(logarithm), kNeither);
    EXPECT_EQ(negative.curvature(grown), kNeither);
    EXPECT_EQ(negative.curvature(sum), kNeither);
    EXPECT_EQ(negative.monotonicity(sum, 1), Monotonicity::Unknown);
    const Shapes none = classified(graph, std::array{Interval::empty()});
    for (Offset node = 0; node < graph.size(); ++node)
    {
        const bool constant = graph.op(node) == Operator::Constant;
        EXPECT_EQ(none.curvature(node), constant ? kAffine : kNeither) << node;
    }
    Graph ratio;
    const std::array parts{ratio.add_variable(0), ratio.add(Operator::Constant, 0.0, {})};
    const Offset quotient = ratio.add(Operator::Division, parts);
    EXPECT_EQ(classified(ratio, std::array{Interval(1.0, 2.0)}).curvature(quotient), kNeither);
}

TEST(CurvatureTest, EmptyChild)
{
    Graph graph;
    const std::array x{graph.add_variable(0), graph.add_variable(1)};
    const std::array weights{1.0, 2.0};
    graph.add_sum(0.0, x, weights);
    graph.add(Operator::Product, 1.0, x);
    graph.add(Operator::Max, x);
    graph.add(Operator::Division, x);
    graph.add(Operator::Power, x);
    unary(graph, Operator::Exp, x[1]);
    std::vector<Interval> nodes(to_size(graph.size()), Interval(0.0, 1.0));
    nodes[to_size(x[1])] = Interval::empty();
    Shapes shapes(graph);
    shapes.classify(graph, nodes);
    for (Offset node = 2; node < graph.size(); ++node)
    {
        EXPECT_EQ(shapes.curvature(node), kNeither) << node;
        for (std::size_t k = 0; k < graph.children(node).size(); ++k)
        {
            EXPECT_EQ(shapes.monotonicity(node, k), Monotonicity::Unknown) << node;
        }
    }
}

TEST(CurvatureTest, Reclassified)
{
    Graph graph;
    const std::array x{graph.add_variable(0), graph.add_variable(1)};
    const Offset angle = graph.add(Operator::Atan2, x);
    Shapes shapes(graph);
    shapes.classify(graph, enclosures(graph, Box{Interval(1.0, 1.0), Interval(2.0, 2.0), kZero}));
    shapes.classify(graph, enclosures(graph, Box{Interval(-1.0, 1.0), Interval(2.0, 3.0), kZero}));
    for (std::size_t k = 0; k < x.size(); ++k)
    {
        EXPECT_EQ(shapes.monotonicity(angle, k), Monotonicity::Unknown) << k;
    }
}

TEST(CurvatureTest, Comparisons)
{
    for (int code = BIYOS_OP_LE; code <= BIYOS_OP_NE; ++code)
    {
        Graph graph;
        const std::array x{graph.add_variable(0), graph.add_variable(1)};
        const Offset node = graph.add(static_cast<Operator>(code), x);
        const Shapes shapes = classified(graph, Box{Interval(1.0, 2.0), Interval(3.0, 4.0), kZero});
        EXPECT_EQ(shapes.curvature(node), kNeither) << code;
        for (std::size_t k = 0; k < x.size(); ++k)
        {
            EXPECT_EQ(shapes.monotonicity(node, k), Monotonicity::Unknown) << code;
        }
    }
}

TEST(CurvatureTest, Connectives)
{
    for (int code = BIYOS_OP_AND; code <= BIYOS_OP_NOT; ++code)
    {
        Graph graph;
        const std::array x{graph.add_variable(0), graph.add_variable(1)};
        const Offset condition = graph.add(Operator::Less, x);
        const Offset node = graph.add(static_cast<Operator>(code), std::span(&condition, 1));
        const Shapes shapes = classified(graph, Box{Interval(1.0, 2.0), Interval(3.0, 4.0), kZero});
        EXPECT_EQ(shapes.curvature(node), kNeither) << code;
        EXPECT_EQ(shapes.monotonicity(node, 0), Monotonicity::Unknown) << code;
    }
}

TEST(CurvatureTest, Branches)
{
    Graph graph;
    const std::array x{graph.add_variable(0), graph.add_variable(1)};
    const Offset condition = graph.add(Operator::Less, x);
    const Offset node = graph.add(Operator::If, std::array{condition, x[0], x[1]});
    const Shapes shapes = classified(graph, Box{Interval(1.0, 2.0), Interval(3.0, 4.0), kZero});
    EXPECT_EQ(shapes.curvature(node), kNeither);
    for (std::size_t k = 0; k < 3; ++k)
    {
        EXPECT_EQ(shapes.monotonicity(node, k), Monotonicity::Unknown) << k;
    }
}

TEST(CurvatureTest, Points)
{
    Graph graph;
    const std::array x{graph.add_variable(0), graph.add_variable(1)};
    const std::array weights{1.0, 2.0};
    const Offset sum = graph.add_sum(0.0, x, weights);
    const Offset sine = unary(graph, Operator::Sin, x[1]);
    const Box box{Interval(-1.0, 1.0), Interval(3.0, 3.0), kZero};
    const Shapes shapes = classified(graph, box);
    EXPECT_EQ(shapes.monotonicity(sum, 0), Monotonicity::Nondecreasing);
    EXPECT_EQ(shapes.monotonicity(sum, 1), Monotonicity::Constant);
    EXPECT_EQ(shapes.curvature(sine), kAffine);
    EXPECT_EQ(shapes.monotonicity(sine, 0), Monotonicity::Constant);
}

TEST(CurvatureTest, Sizes)
{
    Graph graph;
    const Offset x = graph.add_variable(0);
    unary(graph, Operator::Exp, x);
    Shapes shapes(graph);
    const std::vector<Interval> small(1, Interval(0.0, 1.0));
    EXPECT_EQ(refusal([&] { shapes.classify(graph, small); }),
              "invalid argument: nodes: size 1, not the node count 2");
    Graph grown = graph;
    unary(grown, Operator::Log, x);
    const std::vector<Interval> three(3, Interval(0.0, 1.0));
    EXPECT_EQ(refusal([&] { shapes.classify(grown, three); }),
              "invalid argument: graph: size 3, not 2");
    Graph other;
    const std::array pair{other.add_variable(0), other.add_variable(1)};
    other.add(Operator::Max, pair);
    Graph single;
    single.add_variable(0);
    single.add_variable(1);
    single.add(Operator::Exp, one(pair[0]));
    Shapes small_shapes(single);
    EXPECT_EQ(refusal([&] { small_shapes.classify(other, three); }),
              "invalid argument: graph: node 2 has 2 children, not 1");
}

TEST(CurvatureTest, Sampled)
{
    for (const int number : sampled_codes())
    {
        const auto op = static_cast<Operator>(number);
        SCOPED_TRACE("operator " + std::to_string(number));
        Engine engine(kSeed + static_cast<std::uint64_t>(number));
        int reported = 0;
        for (int round = 0; round < kBoxes; ++round)
        {
            const Graph graph = operator_graph(op, parameters_of(op, engine));
            const bool tame = round % 2 == 0;
            const Box box = tame ? draw_box(engine) : box_of(engine);
            expect_operator(graph, box, engine, reported);
        }
    }
}

TEST(CurvatureTest, Random)
{
    Engine engine(kSeed);
    int reported = 0;
    for (int round = 0; round < kGraphs; ++round)
    {
        const Graph graph = draw_graph(engine, kSize);
        const Box box = draw_box(engine);
        SCOPED_TRACE("graph " + std::to_string(round));
        expect_chords(graph, box, engine, reported);
    }
}
