#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "expr/terms.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "model/nodes.hpp"
#include "model/postfix.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::Graph;
using biyos::Index;
using biyos::kNoRoot;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::Operator;
using biyos::QuadraticTerm;
using biyos::Variable;
using biyos::test::failure;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

struct Node
{
    int code;
    std::vector<Offset> children;
    std::vector<double> values;
};

Model with_variables(std::size_t n)
{
    Model model;
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    model.add_variables(std::vector<Variable>(n, free));
    return model;
}

Offset add(Model &model, const Node &node)
{
    return model.add_node(node.code, node.children, node.values);
}

std::vector<Offset> ids(std::span<const Offset> nodes)
{
    return {nodes.begin(), nodes.end()};
}

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

std::string refused(Model &model, const Node &node)
{
    return refusal([&] { add(model, node); });
}

Model with_functions(std::size_t n)
{
    Model model = with_variables(3);
    model.add_functions(std::vector<double>(n, 0.0), std::vector<Offset>(n + 1, 0), {});
    return model;
}

struct Sample
{
    Offset x;
    Offset y;
    Offset exponential;
    Offset top;
};

Sample sample(Model &model)
{
    Sample made{};
    made.x = model.add_variable_node(0);
    made.y = model.add_variable_node(1);
    made.exponential = add(model, {BIYOS_OP_EXP, {made.y}, {}});
    const Offset three = add(model, {BIYOS_OP_CONST, {}, {3.0}});
    const Offset pair = add(model, {BIYOS_OP_PROD, {made.x, made.y}, {1.0}});
    const std::vector<Offset> terms{three, made.x, pair, made.exponential};
    made.top = add(model, {BIYOS_OP_SUM, terms, {0.0, 1.0, 2.0, 1.0, 1.0}});
    return made;
}

void attach(Model &model, const std::vector<Index> &functions, const std::vector<Offset> &nodes)
{
    model.attach_expressions(functions, nodes);
}

std::string attach_refusal(Model &model,
                           const std::vector<Index> &functions,
                           std::vector<Offset> nodes)
{
    return refusal([&] { attach(model, functions, nodes); });
}

double constant_of(const Model &model, Index function)
{
    return model.constants(function, 1).front();
}

Offset root_of(const Model &model, Index function)
{
    return model.roots(function, 1).front();
}

void expect_linear(std::span<const LinearTerm> found, const std::vector<LinearTerm> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_EQ(found[k].variable, wanted[k].variable) << k;
        EXPECT_EQ(bits(found[k].coefficient), bits(wanted[k].coefficient)) << k;
    }
}

void expect_pairs(std::span<const QuadraticTerm> found, const std::vector<QuadraticTerm> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_EQ(found[k].row, wanted[k].row) << k;
        EXPECT_EQ(found[k].column, wanted[k].column) << k;
        EXPECT_EQ(bits(found[k].coefficient), bits(wanted[k].coefficient)) << k;
    }
}

void expect_same(const Model &found, const Model &wanted)
{
    EXPECT_EQ(biyos::difference(found, wanted), "");
    EXPECT_EQ(found.graph().mark(), wanted.graph().mark());
    const Index n = wanted.n_functions();
    EXPECT_EQ(ids(found.roots(0, n)), ids(wanted.roots(0, n)));
}

struct Program
{
    std::vector<Offset> starts;
    std::vector<int> codes;
    std::vector<Offset> arguments;
    std::vector<double> values;
};

biyos::Postfix postfix_of(const Program &program)
{
    return biyos::Postfix{
        .starts = program.starts,
        .codes = program.codes,
        .arguments = program.arguments,
        .values = program.values,
    };
}

std::vector<Offset> decoded(Model &model, const Program &program)
{
    return model.add_expressions(postfix_of(program));
}

std::string decode_refusal(Model &model, const Program &program)
{
    return refusal([&] { decoded(model, program); });
}

Program single(std::vector<int> codes, std::vector<Offset> arguments, std::vector<double> values)
{
    const auto n = static_cast<Offset>(codes.size());
    return Program{
        .starts = {0, n},
        .codes = std::move(codes),
        .arguments = std::move(arguments),
        .values = std::move(values),
    };
}

std::vector<Offset> children_of(const Model &model, Offset node)
{
    return ids(model.graph().children(node));
}

}

TEST(ExpressionTest, NodeByNode)
{
    Model model = with_variables(2);
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {}});
    const Offset y = model.add_variable_node(1);
    const Offset two = add(model, {BIYOS_OP_CONST, {}, {2.0}});
    const Offset shifted = add(model, {BIYOS_OP_SUM, {y, two}, {-0.5, 1.0, 3.0}});
    const Offset sine = add(model, {BIYOS_OP_SIN, {shifted}, {}});
    const Offset product = add(model, {BIYOS_OP_PROD, {exponential, sine}, {4.0}});
    const Offset power = add(model, {BIYOS_OP_CPOW, {product}, {1.5}});
    const Offset signed_power = add(model, {BIYOS_OP_SPOW, {power}, {0.5}});
    const Graph &graph = model.graph();
    EXPECT_EQ(graph.size(), 9);
    EXPECT_EQ(signed_power, 8);
    EXPECT_EQ(graph.op(x), Operator::Variable);
    EXPECT_EQ(graph.variable(y), 1);
    EXPECT_EQ(graph.op(exponential), Operator::Exp);
    EXPECT_EQ(ids(graph.children(exponential)), std::vector<Offset>{x});
    EXPECT_EQ(bits(graph.constant(two)), bits(2.0));
    EXPECT_EQ(graph.op(shifted), Operator::Sum);
    EXPECT_EQ(ids(graph.children(shifted)), (std::vector<Offset>{y, two}));
    EXPECT_EQ(bits(graph.constant(shifted)), bits(-0.5));
    EXPECT_EQ(bits(graph.coefficients(shifted)[0]), bits(1.0));
    EXPECT_EQ(bits(graph.coefficients(shifted)[1]), bits(3.0));
    EXPECT_EQ(graph.op(product), Operator::Product);
    EXPECT_EQ(ids(graph.children(product)), (std::vector<Offset>{exponential, sine}));
    EXPECT_EQ(bits(graph.constant(product)), bits(4.0));
    EXPECT_EQ(graph.op(power), Operator::ConstantPower);
    EXPECT_EQ(bits(graph.constant(power)), bits(1.5));
    EXPECT_EQ(graph.op(signed_power), Operator::SignPower);
    EXPECT_EQ(bits(graph.constant(signed_power)), bits(0.5));
}

