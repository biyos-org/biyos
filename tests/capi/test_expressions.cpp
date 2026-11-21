#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
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
    const std::vector<BIYOS_status> statuses(2, BIYOS_ERR_BADARG);
    EXPECT_EQ(attempt.statuses, statuses);
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.messages, std::vector<std::string>(2, locked));
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(variable_node(model, 0), 0);
}
