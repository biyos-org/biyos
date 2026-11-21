#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

constexpr BIYOS_offset kNoNode = -7;

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
};

void add_variables(const Model &model, int n)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), n, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
}

BIYOS_offset variable_node(const Model &model, BIYOS_index variable)
{
    BIYOS_offset node = kNoNode;
    EXPECT_EQ(BIYOS_addvarnode(model.get(), variable, &node), BIYOS_OK);
    return node;
}

BIYOS_status add_node(const Model &model,
                      int op,
                      const std::vector<BIYOS_offset> &argument,
                      const std::vector<double> &values,
                      BIYOS_offset *node)
{
    const auto n = static_cast<BIYOS_offset>(argument.size());
    const double *passed = values.empty() ? nullptr : values.data();
    return BIYOS_addnode(model.get(), op, n, argument.data(), passed, node);
}

BIYOS_offset node_of(const Model &model,
                     int op,
                     const std::vector<BIYOS_offset> &argument,
                     const std::vector<double> &values)
{
    BIYOS_offset node = kNoNode;
    EXPECT_EQ(add_node(model, op, argument, values, &node), BIYOS_OK);
    return node;
}

BIYOS_offset next_id(const Model &model)
{
    const std::vector<double> one{1.0};
    return node_of(model, BIYOS_OP_CONST, {}, one);
}

struct Program
{
    std::vector<std::int64_t> starts;
    std::vector<int> codes;
    std::vector<std::int64_t> arguments;
    std::vector<double> values;
};

BIYOS_status add_expressions(const Model &model,
                             const Program &program,
                             std::vector<BIYOS_offset> &nodes)
{
    const auto n = static_cast<int>(program.starts.size()) - 1;
    nodes.assign(static_cast<std::size_t>(n), kNoNode);
    const double *values = program.values.empty() ? nullptr : program.values.data();
    return BIYOS_addexpr(model.get(),
                         n,
                         program.starts.data(),
                         program.codes.data(),
                         program.arguments.data(),
                         values,
                         nodes.data());
}

std::vector<BIYOS_offset> expressions_of(const Model &model, const Program &program)
{
    std::vector<BIYOS_offset> nodes;
    EXPECT_EQ(add_expressions(model, program, nodes), BIYOS_OK);
    return nodes;
}

Program single(std::vector<int> code,
               std::vector<std::int64_t> argument,
               std::vector<double> values)
{
    const auto n = static_cast<std::int64_t>(code.size());
    return Program{.starts = {0, n},
                   .codes = std::move(code),
                   .arguments = std::move(argument),
                   .values = std::move(values)};
}

std::string refused(const Env &env, const Model &model, const Program &program)
{
    std::vector<BIYOS_offset> nodes;
    EXPECT_EQ(add_expressions(model, program, nodes), BIYOS_ERR_BADARG);
    EXPECT_EQ(nodes, std::vector<BIYOS_offset>(nodes.size(), kNoNode));
    return env.message();
}

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(biyos::test::last_message(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    const BIYOS_offset child = 0;
    BIYOS_offset node = kNoNode;
    note(attempt, BIYOS_addvarnode(attempt.model, 0, &node));
    note(attempt, BIYOS_addnode(attempt.model, BIYOS_OP_EXP, 1, &child, nullptr, &node));
    note(attempt, BIYOS_addexpr(attempt.model, 0, nullptr, nullptr, nullptr, nullptr, nullptr));
}

}