TEST(ExpressionTest, SharedId)
{
    Model model = with_variables(1);
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {}});
    const Offset ratio = add(model, {BIYOS_OP_DIV, {exponential, exponential}, {}});
    const Offset most = add(model, {BIYOS_OP_MAX, {exponential, ratio, x}, {}});
    const Graph &graph = model.graph();
    EXPECT_EQ(graph.size(), 4);
    EXPECT_EQ(ids(graph.children(ratio)), (std::vector<Offset>{exponential, exponential}));
    EXPECT_EQ(ids(graph.children(most)), (std::vector<Offset>{exponential, ratio, x}));
}

TEST(ExpressionTest, VariableNode)
{
    Model model = with_variables(3);
    const Offset y = model.add_variable_node(1);
    EXPECT_EQ(model.add_variable_node(1), y);
    EXPECT_EQ(model.graph().size(), 1);
    EXPECT_EQ(refusal([&] { model.add_variable_node(3); }),
              "invalid argument: variable 3: unknown");
    EXPECT_EQ(refusal([&] { model.add_variable_node(-1); }),
              "invalid argument: variable -1: unknown");
    EXPECT_EQ(model.graph().size(), 1);
    EXPECT_EQ(model.add_variable_node(2), 1);
}

TEST(ExpressionTest, RefusedNode)
{
    Model model = with_variables(1);
    const Offset x = model.add_variable_node(0);
    const std::string bad = "invalid argument: expression: ";
    EXPECT_EQ(refused(model, {0, {x}, {}}), bad + "operator 0 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_ERF + 1, {x}, {}}), bad + "operator 30 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_KEEP, {x}, {}}), bad + "operator 32 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_REF, {x}, {}}), bad + "operator 33 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_VAR, {}, {}}), bad + "operator 2 takes a variable");
    EXPECT_EQ(refused(model, {BIYOS_OP_EXP, {1}, {}}), bad + "node 1 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_EXP, {-1}, {}}), bad + "node -1 is unknown");
    EXPECT_EQ(refused(model, {BIYOS_OP_EXP, {x, x}, {}}), bad + "operator 9 takes 1 child, not 2");
    EXPECT_EQ(refused(model, {BIYOS_OP_SUM, {}, {0.0}}),
              bad + "operator 3 takes 1 child or more, not 0");
    EXPECT_EQ(refused(model, {BIYOS_OP_CONST, {x}, {1.0}}),
              bad + "operator 1 takes 0 children, not 1");
    EXPECT_EQ(refused(model, {BIYOS_OP_CONST, {}, {kNan}}), bad + "constant is NaN");
    EXPECT_EQ(refused(model, {BIYOS_OP_SUM, {x}, {0.0, kInfinity}}),
              bad + "coefficient 0 is infinity");
    EXPECT_EQ(refused(model, {BIYOS_OP_SUM, {x}, {kNan, 1.0}}), bad + "constant is NaN");
    EXPECT_EQ(refused(model, {BIYOS_OP_SPOW, {x}, {-2.0}}),
              bad + "operator 28 takes a positive constant, not -2");
    EXPECT_EQ(refused(model, {BIYOS_OP_PROD, {x}, {}}),
              "invalid argument: argument 'val': holds 0 values, not 1");
    EXPECT_EQ(refused(model, {BIYOS_OP_SUM, {x, x}, {0.0, 1.0}}),
              "invalid argument: argument 'val': holds 2 values, not 3");
    EXPECT_EQ(model.graph().size(), 1);
}

TEST(ExpressionTest, ValueCount)
{
    Model model = with_variables(1);
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {7.0, 8.0}});
    const Offset sum = add(model, {BIYOS_OP_SUM, {x, exponential}, {1.0, 2.0, 3.0, 4.0}});
    const Graph &graph = model.graph();
    EXPECT_EQ(graph.op(exponential), Operator::Exp);
    EXPECT_EQ(graph.coefficients(sum).size(), 2U);
    EXPECT_EQ(bits(graph.coefficients(sum)[1]), bits(3.0));
}

TEST(ExpressionTest, ValueNeeds)
{
    constexpr Offset kMost = (std::numeric_limits<Offset>::max)();
    EXPECT_EQ(biyos::n_values(BIYOS_OP_CONST, 0), 1);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_SUM, 3), 4);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_SUM, kMost), kMost);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_SUM, -1), 0);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_PROD, 2), 1);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_CPOW, 1), 1);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_SPOW, 1), 1);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_EXP, 1), 0);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_MAX, 3), 0);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_VAR, 4), 0);
    EXPECT_EQ(biyos::n_values(0, 1), 0);
    EXPECT_EQ(biyos::n_values(BIYOS_OP_ERF + 1, 1), 0);
}

