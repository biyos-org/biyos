#include "biyos/biyos.h"
#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"
#include "model/compare/compare.hpp"
#include "model/expressions/postfix.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::difference;
using biyos::Encoding;
using biyos::Model;
using biyos::Offset;
using biyos::Operator;
using biyos::Postfix;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr std::array kKinks{-1.0, 0.5, 2.0};
constexpr std::array kHeights{1.0, 0.0, 3.0};

Model with_variables(std::size_t n)
{
    Model model;
    const biyos::Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    model.add_variables(std::vector<biyos::Variable>(n, free));
    model.add_functions(std::vector<double>(n, 0.0), std::vector<Offset>(n + 1, 0), {});
    return model;
}

Offset add_kinks(Model &model)
{
    return model.add_piecewise_linear(kKinks, kHeights);
}

Model with_piecewise_linears()
{
    Model model = with_variables(2);
    add_kinks(model);
    const std::array heights{1.0, 0.0, -3.0};
    model.add_piecewise_linear(kKinks, heights);
    return model;
}

std::vector<std::uint64_t> bits(std::span<const double> values)
{
    std::vector<std::uint64_t> found;
    for (const double value : values)
    {
        found.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return found;
}

std::vector<Offset> decoded(Model &model, const Encoding &encoding)
{
    return model.add_expressions(Postfix{
        .starts = encoding.starts,
        .codes = encoding.codes,
        .arguments = encoding.arguments,
        .values = encoding.values,
    });
}

std::string decode_refusal(Model &model, const Encoding &encoding)
{
    return refusal([&] { decoded(model, encoding); });
}

Encoding piecewise_of(Offset pwl)
{
    return Encoding{
        .starts = {0, 3},
        .codes = {BIYOS_OP_VAR, BIYOS_OP_PWL, BIYOS_OP_EXP},
        .arguments = {1, pwl, 1},
        .values = {},
    };
}

Model attached(Offset pwl, biyos::Index variable)
{
    Model model = with_piecewise_linears();
    const Offset x = model.add_variable_node(variable);
    const Offset node = model.add_piecewise(pwl, x);
    const std::array<biyos::Index, 1> functions{0};
    model.attach_expressions(functions, std::span(&node, 1));
    return model;
}

}

TEST(PiecewiseTest, Added)
{
    Model model = with_variables(1);
    EXPECT_EQ(add_kinks(model), 0);
    EXPECT_EQ(add_kinks(model), 1);
    const Offset x = model.add_variable_node(0);
    const Offset node = model.add_piecewise(1, x);
    EXPECT_EQ(model.graph().op(node), Operator::PiecewiseLinear);
    EXPECT_EQ(model.graph().piecewise_linear(node), 1);
    EXPECT_EQ(std::vector<Offset>(model.graph().children(node).begin(),
                                  model.graph().children(node).end()),
              std::vector<Offset>{x});
    const biyos::PiecewiseLinear pwl = model.graph().points().piecewise_linear(1);
    EXPECT_EQ(bits(pwl.x), bits(kKinks));
    EXPECT_EQ(bits(pwl.y), bits(kHeights));
}

TEST(PiecewiseTest, Batch)
{
    Model model = with_piecewise_linears();
    const std::array<Offset, 3> starts{0, 3, 5};
    const std::array x{-1.0, 0.5, 2.0, -0.0, 4.0};
    const std::array y{1.0, 0.0, 3.0, -0.0, 7.0};
    EXPECT_EQ(model.add_piecewise_linears(starts, x, y), 2);
    EXPECT_EQ(model.graph().points().size(), 4);
    EXPECT_EQ(bits(model.graph().points().piecewise_linear(2).x), bits(kKinks));
    EXPECT_EQ(bits(model.graph().points().piecewise_linear(3).x), bits(std::span(x).last(2)));
    EXPECT_EQ(bits(model.graph().points().piecewise_linear(3).y), bits(std::span(y).last(2)));
    const std::array<Offset, 1> none{0};
    EXPECT_EQ(model.add_piecewise_linears(none, {}, {}), 4);
    EXPECT_EQ(model.graph().points().size(), 4);
}

TEST(PiecewiseTest, BatchRefused)
{
    Model model = with_variables(1);
    const Model before = model;
    const std::array<Offset, 4> starts{0, 2, 4, 6};
    const std::array x{0.0, 1.0, 0.0, 1.0, 1.0, 1.0};
    const std::array y{0.0, 1.0, 0.0, 1.0, 0.0, 1.0};
    EXPECT_EQ(refusal([&] { model.add_piecewise_linears(starts, x, y); }),
              "invalid argument: pwl 2: x 1 is not above x 0");
    EXPECT_EQ(model.graph().points().size(), 0);
    EXPECT_EQ(model.graph().mark(), before.graph().mark());
    EXPECT_EQ(difference(model, before), "");
}

