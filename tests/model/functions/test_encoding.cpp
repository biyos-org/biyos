#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "model/expressions/postfix.hpp"
#include "model/match.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <vector>

namespace
{

using biyos::Encoding;
using biyos::Graph;
using biyos::kNoRoot;
using biyos::Model;
using biyos::Offset;
using biyos::Operator;

using Engine = std::mt19937;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr std::array<std::uint32_t, 8> kSeeds{1, 2, 3, 5, 8, 13, 21, 34};
constexpr std::array kWide{BIYOS_OP_SUM, BIYOS_OP_PROD, BIYOS_OP_MAX, BIYOS_OP_MIN};
constexpr std::array kBinary{BIYOS_OP_DIV, BIYOS_OP_POW};
constexpr std::array kScaled{BIYOS_OP_CONST, BIYOS_OP_PROD, BIYOS_OP_CPOW};
constexpr int kVariables = 4;
constexpr int kNodes = 80;
constexpr int kRoots = 6;

std::size_t at(Offset value)
{
    return static_cast<std::size_t>(value);
}

int below(Engine &engine, int n)
{
    return static_cast<int>(engine() % static_cast<std::uint32_t>(n));
}

template <std::size_t N> bool among(const std::array<int, N> &codes, int code)
{
    return std::ranges::count(codes, code) > 0;
}

std::vector<std::uint64_t> bits(const std::vector<double> &values)
{
    std::vector<std::uint64_t> found;
    for (const double value : values)
    {
        found.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return found;
}

Model with_variables(int n)
{
    Model model;
    const biyos::Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    model.add_variables(std::vector<biyos::Variable>(at(n), free));
    return model;
}

Offset add(Model &model,
           int code,
           const std::vector<Offset> &children,
           const std::vector<double> &values)
{
    return model.add_node(code, children, values);
}

Encoding encoded(const Model &model, const std::vector<Offset> &roots)
{
    return biyos::encode(model.graph(), roots);
}

std::vector<Offset> decoded(Model &model, const Encoding &encoding)
{
    return model.add_expressions(biyos::Postfix{
        .starts = encoding.starts,
        .codes = encoding.codes,
        .arguments = encoding.arguments,
        .values = encoding.values,
    });
}

void expect_same(const Encoding &found, const Encoding &wanted)
{
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.codes, wanted.codes);
    EXPECT_EQ(found.arguments, wanted.arguments);
    EXPECT_EQ(bits(found.values), bits(wanted.values));
}

double value(Engine &engine)
{
    if (below(engine, 10) == 0)
    {
        return -0.0;
    }
    return (below(engine, 17) - 8) / 4.0;
}

int arity(Engine &engine, int code)
{
    if (code == BIYOS_OP_CONST)
    {
        return 0;
    }
    if (among(kWide, code))
    {
        return 1 + below(engine, 3);
    }
    return among(kBinary, code) ? 2 : 1;
}

std::vector<double> values(Engine &engine, int code, int n)
{
    std::vector<double> drawn;
    if (code == BIYOS_OP_SUM)
    {
        for (int k = 0; k <= n; ++k)
        {
            drawn.push_back(value(engine));
        }
    }
    if (among(kScaled, code))
    {
        drawn.push_back(value(engine));
    }
    if (code == BIYOS_OP_SPOW)
    {
        drawn.push_back((1 + below(engine, 8)) / 4.0);
    }
    return drawn;
}

int random_code(Engine &engine)
{
    if (below(engine, 2) == 0)
    {
        return below(engine, 2) == 0 ? BIYOS_OP_SUM : BIYOS_OP_PROD;
    }
    return 1 + below(engine, BIYOS_OP_ERF);
}

Offset random_child(Engine &engine, const Model &model)
{
    const auto n = static_cast<int>(model.graph().size());
    if (below(engine, 2) == 0)
    {
        return n - 1 - below(engine, (std::min)(n, 3));
    }
    return below(engine, n);
}

void add_random(Model &model, Engine &engine)
{
    const int code = random_code(engine);
    if (code == BIYOS_OP_VAR)
    {
        model.add_variable_node(below(engine, kVariables));
        return;
    }
    const int n = arity(engine, code);
    std::vector<Offset> children;
    for (int k = 0; k < n; ++k)
    {
        children.push_back(random_child(engine, model));
    }
    add(model, code, children, values(engine, code, n));
}

Model random_graph(Engine &engine)
{
    Model model = with_variables(kVariables);
    for (int variable = 0; variable < kVariables; ++variable)
    {
        model.add_variable_node(variable);
    }
    while (model.graph().size() < kNodes)
    {
        add_random(model, engine);
    }
    return model;
}

std::vector<Offset> random_roots(Engine &engine)
{
    std::vector<Offset> roots;
    for (int k = 0; k < kRoots; ++k)
    {
        roots.push_back(below(engine, kRoots) == 0 ? kNoRoot
                                                   : kNodes - 1 - below(engine, kNodes / 2));
    }
    return roots;
}

Offset reached(const Graph &graph, const std::vector<Offset> &roots)
{
    std::vector<bool> seen(at(graph.size()), false);
    std::vector<Offset> pending;
    Offset n = 0;
    for (const Offset root : roots)
    {
        if (root != kNoRoot)
        {
            pending.push_back(root);
        }
    }
    while (!pending.empty())
    {
        const Offset node = pending.back();
        pending.pop_back();
        if (seen[at(node)])
        {
            continue;
        }
        seen[at(node)] = true;
        ++n;
        const std::span<const Offset> children = graph.children(node);
        pending.insert(pending.end(), children.begin(), children.end());
    }
    return n;
}

void expect_matched(const Model &model,
                    const Model &rebuilt,
                    const std::vector<Offset> &roots,
                    const std::vector<Offset> &tops)
{
    biyos::Matching matching(model.graph(), rebuilt.graph());
    Offset empty = 0;
    for (std::size_t k = 0; k < roots.size(); ++k)
    {
        if (roots[k] == kNoRoot)
        {
            EXPECT_EQ(rebuilt.graph().op(tops[k]), Operator::Constant) << k;
            ++empty;
            continue;
        }
        EXPECT_EQ(matching.difference(roots[k], tops[k]), "") << k;
    }
    EXPECT_EQ(rebuilt.graph().size(), reached(model.graph(), roots) + empty);
}

TEST(EncodingTest, Exact)
{
    Model model = with_variables(2);
    const Offset x = model.add_variable_node(0);
    const Offset exponential = add(model, BIYOS_OP_EXP, {x}, {});
    const Offset y = model.add_variable_node(1);
    const Offset two = add(model, BIYOS_OP_CONST, {}, {2.0});
    const Offset shifted = add(model, BIYOS_OP_SUM, {y, two}, {-0.5, 1.0, 3.0});
    const Offset sine = add(model, BIYOS_OP_SIN, {shifted}, {});
    const Offset product = add(model, BIYOS_OP_PROD, {exponential, sine}, {4.0});
    const Offset power = add(model, BIYOS_OP_CPOW, {product}, {1.5});
    const Offset top = add(model, BIYOS_OP_SPOW, {power}, {0.5});
    const Encoding wanted{
        .starts = {0, 9},
        .codes =
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
        .arguments = {0, 1, 1, 0, 2, 1, 2, 1, 1},
        .values = {2.0, -0.5, 1.0, 3.0, 4.0, 1.5, 0.5},
    };
    expect_same(encoded(model, {top}), wanted);
}

TEST(EncodingTest, Shared)
{
    Model model = with_variables(1);
    const Offset sine = add(model, BIYOS_OP_SIN, {model.add_variable_node(0)}, {});
    const Offset ratio = add(model, BIYOS_OP_DIV, {sine, sine}, {});
    const Offset exponential = add(model, BIYOS_OP_EXP, {sine}, {});
    const Encoding wanted{
        .starts = {0, 5, 7, 8},
        .codes =
            {
                BIYOS_OP_VAR,
                BIYOS_OP_SIN,
                BIYOS_OP_KEEP,
                BIYOS_OP_REF,
                BIYOS_OP_DIV,
                BIYOS_OP_REF,
                BIYOS_OP_EXP,
                BIYOS_OP_REF,
            },
        .arguments = {0, 1, 1, 2, 2, 2, 1, 2},
        .values = {},
    };
    expect_same(encoded(model, {ratio, exponential, sine}), wanted);
}

TEST(EncodingTest, Nested)
{
    Model model = with_variables(3);
    const Offset x = model.add_variable_node(0);
    const Offset y = model.add_variable_node(1);
    const Offset z = model.add_variable_node(2);
    const Offset inner = add(model, BIYOS_OP_SUM, {x, y}, {0.5, 1.0, 2.0});
    const Offset outer = add(model, BIYOS_OP_SUM, {inner, z}, {0.0, 3.0, 4.0});
    const Offset pair = add(model, BIYOS_OP_PROD, {x, y}, {2.0});
    const Offset triple = add(model, BIYOS_OP_PROD, {pair, z}, {3.0});
    const Offset larger = add(model, BIYOS_OP_MAX, {x, y}, {});
    const Offset largest = add(model, BIYOS_OP_MAX, {larger, z}, {});
    const Encoding wanted{
        .starts = {0, 6, 12, 17},
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
                BIYOS_OP_VAR,
                BIYOS_OP_VAR,
                BIYOS_OP_MAX,
                BIYOS_OP_VAR,
                BIYOS_OP_MAX,
            },
        .arguments = {0, 1, 2, 1, 2, 2, 0, 1, 2, 1, 2, 2, 0, 1, 2, 2, 2},
        .values = {0.5, 1.0, 2.0, 0.0, 3.0, 4.0, 2.0, 3.0},
    };
    expect_same(encoded(model, {outer, triple, largest}), wanted);
}

TEST(EncodingTest, NoRoot)
{
    Model model = with_variables(1);
    const Offset x = model.add_variable_node(0);
    const Encoding wanted{
        .starts = {0, 1, 2, 3},
        .codes = {BIYOS_OP_CONST, BIYOS_OP_VAR, BIYOS_OP_VAR},
        .arguments = {0, 0, 0},
        .values = {0.0},
    };
    expect_same(encoded(model, {kNoRoot, x, x}), wanted);
    const Encoding none{
        .starts = {0},
        .codes = {},
        .arguments = {},
        .values = {},
    };
    expect_same(encoded(model, {}), none);
}

TEST(EncodingTest, Random)
{
    for (const std::uint32_t seed : kSeeds)
    {
        SCOPED_TRACE(seed);
        Engine engine(seed);
        const Model model = random_graph(engine);
        const std::vector<Offset> roots = random_roots(engine);
        const Encoding encoding = encoded(model, roots);
        Model rebuilt = with_variables(kVariables);
        const std::vector<Offset> tops = decoded(rebuilt, encoding);
        expect_matched(model, rebuilt, roots, tops);
        expect_same(encoded(rebuilt, tops), encoding);
    }
}

}