TEST(ExpressionTest, NoRoots)
{
    Model model = with_variables(1);
    model.add_functions(std::vector<double>(3, 0.0), std::vector<Offset>(4, 0), {});
    EXPECT_EQ(ids(model.roots(0, 3)), std::vector<Offset>(3, biyos::kNoRoot));
    EXPECT_TRUE(model.roots(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.roots(2, 2); }), "invalid argument: function 3: unknown");
}

TEST(ExpressionTest, Attached)
{
    Model model = with_functions(2);
    const Sample made = sample(model);
    attach(model, {1}, {made.top});
    EXPECT_EQ(bits(constant_of(model, 1)), bits(3.0));
    expect_linear(model.linear_terms(1, 1), {{0, 2.0}});
    expect_pairs(model.quadratic_terms(1, 1), {{0, 1, 1.0}});
    EXPECT_EQ(root_of(model, 1), made.exponential);
    EXPECT_EQ(root_of(model, 0), kNoRoot);
    EXPECT_TRUE(model.linear_terms(0, 1).empty());
    EXPECT_EQ(model.graph().size(), 6);
}

TEST(ExpressionTest, AttachTwice)
{
    Model model = with_functions(1);
    const Sample made = sample(model);
    attach(model, {0}, {made.top});
    attach(model, {0}, {made.top});
    EXPECT_EQ(bits(constant_of(model, 0)), bits(6.0));
    expect_linear(model.linear_terms(0, 1), {{0, 4.0}});
    expect_pairs(model.quadratic_terms(0, 1), {{0, 1, 2.0}});
    const Offset root = root_of(model, 0);
    EXPECT_EQ(root, 6);
    EXPECT_EQ(model.graph().op(root), Operator::Sum);
    EXPECT_EQ(children_of(model, root), (std::vector<Offset>{made.exponential, made.exponential}));
    EXPECT_EQ(bits(model.graph().constant(root)), bits(0.0));
    EXPECT_EQ(bits(model.graph().coefficients(root)[0]), bits(1.0));
    EXPECT_EQ(bits(model.graph().coefficients(root)[1]), bits(1.0));
}

TEST(ExpressionTest, HeldParts)
{
    Model model = with_variables(3);
    const std::vector<double> constants{1.0, 0.5};
    const std::vector<Offset> starts{0, 2, 3};
    model.add_functions(constants, starts, {{2, 1.0}, {1, -1.0}, {0, 7.0}});
    const std::vector<Index> held{0};
    const std::vector<Offset> one{0, 1};
    model.add_quadratic_terms(held, one, {{0, 0, 5.0}});
    const Sample made = sample(model);
    attach(model, {0}, {made.top});
    EXPECT_EQ(bits(constant_of(model, 0)), bits(4.0));
    expect_linear(model.linear_terms(0, 1), {{0, 2.0}, {1, -1.0}, {2, 1.0}});
    expect_pairs(model.quadratic_terms(0, 1), {{0, 0, 5.0}, {0, 1, 1.0}});
    expect_linear(model.linear_terms(1, 1), {{0, 7.0}});
    EXPECT_TRUE(model.quadratic_terms(1, 1).empty());
    EXPECT_EQ(bits(constant_of(model, 1)), bits(0.5));
}

TEST(ExpressionTest, Joined)
{
    Model model = with_functions(1);
    const Sample made = sample(model);
    const Offset sine = add(model, {BIYOS_OP_SIN, {made.x}, {}});
    attach(model, {0}, {made.exponential});
    attach(model, {0}, {made.x});
    EXPECT_EQ(root_of(model, 0), made.exponential);
    attach(model, {0}, {sine});
    const Offset root = root_of(model, 0);
    EXPECT_EQ(children_of(model, root), (std::vector<Offset>{made.exponential, sine}));
    expect_linear(model.linear_terms(0, 1), {{0, 1.0}});
}

TEST(ExpressionTest, SharedNode)
{
    Model model = with_functions(3);
    const Offset x = model.add_variable_node(0);
    const Offset z = model.add_variable_node(2);
    const Offset sine = add(model, {BIYOS_OP_SIN, {x}, {}});
    const Offset first = add(model, {BIYOS_OP_SUM, {sine, z}, {0.0, 1.0, 1.0}});
    const Offset second = add(model, {BIYOS_OP_SUM, {sine, x}, {1.0, 1.0, -2.0}});
    attach(model, {2, 0}, {first, second});
    EXPECT_EQ(root_of(model, 2), sine);
    EXPECT_EQ(root_of(model, 0), sine);
    expect_linear(model.linear_terms(2, 1), {{2, 1.0}});
    expect_linear(model.linear_terms(0, 1), {{0, -2.0}});
    EXPECT_EQ(bits(constant_of(model, 0)), bits(1.0));
    EXPECT_EQ(model.graph().size(), 5);
}

TEST(ExpressionTest, VariableRoot)
{
    Model model = with_functions(1);
    const Offset x = model.add_variable_node(2);
    attach(model, {0}, {x});
    expect_linear(model.linear_terms(0, 1), {{2, 1.0}});
    EXPECT_EQ(root_of(model, 0), kNoRoot);
}