TEST(PiecewiseTest, Refused)
{
    Model model = with_piecewise_linears();
    const Offset x = model.add_variable_node(0);
    const Model before = model;
    const std::array falling{0.0, 0.0};
    const std::array pair{1.0, 2.0};
    EXPECT_EQ(refusal([&] { model.add_piecewise_linear(falling, pair); }),
              "invalid argument: expression: x 1 is not above x 0");
    EXPECT_EQ(refusal([&] { model.add_piecewise(2, x); }),
              "invalid argument: expression: pwl 2 is unknown");
    EXPECT_EQ(refusal([&] { model.add_piecewise(-1, x); }),
              "invalid argument: expression: pwl -1 is unknown");
    EXPECT_EQ(refusal([&] { model.add_piecewise(0, x + 1); }),
              "invalid argument: expression: node 1 is unknown");
    const std::array child{x};
    EXPECT_EQ(refusal([&] { model.add_node(BIYOS_OP_PWL, child, {}); }),
              "invalid argument: expression: operator 31 takes a pwl");
    EXPECT_EQ(difference(model, before), "");
    EXPECT_EQ(model.graph().mark(), before.graph().mark());
}

TEST(PiecewiseTest, Postfix)
{
    Model nodes = with_piecewise_linears();
    const Offset y = nodes.add_variable_node(1);
    const Offset node = nodes.add_piecewise(1, y);
    const std::array child{node};
    const Offset top = nodes.add_node(BIYOS_OP_EXP, child, {});
    Model postfix = with_piecewise_linears();
    EXPECT_EQ(decoded(postfix, piecewise_of(1)), std::vector<Offset>{top});
    EXPECT_EQ(postfix.graph().mark(), nodes.graph().mark());
    EXPECT_EQ(postfix.graph().piecewise_linear(node), 1);
    const Encoding written = biyos::encode(nodes.graph(), std::span(&top, 1));
    const Encoding wanted = piecewise_of(1);
    EXPECT_EQ(written.starts, wanted.starts);
    EXPECT_EQ(written.codes, wanted.codes);
    EXPECT_EQ(written.arguments, wanted.arguments);
    EXPECT_TRUE(written.values.empty());
}

TEST(PiecewiseTest, PostfixRefused)
{
    Model model = with_piecewise_linears();
    const Model before = model;
    EXPECT_EQ(decode_refusal(model, piecewise_of(2)),
              "invalid argument: expression 0: entry 1: pwl 2 is unknown");
    Encoding none = piecewise_of(0);
    none.codes = {BIYOS_OP_PWL};
    none.arguments = {0};
    none.starts = {0, 1};
    EXPECT_EQ(decode_refusal(model, none),
              "invalid argument: expression 0: entry 0: 1 operand taken, 0 given");
    EXPECT_EQ(model.graph().mark(), before.graph().mark());
}

TEST(PiecewiseTest, Compared)
{
    Model more = with_piecewise_linears();
    add_kinks(more);
    EXPECT_EQ(difference(with_piecewise_linears(), more), "pwl count 2 against 3");
    Model shorter = with_variables(2);
    add_kinks(shorter);
    shorter.add_piecewise_linear(std::span(kKinks).first(2), std::span(kHeights).first(2));
    EXPECT_EQ(difference(with_piecewise_linears(), shorter), "pwl 1: point count 3 against 2");
    Model moved = with_variables(2);
    add_kinks(moved);
    const std::array kinks{-1.0, 0.5, 2.5};
    moved.add_piecewise_linear(kinks, std::array{1.0, 0.0, -3.0});
    EXPECT_EQ(difference(with_piecewise_linears(), moved), "pwl 1: point 2: x 2 against 2.5");
    Model signed_zero = with_variables(2);
    add_kinks(signed_zero);
    signed_zero.add_piecewise_linear(kKinks, std::array{1.0, -0.0, -3.0});
    EXPECT_EQ(difference(with_piecewise_linears(), signed_zero), "pwl 1: point 1: y 0 against -0");
    EXPECT_EQ(difference(attached(0, 0), attached(1, 0)),
              "function 0: node 1 against 1: pwl 0 against 1");
    EXPECT_EQ(difference(attached(0, 0), attached(0, 1)),
              "function 0: node 0 against 0: variable 0 against 1");
    EXPECT_EQ(difference(attached(1, 1), attached(1, 1)), "");
}

TEST(PiecewiseTest, Copied)
{
    const Model model = attached(1, 0);
    Model copy = model;
    EXPECT_EQ(difference(copy, model), "");
    EXPECT_EQ(copy.graph().mark(), model.graph().mark());
    add_kinks(copy);
    EXPECT_EQ(model.graph().points().size(), 2);
    EXPECT_EQ(difference(copy, model), "pwl count 3 against 2");
}