TEST(CApiTest, Nodes)
{
    const Env env;
    const Model model(env);
    add_variables(model, 2);
    const BIYOS_offset x = variable_node(model, 0);
    const BIYOS_offset y = variable_node(model, 1);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 1);
    EXPECT_EQ(variable_node(model, 0), x);
    const BIYOS_offset ratio = node_of(model, BIYOS_OP_DIV, {x, y}, {});
    const BIYOS_offset sum = node_of(model, BIYOS_OP_SUM, {ratio, ratio, x}, {1.0, 2.0, 3.0, 4.0});
    const BIYOS_offset product = node_of(model, BIYOS_OP_PROD, {sum, y}, {0.5});
    const BIYOS_offset power = node_of(model, BIYOS_OP_CPOW, {product}, {3.0});
    const BIYOS_offset sign = node_of(model, BIYOS_OP_SPOW, {power}, {0.5});
    EXPECT_EQ(sign, 6);
    EXPECT_EQ(BIYOS_addvarnode(model.get(), 1, nullptr), BIYOS_OK);
    EXPECT_EQ(add_node(model, BIYOS_OP_EXP, {sign}, {}, nullptr), BIYOS_OK);
    EXPECT_EQ(next_id(model), 8);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, RefusedNode)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    const BIYOS_offset x = variable_node(model, 0);
    BIYOS_offset node = kNoNode;
    const std::string bad = "invalid argument: ";
    const std::string expression = bad + "expression: ";
    EXPECT_EQ(BIYOS_addvarnode(model.get(), 1, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "variable 1: unknown");
    EXPECT_EQ(BIYOS_addvarnode(model.get(), -1, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "variable -1: unknown");
    EXPECT_EQ(add_node(model, BIYOS_OP_NODE, {x}, {}, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), expression + "operator 30 is unknown");
    EXPECT_EQ(add_node(model, BIYOS_OP_VAR, {}, {}, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), expression + "operator 2 takes a variable");
    EXPECT_EQ(add_node(model, BIYOS_OP_EXP, {x, x}, {}, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), expression + "operator 9 takes 1 child, not 2");
    EXPECT_EQ(add_node(model, BIYOS_OP_LOG, {1}, {}, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), expression + "node 1 is unknown");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(add_node(model, BIYOS_OP_PROD, {x}, {nan}, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), expression + "constant is NaN");
    EXPECT_EQ(add_node(model, BIYOS_OP_SPOW, {x}, {0.0}, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), expression + "operator 28 takes a positive constant, not 0");
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_EXP, -1, nullptr, nullptr, &node),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_EXP, 1, nullptr, nullptr, &node),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'arg': is null");
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_CPOW, 1, &x, nullptr, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'val': is null");
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_SUM, 1, &x, nullptr, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'val': is null");
    EXPECT_EQ(node, kNoNode);
    EXPECT_EQ(next_id(model), 1);
}

TEST(CApiTest, NullValues)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    const BIYOS_offset x = variable_node(model, 0);
    BIYOS_offset node = kNoNode;
    const double unread = 5.0;
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_EXP, 1, &x, nullptr, &node), BIYOS_OK);
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_SIN, 1, &x, &unread, &node), BIYOS_OK);
    EXPECT_EQ(node, 2);
    const double value = 2.0;
    EXPECT_EQ(BIYOS_addnode(model.get(), BIYOS_OP_CONST, 0, nullptr, &value, &node), BIYOS_OK);
    EXPECT_EQ(node, 3);
}

TEST(CApiTest, LockedExpressions)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<BIYOS_status> statuses(3, BIYOS_ERR_BADARG);
    EXPECT_EQ(attempt.statuses, statuses);
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.messages, std::vector<std::string>(3, locked));
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(variable_node(model, 0), 0);
}