TEST(ExpressionTest, Counted)
{
    Model model = with_functions(2);
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {}});
    const Offset sine = add(model, {BIYOS_OP_SIN, {x}, {}});
    const Offset sum = add(model, {BIYOS_OP_SUM, {exponential, sine, x}, {0.0, 2.0, 3.0, 1.0}});
    attach(model, {0}, {sum});
    const Offset kept = root_of(model, 0);
    EXPECT_EQ(children_of(model, kept), (std::vector<Offset>{exponential, sine}));
    const Offset outer = add(model, {BIYOS_OP_SUM, {kept, x}, {0.0, 1.0, 1.0}});
    const Offset cosine = add(model, {BIYOS_OP_COS, {x}, {}});
    attach(model, {0}, {cosine});
    attach(model, {1}, {outer});
    EXPECT_EQ(children_of(model, root_of(model, 1)), std::vector<Offset>{kept});
}

TEST(ExpressionTest, Remainder)
{
    Model model = with_functions(1);
    const Offset x = model.add_variable_node(0);
    const Offset sine = add(model, {BIYOS_OP_SIN, {x}, {}});
    const Offset cosine = add(model, {BIYOS_OP_COS, {x}, {}});
    const Offset sum = add(model, {BIYOS_OP_SUM, {sine, x, cosine}, {0.0, 2.0, 1.0, 3.0}});
    attach(model, {0}, {sum});
    const Offset root = root_of(model, 0);
    EXPECT_EQ(root, 4);
    EXPECT_EQ(children_of(model, root), (std::vector<Offset>{sine, cosine}));
    expect_linear(model.linear_terms(0, 1), {{0, 1.0}});
}

TEST(ExpressionTest, Cleared)
{
    Model model = with_functions(3);
    const Sample made = sample(model);
    attach(model, {0, 1, 2}, {made.top, made.top, made.exponential});
    const std::vector<Index> held{1};
    const std::vector<Offset> one{0, 1};
    model.add_quadratic_terms(held, one, {{2, 2, 1.0}});
    const std::vector<Index> cleared{1, 0, 1};
    model.clear_functions(cleared);
    for (Index function = 0; function < 2; ++function)
    {
        EXPECT_EQ(bits(constant_of(model, function)), bits(0.0));
        EXPECT_TRUE(model.linear_terms(function, 1).empty());
        EXPECT_TRUE(model.quadratic_terms(function, 1).empty());
        EXPECT_EQ(root_of(model, function), kNoRoot);
    }
    EXPECT_EQ(root_of(model, 2), made.exponential);
    EXPECT_EQ(model.graph().size(), 6);
    attach(model, {1}, {made.top});
    Model fresh = with_functions(3);
    const Sample again = sample(fresh);
    attach(fresh, {1, 2}, {again.top, again.exponential});
    EXPECT_EQ(biyos::difference(model, fresh), "");
    EXPECT_EQ(root_of(model, 1), root_of(fresh, 1));
}

TEST(ExpressionTest, ClearedTerms)
{
    Model model = with_variables(3);
    const std::vector<double> constants{1.0, 2.0, 3.0, 4.0};
    const std::vector<Offset> starts{0, 1, 3, 4, 6};
    const std::vector<LinearTerm> terms{{0, 1.0}, {0, 2.0}, {1, 3.0}, {2, 4.0}, {0, 5.0}, {1, 6.0}};
    model.add_functions(constants, starts, terms);
    const std::vector<Index> functions{3, 1, 0};
    const std::vector<Offset> pairs{0, 1, 2, 3};
    model.add_quadratic_terms(functions, pairs, {{0, 0, 1.0}, {1, 1, 2.0}, {2, 2, 3.0}});
    const std::vector<Index> cleared{1};
    model.clear_functions(cleared);
    expect_linear(model.linear_terms(0, 1), {{0, 1.0}});
    EXPECT_TRUE(model.linear_terms(1, 1).empty());
    expect_linear(model.linear_terms(2, 1), {{2, 4.0}});
    expect_linear(model.linear_terms(3, 1), {{0, 5.0}, {1, 6.0}});
    expect_pairs(model.quadratic_terms(0, 1), {{2, 2, 3.0}});
    EXPECT_TRUE(model.quadratic_terms(1, 1).empty());
    expect_pairs(model.quadratic_terms(3, 1), {{0, 0, 1.0}});
    EXPECT_EQ(bits(constant_of(model, 2)), bits(3.0));
    const std::vector<Index> first{3, 0, 0};
    model.clear_functions(first);
    EXPECT_EQ(model.linear_terms(0, 4).size(), 1U);
    expect_linear(model.linear_terms(2, 1), {{2, 4.0}});
    EXPECT_TRUE(model.quadratic_terms(0, 4).empty());
    model.clear_functions({});
    EXPECT_EQ(model.linear_terms(0, 4).size(), 1U);
    model.add_functions(std::vector<double>{0.0}, std::vector<Offset>{0, 1}, {{1, 9.0}});
    const std::vector<Index> added{4};
    model.add_quadratic_terms(added, std::vector<Offset>{0, 1}, {{0, 1, 2.0}});
    expect_linear(model.linear_terms(4, 1), {{1, 9.0}});
    expect_pairs(model.quadratic_terms(4, 1), {{0, 1, 2.0}});
    EXPECT_EQ(model.linear_terms(0, 5).size(), 2U);
}

TEST(ExpressionTest, ClearRefused)
{
    Model model = with_functions(2);
    const Sample made = sample(model);
    attach(model, {0}, {made.top});
    const Model before = model;
    const std::vector<Index> unknown{0, 2};
    EXPECT_EQ(refusal([&] { model.clear_functions(unknown); }),
              "invalid argument: function 2: unknown");
    const std::vector<Index> negative{-1};
    EXPECT_EQ(refusal([&] { model.clear_functions(negative); }),
              "invalid argument: function -1: unknown");
    expect_same(model, before);
}

TEST(ExpressionTest, Copied)
{
    Model model = with_functions(3);
    const Sample made = sample(model);
    attach(model, {2, 0}, {made.top, made.exponential});
    const Model copy = model;
    EXPECT_EQ(biyos::difference(copy, model), "");
    EXPECT_EQ(copy.graph().mark(), model.graph().mark());
    attach(model, {1}, {made.exponential});
    EXPECT_EQ(biyos::difference(copy, model), "function 1: root none against node 2");
    EXPECT_EQ(root_of(copy, 1), kNoRoot);
    Model changed = copy;
    add(changed, {BIYOS_OP_SIN, {made.x}, {}});
    EXPECT_EQ(changed.graph().size(), copy.graph().size() + 1);
    EXPECT_EQ(biyos::difference(changed, copy), "");
}

TEST(ExpressionTest, AttachRefused)
{
    constexpr double kLarge = 1.0e308;
    Model model = with_variables(2);
    const std::vector<double> constants{kLarge, 0.0};
    const std::vector<Offset> starts{0, 1, 1};
    model.add_functions(constants, starts, {{0, kLarge}});
    const std::vector<Index> held{1};
    const std::vector<Offset> one{0, 1};
    model.add_quadratic_terms(held, one, {{1, 0, kLarge}});
    const Offset x = model.add_variable_node(0);
    const Offset y = model.add_variable_node(1);
    const Offset large = add(model, {BIYOS_OP_CONST, {}, {kLarge}});
    const Offset linear = add(model, {BIYOS_OP_SUM, {x}, {0.0, kLarge}});
    const Offset pair = add(model, {BIYOS_OP_PROD, {x, y}, {kLarge}});
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {}});
    const Model before = model;
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(attach_refusal(model, {2}, {x}), bad + "function 2: unknown");
    EXPECT_EQ(attach_refusal(model, {1, 1}, {x, y}),
              bad + "argument 'idx': function 1 is repeated");
    EXPECT_EQ(attach_refusal(model, {1, 0}, {x, 6}), bad + "function 0: node 6 is unknown");
    EXPECT_EQ(attach_refusal(model, {0}, {-1}), bad + "function 0: node -1 is unknown");
    EXPECT_EQ(attach_refusal(model, {1, 0}, {exponential, large}),
              bad + "function 0: sum of constants is infinity");
    EXPECT_EQ(attach_refusal(model, {1, 0}, {exponential, linear}),
              bad + "function 0: sum of coefficients of variable 0 is infinity");
    EXPECT_EQ(attach_refusal(model, {0, 1}, {exponential, pair}),
              bad + "function 1: sum of coefficients of pair (0, 1) is infinity");
    expect_same(model, before);
    EXPECT_EQ(root_of(model, 0), kNoRoot);
}

TEST(ExpressionTest, RefusalOrder)
{
    constexpr double kLarge = 1.0e308;
    Model model = with_variables(2);
    const std::vector<double> constants{kLarge, 0.0};
    const std::vector<Offset> starts{0, 0, 1};
    model.add_functions(constants, starts, {{0, kLarge}});
    const Offset x = model.add_variable_node(0);
    const Offset both = add(model, {BIYOS_OP_SUM, {x}, {kLarge, kLarge}});
    const Offset sine = add(model, {BIYOS_OP_SIN, {x}, {}});
    const Model before = model;
    EXPECT_EQ(attach_refusal(model, {1, 0}, {both, both}),
              "invalid argument: function 0: sum of constants is infinity");
    EXPECT_EQ(attach_refusal(model, {0, 1}, {sine, both}),
              "invalid argument: function 1: sum of coefficients of variable 0 is infinity");
    expect_same(model, before);
}

TEST(ExpressionTest, SignedZero)
{
    Model model = with_variables(1);
    const std::vector<double> constants{-0.0, -0.0, 0.0};
    model.add_functions(constants, std::vector<Offset>(4, 0), {});
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {x}, {}});
    const Offset zero = add(model, {BIYOS_OP_CONST, {}, {-0.0}});
    const Offset sum = add(model, {BIYOS_OP_SUM, {exponential, zero}, {-0.0, 1.0, 1.0}});
    attach(model, {0, 1, 2}, {exponential, sum, sum});
    EXPECT_EQ(bits(constant_of(model, 0)), bits(-0.0));
    EXPECT_EQ(bits(constant_of(model, 1)), bits(-0.0));
    EXPECT_EQ(bits(constant_of(model, 2)), bits(0.0));
    EXPECT_EQ(root_of(model, 1), exponential);
}