TEST(CApiTest, Postfix)
{
    const Env env;
    const Model model(env);
    add_variables(model, 2);
    const Program program{
        .starts = {0, 4, 9},
        .codes =
            {
                BIYOS_OP_VAR,
                BIYOS_OP_EXP,
                BIYOS_OP_VAR,
                BIYOS_OP_DIV,
                BIYOS_OP_CONST,
                BIYOS_OP_VAR,
                BIYOS_OP_VAR,
                BIYOS_OP_PROD,
                BIYOS_OP_SUM,
            },
        .arguments = {0, 1, 1, 2, 0, 0, 1, 2, 2},
        .values = {2.0, 3.0, 0.0, 1.0, 1.0},
    };
    EXPECT_EQ(expressions_of(model, program), (std::vector<BIYOS_offset>{3, 6}));
    EXPECT_EQ(variable_node(model, 1), 2);
    EXPECT_EQ(next_id(model), 7);
    const std::int64_t none = 0;
    EXPECT_EQ(BIYOS_addexpr(model.get(), 0, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_addexpr(model.get(), 0, &none, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Program shared = single({BIYOS_OP_NODE, BIYOS_OP_NODE, BIYOS_OP_MAX}, {3, 6, 2}, {});
    EXPECT_EQ(BIYOS_addexpr(model.get(),
                            1,
                            shared.starts.data(),
                            shared.codes.data(),
                            shared.arguments.data(),
                            nullptr,
                            nullptr),
              BIYOS_OK);
    EXPECT_EQ(next_id(model), 9);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, PostfixErrors)
{
    const Env env;
    const Model model(env);
    add_variables(model, 2);
    const BIYOS_offset x = variable_node(model, 0);
    const std::string bad = "invalid argument: expression 0: ";
    const Program few{
        .starts = {0, 3, 5},
        .codes = {BIYOS_OP_VAR, BIYOS_OP_VAR, BIYOS_OP_DIV, BIYOS_OP_VAR, BIYOS_OP_DIV},
        .arguments = {0, 1, 2, 0, 2},
        .values = {},
    };
    EXPECT_EQ(refused(env, model, few),
              "invalid argument: expression 1: entry 4: 2 operands taken, 1 given");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, BIYOS_OP_VAR}, {0, 1}, {})),
              bad + "2 operands left, not 1");
    EXPECT_EQ(refused(env, model, single({}, {}, {})), bad + "0 operands left, not 1");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, 31}, {0, 1}, {})),
              bad + "entry 1: operator 31 is unknown");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR}, {2}, {})),
              bad + "entry 0: variable 2 is unknown");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_NODE}, {x + 1}, {})),
              bad + "entry 0: node 1 is unknown");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, BIYOS_OP_LOG}, {1, 0}, {})),
              bad + "entry 1: operator 10 takes 1 child, not 0");
    const double infinity = std::numeric_limits<double>::infinity();
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, BIYOS_OP_PROD}, {1, 1}, {infinity})),
              bad + "entry 1: constant is infinity");
    const Program made{
        .starts = {0, 1, 2},
        .codes = {BIYOS_OP_VAR, BIYOS_OP_NODE},
        .arguments = {1, 1},
        .values = {},
    };
    EXPECT_EQ(refused(env, model, made),
              "invalid argument: expression 1: entry 1: node 1 is unknown");
    EXPECT_EQ(variable_node(model, 1), 1);
    EXPECT_EQ(next_id(model), 2);
}

TEST(CApiTest, NullPostfix)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    BIYOS_model *held = model.get();
    const std::int64_t one[] = {0, 1};
    const std::int64_t two[] = {0, 2};
    const int variable = BIYOS_OP_VAR;
    const int variables[] = {BIYOS_OP_VAR, BIYOS_OP_CPOW};
    const std::int64_t argument[] = {0, 1};
    const double exponent = 2.0;
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(BIYOS_addexpr(held, 1, nullptr, &variable, argument, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': is null");
    EXPECT_EQ(BIYOS_addexpr(held, 1, one, nullptr, argument, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'code': is null");
    EXPECT_EQ(BIYOS_addexpr(held, 1, one, &variable, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'arg': is null");
    EXPECT_EQ(BIYOS_addexpr(held, 1, two, variables, argument, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'val': is null");
    EXPECT_EQ(BIYOS_addexpr(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'n': -1 is negative");
    const std::int64_t late[] = {1, 1};
    EXPECT_EQ(BIYOS_addexpr(held, 1, late, &variable, argument, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': begins at 1, not 0");
    const std::int64_t falling[] = {0, 1, 0};
    EXPECT_EQ(BIYOS_addexpr(held, 2, falling, &variable, argument, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'off': decreases from 1 to 0 at entry 2");
    BIYOS_offset node = kNoNode;
    EXPECT_EQ(BIYOS_addexpr(held, 1, two, variables, argument, &exponent, &node), BIYOS_OK);
    EXPECT_EQ(node, 1);
}