TEST(ExpressionTest, Postfix)
{
    Model nodes = with_functions(1);
    const Offset x = nodes.add_variable_node(0);
    const Offset exponential = add(nodes, {BIYOS_OP_EXP, {x}, {}});
    const Offset y = nodes.add_variable_node(1);
    const Offset two = add(nodes, {BIYOS_OP_CONST, {}, {2.0}});
    const Offset shifted = add(nodes, {BIYOS_OP_SUM, {y, two}, {-0.5, 1.0, 3.0}});
    const Offset sine = add(nodes, {BIYOS_OP_SIN, {shifted}, {}});
    const Offset product = add(nodes, {BIYOS_OP_PROD, {exponential, sine}, {4.0}});
    const Offset power = add(nodes, {BIYOS_OP_CPOW, {product}, {1.5}});
    const Offset top = add(nodes, {BIYOS_OP_SPOW, {power}, {0.5}});
    Model postfix = with_functions(1);
    const Program program = single(
        {
            BIYOS_OP_VAR,
            BIYOS_OP_EXP,
            BIYOS_OP_VAR,
            BIYOS_OP_CONST,
            BIYOS_OP_SUM,
            BIYOS_OP_SIN,
            BIYOS_OP_PROD,
            BIYOS_OP_CPOW,
            BIYOS_OP_SPOW,
        },
        {0, 1, 1, 0, 2, 1, 2, 1, 1},
        {2.0, -0.5, 1.0, 3.0, 4.0, 1.5, 0.5});
    EXPECT_EQ(decoded(postfix, program), std::vector<Offset>{top});
    EXPECT_EQ(postfix.graph().mark(), nodes.graph().mark());
    attach(nodes, {0}, {top});
    attach(postfix, {0}, {top});
    EXPECT_EQ(biyos::difference(postfix, nodes), "");
    EXPECT_EQ(root_of(postfix, 0), top);
}

TEST(ExpressionTest, PostfixOrder)
{
    Model nodes = with_functions(1);
    const Offset x = nodes.add_variable_node(0);
    const Offset y = nodes.add_variable_node(1);
    const Offset sum = add(nodes, {BIYOS_OP_SUM, {x, y}, {0.0, 1.0, 1.0}});
    const Offset z = nodes.add_variable_node(2);
    const Offset product = add(nodes, {BIYOS_OP_PROD, {sum, z}, {1.0}});
    const Offset top = add(nodes, {BIYOS_OP_LOG, {product}, {}});
    Model postfix = with_functions(1);
    const Program program = single(
        {BIYOS_OP_VAR, BIYOS_OP_VAR, BIYOS_OP_SUM, BIYOS_OP_VAR, BIYOS_OP_PROD, BIYOS_OP_LOG},
        {0, 1, 2, 2, 2, 1},
        {0.0, 1.0, 1.0, 1.0});
    const std::vector<Offset> tops = decoded(postfix, program);
    EXPECT_EQ(tops, std::vector<Offset>{5});
    EXPECT_EQ(children_of(postfix, 4), (std::vector<Offset>{3, 2}));
    attach(nodes, {0}, {top});
    attach(postfix, {0}, tops);
    EXPECT_EQ(biyos::difference(postfix, nodes), "");
}

TEST(ExpressionTest, PostfixShared)
{
    Model model = with_functions(2);
    const Offset x = model.add_variable_node(0);
    const Offset sine = add(model, {BIYOS_OP_SIN, {x}, {}});
    const Program program{
        .starts = {0, 3, 5},
        .codes = {BIYOS_OP_NODE, BIYOS_OP_NODE, BIYOS_OP_DIV, BIYOS_OP_NODE, BIYOS_OP_EXP},
        .arguments = {sine, sine, 2, sine, 1},
        .values = {},
    };
    const std::vector<Offset> tops = decoded(model, program);
    EXPECT_EQ(tops, (std::vector<Offset>{2, 3}));
    EXPECT_EQ(children_of(model, 2), (std::vector<Offset>{sine, sine}));
    EXPECT_EQ(children_of(model, 3), std::vector<Offset>{sine});
    EXPECT_EQ(model.graph().size(), 4);
    attach(model, {0, 1}, tops);
    EXPECT_EQ(children_of(model, root_of(model, 0))[0], children_of(model, root_of(model, 1))[0]);
}

TEST(ExpressionTest, PostfixMerged)
{
    Model model = with_functions(1);
    const Offset exponential = add(model, {BIYOS_OP_EXP, {model.add_variable_node(0)}, {}});
    const Program program =
        single({BIYOS_OP_VAR, BIYOS_OP_VAR, BIYOS_OP_SUM, BIYOS_OP_NODE, BIYOS_OP_SUM},
               {1, 2, 2, exponential, 2},
               {1.0, 2.0, 3.0, 0.5, 4.0, 5.0});
    const std::vector<Offset> tops = decoded(model, program);
    EXPECT_EQ(tops, std::vector<Offset>{4});
    EXPECT_EQ(children_of(model, 4), (std::vector<Offset>{2, 3, exponential}));
    EXPECT_EQ(bits(model.graph().constant(4)), bits(4.5));
    EXPECT_EQ(bits(model.graph().coefficients(4)[0]), bits(8.0));
    EXPECT_EQ(bits(model.graph().coefficients(4)[1]), bits(12.0));
    EXPECT_EQ(bits(model.graph().coefficients(4)[2]), bits(5.0));
}

TEST(ExpressionTest, PostfixKept)
{
    Model model = with_functions(2);
    const Program program{
        .starts = {0, 6, 12},
        .codes =
            {
                BIYOS_OP_VAR,
                BIYOS_OP_VAR,
                BIYOS_OP_SUM,
                BIYOS_OP_KEEP,
                BIYOS_OP_VAR,
                BIYOS_OP_SUM,
                BIYOS_OP_VAR,
                BIYOS_OP_VAR,
                BIYOS_OP_PROD,
                BIYOS_OP_KEEP,
                BIYOS_OP_VAR,
                BIYOS_OP_PROD,
            },
        .arguments = {0, 1, 2, 1, 2, 2, 0, 1, 2, 1, 2, 2},
        .values = {0.5, 1.0, 2.0, 0.0, 3.0, 4.0, 2.0, 3.0},
    };
    const std::vector<Offset> tops = decoded(model, program);
    EXPECT_EQ(tops, (std::vector<Offset>{4, 6}));
    EXPECT_EQ(children_of(model, 4), (std::vector<Offset>{2, 3}));
    EXPECT_EQ(children_of(model, 2), (std::vector<Offset>{0, 1}));
    EXPECT_EQ(bits(model.graph().constant(2)), bits(0.5));
    EXPECT_EQ(bits(model.graph().coefficients(4)[1]), bits(4.0));
    EXPECT_EQ(children_of(model, 6), (std::vector<Offset>{5, 3}));
    EXPECT_EQ(bits(model.graph().constant(5)), bits(2.0));
    EXPECT_EQ(bits(model.graph().constant(6)), bits(3.0));
}

TEST(ExpressionTest, PostfixReference)
{
    Model model = with_functions(2);
    const Program program{
        .starts = {0, 5, 7, 11},
        .codes =
            {
                BIYOS_OP_VAR,
                BIYOS_OP_EXP,
                BIYOS_OP_KEEP,
                BIYOS_OP_REF,
                BIYOS_OP_DIV,
                BIYOS_OP_REF,
                BIYOS_OP_SIN,
                BIYOS_OP_VAR,
                BIYOS_OP_KEEP,
                BIYOS_OP_REF,
                BIYOS_OP_PROD,
            },
        .arguments = {0, 1, 1, 2, 2, 2, 1, 1, 1, 8, 2},
        .values = {1.0},
    };
    const std::vector<Offset> tops = decoded(model, program);
    EXPECT_EQ(tops, (std::vector<Offset>{2, 3, 5}));
    EXPECT_EQ(children_of(model, 2), (std::vector<Offset>{1, 1}));
    EXPECT_EQ(children_of(model, 3), std::vector<Offset>{1});
    EXPECT_EQ(children_of(model, 5), (std::vector<Offset>{4, 4}));
    EXPECT_EQ(model.graph().size(), 6);
}

TEST(ExpressionTest, KeepErrors)
{
    Model model = with_functions(1);
    model.add_variable_node(0);
    const Model before = model;
    const std::string entry = "invalid argument: expression 0: entry ";
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_KEEP}, {1}, {})),
              entry + "0: 1 operand taken, 0 given");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_KEEP}, {0, 2}, {})),
              entry + "1: operator 32 takes 1 child, not 2");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_KEEP}, {0, 0}, {})),
              entry + "1: operator 32 takes 1 child, not 0");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_REF}, {0, 0}, {})),
              entry + "1: entry 0 is no earlier keep");
    EXPECT_EQ(
        decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_KEEP, BIYOS_OP_REF}, {0, 1, 2}, {})),
        entry + "2: entry 2 is no earlier keep");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_REF}, {-1}, {})),
              entry + "0: entry -1 is no earlier keep");
    EXPECT_EQ(
        decode_refusal(
            model,
            single({BIYOS_OP_VAR, BIYOS_OP_KEEP, BIYOS_OP_REF, BIYOS_OP_DIV}, {0, 1, 0, 2}, {})),
        entry + "2: entry 0 is no earlier keep");
    const Program later{
        .starts = {0, 1, 3},
        .codes = {BIYOS_OP_REF, BIYOS_OP_VAR, BIYOS_OP_KEEP},
        .arguments = {2, 0, 1},
        .values = {},
    };
    EXPECT_EQ(decode_refusal(model, later), entry + "0: entry 2 is no earlier keep");
    expect_same(model, before);
}

TEST(ExpressionTest, PostfixValues)
{
    Model model = with_functions(1);
    const Program program{
        .starts = {0, 1, 3, 4},
        .codes = {BIYOS_OP_CONST, BIYOS_OP_VAR, BIYOS_OP_CPOW, BIYOS_OP_CONST},
        .arguments = {0, 0, 1, 0},
        .values = {1.25, 3.0, -2.5},
    };
    const std::vector<Offset> tops = decoded(model, program);
    ASSERT_EQ(tops.size(), 3U);
    EXPECT_EQ(bits(model.graph().constant(tops[0])), bits(1.25));
    EXPECT_EQ(bits(model.graph().constant(tops[1])), bits(3.0));
    EXPECT_EQ(bits(model.graph().constant(tops[2])), bits(-2.5));
    EXPECT_EQ(decoded(model, Program{.starts = {0}, .codes = {}, .arguments = {}, .values = {}}),
              std::vector<Offset>{});
}

TEST(ExpressionTest, PostfixErrors)
{
    Model model = with_functions(1);
    const Offset x = model.add_variable_node(0);
    const Model before = model;
    const std::string bad = "invalid argument: expression 0: ";
    const std::string entry = bad + "entry ";
    const double nan = kNan;
    const Offset far = Offset{1} << 40;
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, 0}, {1, 1}, {})),
              entry + "1: operator 0 is unknown");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, -5}, {1, 1}, {})),
              entry + "1: operator -5 is unknown");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_NODE}, {9}, {})),
              entry + "0: node 9 is unknown");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_NODE}, {-1}, {})),
              entry + "0: node -1 is unknown");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_DIV}, {1, 2}, {})),
              entry + "1: 2 operands taken, 1 given");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_EXP}, {1, 2}, {})),
              entry + "1: operator 9 takes 1 child, not 2");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_VAR}, {1, 2}, {})),
              bad + "2 operands left, not 1");
    EXPECT_EQ(decode_refusal(model, single({}, {}, {})), bad + "0 operands left, not 1");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR}, {3}, {})),
              entry + "0: variable 3 is unknown");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR}, {-1}, {})),
              entry + "0: variable -1 is unknown");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR}, {far}, {})),
              entry + "0: variable 1099511627776 is unknown");
    const Offset wraps = 1 - (Offset{1} << 32);
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR}, {wraps}, {})),
              entry + "0: variable -4294967295 is unknown");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_CONST}, {1}, {1.0})),
              entry + "0: operator 1 takes 0 children, not 1");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_SUM}, {0, 0}, {0.0})),
              entry + "1: operator 3 takes 1 child or more, not 0");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_SUM}, {0, far}, {})),
              entry + "1: 1099511627776 operands taken, 1 given");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_CONST}, {0}, {nan})),
              entry + "0: constant is NaN");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_SPOW}, {0, 1}, {-1.0})),
              entry + "1: operator 28 takes a positive constant, not -1");
    EXPECT_EQ(decode_refusal(model, single({BIYOS_OP_VAR, BIYOS_OP_SUM}, {0, 1}, {0.0, nan})),
              entry + "1: coefficient 0 is NaN");
    expect_same(model, before);
    EXPECT_EQ(model.graph().size(), x + 1);
}

TEST(ExpressionTest, PostfixRollback)
{
    Model model = with_functions(1);
    const Offset x = model.add_variable_node(0);
    const Model before = model;
    const Program program{
        .starts = {0, 3, 5},
        .codes = {BIYOS_OP_VAR, BIYOS_OP_VAR, BIYOS_OP_DIV, BIYOS_OP_NODE, BIYOS_OP_EXP},
        .arguments = {2, 0, 2, 2, 1},
        .values = {},
    };
    EXPECT_EQ(decode_refusal(model, program),
              "invalid argument: expression 1: entry 3: node 2 is unknown");
    expect_same(model, before);
    const Offset constant = add(model, {BIYOS_OP_CONST, {}, {1.0}});
    EXPECT_EQ(constant, x + 1);
    EXPECT_EQ(model.add_variable_node(2), x + 2);
    EXPECT_EQ(model.add_variable_node(0), x);
}

TEST(ExpressionTest, ValuesRun)
{
    Model model = with_functions(1);
    const Model before = model;
    const Program program = single({BIYOS_OP_VAR, BIYOS_OP_SUM}, {0, 1}, {0.0});
    EXPECT_EQ(failure([&] { decoded(model, program); }, biyos::ErrorKind::Internal),
              "internal error: postfix: values run out");
    expect_same(model, before);
}

TEST(ExpressionTest, PostfixShape)
{
    Model model = with_functions(1);
    const Model before = model;
    const std::vector<Offset> starts{0, 2};
    const std::vector<int> codes{BIYOS_OP_VAR, BIYOS_OP_EXP};
    const std::vector<Offset> one{0};
    const std::vector<Offset> two{0, 1};
    const biyos::Postfix shorter{.starts = starts, .codes = codes, .arguments = one, .values = {}};
    const biyos::Postfix empty{.starts = {}, .codes = {}, .arguments = {}, .values = {}};
    const biyos::Postfix longer{
        .starts = std::span(starts).first(1),
        .codes = codes,
        .arguments = two,
        .values = {},
    };
    const std::string differ = "internal error: model: postfix arrays differ in length";
    const auto internal = biyos::ErrorKind::Internal;
    EXPECT_EQ(failure([&] { model.add_expressions(shorter); }, internal), differ);
    EXPECT_EQ(failure([&] { model.add_expressions(empty); }, internal), differ);
    EXPECT_EQ(failure([&] { model.add_expressions(longer); }, internal),
              "internal error: model: starts and terms differ in number");
    expect_same(model, before);
}

TEST(ExpressionTest, PostfixCount)
{
    const std::vector<int> codes{
        BIYOS_OP_VAR,
        BIYOS_OP_CONST,
        BIYOS_OP_SUM,
        BIYOS_OP_NODE,
        BIYOS_OP_PROD,
        BIYOS_OP_EXP,
    };
    EXPECT_EQ(biyos::n_values(codes, std::vector<Offset>{0, 0, 2, 0, 2, 1}), 5);
    EXPECT_EQ(biyos::n_values(codes, std::vector<Offset>{0, 0, -3, 0, 2, 1}), 3);
    EXPECT_EQ(biyos::n_values(codes, std::vector<Offset>{0, 0, 3, 0, 2, 1}), 6);
    const Offset most = (std::numeric_limits<Offset>::max)();
    EXPECT_EQ(biyos::n_values(codes, std::vector<Offset>{0, 0, most, 0, 2, 1}), 9);
    const std::vector<int> sums(3, BIYOS_OP_SUM);
    EXPECT_EQ(biyos::n_values(sums, std::vector<Offset>{3, 3, 3}), 6);
    EXPECT_EQ(biyos::n_values(sums, std::vector<Offset>{1, 1, 1}), 6);
    EXPECT_EQ(biyos::n_values(sums, std::vector<Offset>{0, 1, 0}), 4);
    EXPECT_EQ(biyos::n_values({}, {}), 0);
}
