#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"

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

using biyos::test::Env;
using biyos::test::Model;

constexpr BIYOS_offset kNoNode = -7;

std::vector<std::uint64_t> bits_of(const std::vector<double> &values)
{
    std::vector<std::uint64_t> bits;
    bits.reserve(values.size());
    for (const double value : values)
    {
        bits.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return bits;
}

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
    std::vector<std::int64_t> counts;
    std::vector<int> codes;
    std::vector<std::int64_t> arguments;
    std::vector<double> values;
};

BIYOS_status add_expressions(const Model &model,
                             const Program &program,
                             std::vector<BIYOS_offset> &nodes)
{
    const auto n = static_cast<int>(program.counts.size());
    nodes.assign(static_cast<std::size_t>(n), kNoNode);
    const double *values = program.values.empty() ? nullptr : program.values.data();
    return BIYOS_addexpr(model.get(),
                         n,
                         program.counts.data(),
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
    return Program{.counts = {n},
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

BIYOS_status attach(const Model &model,
                    const std::vector<int> &functions,
                    const std::vector<BIYOS_offset> &nodes)
{
    const auto n = static_cast<int>(functions.size());
    return BIYOS_attachexprsp(model.get(), n, functions.data(), nodes.data());
}

struct Read
{
    std::vector<double> constants;
    std::vector<std::int64_t> counts;
    std::vector<int> variables;
    std::vector<double> coefficients;
};

Read read_functions(const Model &model, int n)
{
    BIYOS_offset nnz = -1;
    BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_getnrlintermrg(held, 0, n, &nnz), BIYOS_OK);
    const auto size = static_cast<std::size_t>(nnz);
    Read read{
        .constants = std::vector<double>(static_cast<std::size_t>(n)),
        .counts = std::vector<std::int64_t>(static_cast<std::size_t>(n)),
        .variables = std::vector<int>(size),
        .coefficients = std::vector<double>(size),
    };
    EXPECT_EQ(BIYOS_getcstrg(held, 0, n, read.constants.data()), BIYOS_OK);
    const BIYOS_status status = BIYOS_getrlintermrg(held,
                                                    0,
                                                    n,
                                                    nnz,
                                                    read.counts.data(),
                                                    read.variables.data(),
                                                    read.coefficients.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

struct Pairs
{
    std::vector<std::int64_t> counts;
    std::vector<int> firsts;
    std::vector<int> seconds;
    std::vector<double> coefficients;
};

Pairs read_pairs(const Model &model, int n)
{
    BIYOS_offset nnz = -1;
    BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_getnquadtermrg(held, 0, n, &nnz), BIYOS_OK);
    const auto size = static_cast<std::size_t>(nnz);
    Pairs read{
        .counts = std::vector<std::int64_t>(static_cast<std::size_t>(n)),
        .firsts = std::vector<int>(size),
        .seconds = std::vector<int>(size),
        .coefficients = std::vector<double>(size),
    };
    const BIYOS_status status = BIYOS_getquadtermrg(held,
                                                    0,
                                                    n,
                                                    nnz,
                                                    read.counts.data(),
                                                    read.firsts.data(),
                                                    read.seconds.data(),
                                                    read.coefficients.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void add_functions(const Model &model, int n)
{
    const std::vector<double> constants(static_cast<std::size_t>(n), 0.5);
    ASSERT_EQ(BIYOS_addfun(model.get(), n, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcstrg(model.get(), 0, n, constants.data()), BIYOS_OK);
}

std::vector<BIYOS_offset> roots_of(const Model &model, int n)
{
    const std::span<const biyos::Offset> roots = model.get()->model().roots(0, n);
    return {roots.begin(), roots.end()};
}

Program sized(BIYOS_offset nnz, BIYOS_offset nval, int n)
{
    return Program{
        .counts = std::vector<std::int64_t>(static_cast<std::size_t>(n), -1),
        .codes = std::vector<int>(static_cast<std::size_t>(nnz), -1),
        .arguments = std::vector<std::int64_t>(static_cast<std::size_t>(nnz), -1),
        .values = std::vector<double>(static_cast<std::size_t>(nval), -1.0),
    };
}

BIYOS_status get_range(const Model &model, BIYOS_index pos, BIYOS_index n, Program &read)
{
    return BIYOS_getexprrg(model.get(),
                           pos,
                           pos + n,
                           static_cast<BIYOS_offset>(read.codes.size()),
                           static_cast<BIYOS_offset>(read.values.size()),
                           read.counts.data(),
                           read.codes.data(),
                           read.arguments.data(),
                           read.values.data());
}

Program read_range(const Model &model, BIYOS_index pos, BIYOS_index n)
{
    BIYOS_offset nnz = -1;
    BIYOS_offset nval = -1;
    EXPECT_EQ(BIYOS_getnexprrg(model.get(), pos, pos + n, &nnz, &nval), BIYOS_OK);
    Program read = sized(nnz, nval, n);
    EXPECT_EQ(get_range(model, pos, n, read), BIYOS_OK);
    return read;
}

Program read_list(const Model &model, const std::vector<int> &functions)
{
    const auto n = static_cast<int>(functions.size());
    BIYOS_offset nnz = -1;
    BIYOS_offset nval = -1;
    EXPECT_EQ(BIYOS_getnexprsp(model.get(), n, functions.data(), &nnz, &nval), BIYOS_OK);
    Program read = sized(nnz, nval, n);
    const BIYOS_status status = BIYOS_getexprsp(model.get(),
                                                n,
                                                functions.data(),
                                                nnz,
                                                nval,
                                                read.counts.data(),
                                                read.codes.data(),
                                                read.arguments.data(),
                                                read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void add_ratio(const Model &model)
{
    add_variables(model, 2);
    add_functions(model, 3);
    const BIYOS_offset sine = node_of(model, BIYOS_OP_SIN, {variable_node(model, 0)}, {});
    const BIYOS_offset y = variable_node(model, 1);
    const BIYOS_offset ratio = node_of(model, BIYOS_OP_DIV, {sine, y}, {});
    ASSERT_EQ(attach(model, {0, 2}, {sine, ratio}), BIYOS_OK);
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
    note(attempt, BIYOS_attachexprsp(attempt.model, 0, nullptr, nullptr));
    note(attempt, BIYOS_attachexprrg(attempt.model, 0, 0, nullptr));
    note(attempt, BIYOS_clrfunsp(attempt.model, 0, nullptr));
    note(attempt, BIYOS_clrfunrg(attempt.model, 0, 0));
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
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<BIYOS_status> statuses(7, BIYOS_ERR_BADARG);
    EXPECT_EQ(attempt.statuses, statuses);
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(attempt.messages, std::vector<std::string>(7, locked));
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(variable_node(model, 0), 0);
}

TEST(CApiTest, Postfix)
{
    const Env env;
    const Model model(env);
    add_variables(model, 2);
    const Program program{
        .counts = {4, 5},
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
                            shared.counts.data(),
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
        .counts = {3, 2},
        .codes = {BIYOS_OP_VAR, BIYOS_OP_VAR, BIYOS_OP_DIV, BIYOS_OP_VAR, BIYOS_OP_DIV},
        .arguments = {0, 1, 2, 0, 2},
        .values = {},
    };
    EXPECT_EQ(refused(env, model, few),
              "invalid argument: expression 1: entry 4: 2 operands taken, 1 given");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, BIYOS_OP_VAR}, {0, 1}, {})),
              bad + "2 operands left, not 1");
    EXPECT_EQ(refused(env, model, single({}, {}, {})), bad + "0 operands left, not 1");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, 34}, {0, 1}, {})),
              bad + "entry 1: operator 34 is unknown");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, BIYOS_OP_PWL}, {0, 0}, {})),
              bad + "entry 1: pwl 0 is unknown");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR}, {2}, {})),
              bad + "entry 0: variable 2 is unknown");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_NODE}, {x + 1}, {})),
              bad + "entry 0: node 1 is unknown");
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, BIYOS_OP_LOG}, {1, 0}, {})),
              bad + "entry 1: operator 10 takes 1 child, not 0");
    const double infinity = BIYOS_INF;
    EXPECT_EQ(refused(env, model, single({BIYOS_OP_VAR, BIYOS_OP_PROD}, {1, 1}, {infinity})),
              bad + "entry 1: constant is infinity");
    const Program made{
        .counts = {1, 1},
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
    const std::int64_t one = 1;
    const std::int64_t two = 2;
    const int variable = BIYOS_OP_VAR;
    const int variables[] = {BIYOS_OP_VAR, BIYOS_OP_CPOW};
    const std::int64_t argument[] = {0, 1};
    const double exponent = 2.0;
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(BIYOS_addexpr(held, 1, nullptr, &variable, argument, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'cnt': is null");
    EXPECT_EQ(BIYOS_addexpr(held, 1, &one, nullptr, argument, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'code': is null");
    EXPECT_EQ(BIYOS_addexpr(held, 1, &one, &variable, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'arg': is null");
    EXPECT_EQ(BIYOS_addexpr(held, 1, &two, variables, argument, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'val': is null");
    EXPECT_EQ(BIYOS_addexpr(held, -1, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'n': -1 is negative");
    const std::int64_t negative[] = {1, 0, -1};
    EXPECT_EQ(BIYOS_addexpr(held, 3, negative, &variable, argument, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'cnt': -1 is negative at entry 2");
    const std::int64_t overflowing[] = {(std::numeric_limits<std::int64_t>::max)(), 1};
    EXPECT_EQ(BIYOS_addexpr(held, 2, overflowing, &variable, argument, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'cnt': sum overflows at entry 1");
    BIYOS_offset node = kNoNode;
    EXPECT_EQ(BIYOS_addexpr(held, 1, &two, variables, argument, &exponent, &node), BIYOS_OK);
    EXPECT_EQ(node, 1);
}

TEST(CApiTest, Attach)
{
    const Env env;
    const Model model(env);
    add_variables(model, 3);
    add_functions(model, 2);
    const int owners[] = {1, 1};
    const int variables[] = {2, 0};
    const double coefficients[] = {1.0, -1.0};
    ASSERT_EQ(BIYOS_addlinterm(model.get(), 2, owners, variables, coefficients), BIYOS_OK);
    const Program program = single(
        {
            BIYOS_OP_CONST,
            BIYOS_OP_VAR,
            BIYOS_OP_VAR,
            BIYOS_OP_VAR,
            BIYOS_OP_PROD,
            BIYOS_OP_VAR,
            BIYOS_OP_EXP,
            BIYOS_OP_SUM,
        },
        {0, 0, 0, 1, 2, 1, 1, 4},
        {3.0, 1.0, 0.0, 1.0, 2.0, 1.0, 1.0});
    const std::vector<BIYOS_offset> tops = expressions_of(model, program);
    ASSERT_EQ(attach(model, {1}, tops), BIYOS_OK);
    const Read functions = read_functions(model, 2);
    EXPECT_EQ(functions.constants, (std::vector<double>{0.5, 3.5}));
    EXPECT_EQ(functions.counts, (std::vector<std::int64_t>{0, 2}));
    EXPECT_EQ(functions.variables, (std::vector<int>{0, 2}));
    EXPECT_EQ(functions.coefficients, (std::vector<double>{1.0, 1.0}));
    const Pairs pairs = read_pairs(model, 2);
    EXPECT_EQ(pairs.counts, (std::vector<std::int64_t>{0, 1}));
    EXPECT_EQ(pairs.firsts, std::vector<int>{0});
    EXPECT_EQ(pairs.seconds, std::vector<int>{1});
    EXPECT_EQ(pairs.coefficients, std::vector<double>{1.0});
    EXPECT_EQ(tops, std::vector<BIYOS_offset>{5});
    EXPECT_EQ(roots_of(model, 2), (std::vector<BIYOS_offset>{biyos::kNoRoot, 3}));
    EXPECT_EQ(next_id(model), 6);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, SharedExpression)
{
    const Env env;
    const Model model(env);
    add_variables(model, 2);
    add_functions(model, 2);
    const BIYOS_offset x = variable_node(model, 0);
    const BIYOS_offset sine = node_of(model, BIYOS_OP_SIN, {x}, {});
    const Program program{
        .counts = {3, 2},
        .codes = {BIYOS_OP_NODE, BIYOS_OP_VAR, BIYOS_OP_SUM, BIYOS_OP_NODE, BIYOS_OP_SQRT},
        .arguments = {sine, 1, 2, sine, 1},
        .values = {0.0, 1.0, 1.0},
    };
    const std::vector<BIYOS_offset> tops = expressions_of(model, program);
    ASSERT_EQ(attach(model, {0, 1}, tops), BIYOS_OK);
    const std::vector<BIYOS_offset> roots = roots_of(model, 2);
    EXPECT_EQ(roots[0], sine);
    const biyos::Graph &graph = model.get()->model().graph();
    ASSERT_EQ(graph.children(roots[1]).size(), 1U);
    EXPECT_EQ(graph.children(roots[1]).front(), sine);
    EXPECT_EQ(next_id(model), 5);
    EXPECT_EQ(read_functions(model, 2).variables, std::vector<int>{1});
}

TEST(CApiTest, AttachTwice)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    add_functions(model, 1);
    const Program program = single({BIYOS_OP_VAR, BIYOS_OP_EXP, BIYOS_OP_VAR, BIYOS_OP_SUM},
                                   {0, 1, 0, 2},
                                   {1.0, 1.0, 2.0});
    const std::vector<BIYOS_offset> tops = expressions_of(model, program);
    ASSERT_EQ(attach(model, {0}, tops), BIYOS_OK);
    ASSERT_EQ(attach(model, {0}, tops), BIYOS_OK);
    const Read functions = read_functions(model, 1);
    EXPECT_EQ(functions.constants, std::vector<double>{2.5});
    EXPECT_EQ(functions.coefficients, std::vector<double>{4.0});
    const biyos::Graph &graph = model.get()->model().graph();
    const BIYOS_offset root = roots_of(model, 1).front();
    EXPECT_EQ(root, 3);
    EXPECT_EQ(graph.children(root)[0], 1);
    EXPECT_EQ(graph.children(root)[1], 1);
}

TEST(CApiTest, RefusedAttach)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    add_functions(model, 2);
    const BIYOS_offset x = variable_node(model, 0);
    const double large = 1.0e308;
    const BIYOS_offset big = node_of(model, BIYOS_OP_SUM, {x}, {large, 1.0});
    ASSERT_EQ(attach(model, {0}, {big}), BIYOS_OK);
    const Read before = read_functions(model, 2);
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(attach(model, {2}, {x}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 2: unknown");
    EXPECT_EQ(attach(model, {1, 1}, {x, x}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'idx': function 1 is repeated");
    EXPECT_EQ(attach(model, {1}, {big + 1}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 1: node 2 is unknown");
    EXPECT_EQ(attach(model, {1, 0}, {x, big}), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 0: sum of constants is infinity");
    EXPECT_EQ(BIYOS_attachexprsp(model.get(), 1, nullptr, &x), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'idx': is null");
    const int function = 0;
    EXPECT_EQ(BIYOS_attachexprsp(model.get(), 1, &function, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'node': is null");
    EXPECT_EQ(BIYOS_attachexprsp(model.get(), -1, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_attachexprsp(model.get(), 0, nullptr, nullptr), BIYOS_OK);
    const Read after = read_functions(model, 2);
    EXPECT_EQ(after.constants, before.constants);
    EXPECT_EQ(after.coefficients, before.coefficients);
    EXPECT_EQ(roots_of(model, 2), (std::vector<BIYOS_offset>(2, biyos::kNoRoot)));
    EXPECT_EQ(next_id(model), 2);
}

TEST(CApiTest, AttachRange)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    add_functions(model, 3);
    const BIYOS_offset x = variable_node(model, 0);
    const BIYOS_offset sine = node_of(model, BIYOS_OP_SIN, {x}, {});
    const std::vector<BIYOS_offset> nodes{sine, x};
    ASSERT_EQ(BIYOS_attachexprrg(model.get(), 1, 3, nodes.data()), BIYOS_OK);
    const std::vector<BIYOS_offset> roots{biyos::kNoRoot, sine, biyos::kNoRoot};
    EXPECT_EQ(roots_of(model, 3), roots);
    EXPECT_EQ(read_functions(model, 3).counts, (std::vector<std::int64_t>{0, 0, 1}));
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(BIYOS_attachexprrg(model.get(), 2, 4, nodes.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 3: unknown");
    EXPECT_EQ(BIYOS_attachexprrg(model.get(), -1, 0, nodes.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function -1: unknown");
    EXPECT_EQ(BIYOS_attachexprrg(model.get(), 0, -1, nodes.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'end': -1 is below 'beg' 0");
    EXPECT_EQ(BIYOS_attachexprrg(model.get(), 0, 1, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'node': is null");
    EXPECT_EQ(BIYOS_attachexprrg(model.get(), 3, 3, nullptr), BIYOS_OK);
    EXPECT_EQ(roots_of(model, 3), roots);
}

TEST(CApiTest, Cleared)
{
    const Env env;
    const Model model(env);
    add_variables(model, 2);
    const double constants[] = {1.0, 2.0, 3.0};
    const std::int64_t counts[] = {1, 2, 1};
    const int variables[] = {0, 0, 1, 1};
    const double coefficients[] = {4.0, 5.0, 6.0, 7.0};
    ASSERT_EQ(BIYOS_addfun(model.get(), 3, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcstrg(model.get(), 0, 3, constants), BIYOS_OK);
    ASSERT_EQ(BIYOS_addrlintermrg(model.get(), 0, 3, counts, variables, coefficients), BIYOS_OK);
    const int owner = 1;
    ASSERT_EQ(BIYOS_addquadterm(model.get(), 1, &owner, variables, variables, coefficients),
              BIYOS_OK);
    const BIYOS_offset x = variable_node(model, 0);
    const BIYOS_offset exponential = node_of(model, BIYOS_OP_EXP, {x}, {});
    ASSERT_EQ(attach(model, {0, 1}, {exponential, exponential}), BIYOS_OK);
    const int cleared[] = {1, 0, 1};
    ASSERT_EQ(BIYOS_clrfunsp(model.get(), 3, cleared), BIYOS_OK);
    const Read functions = read_functions(model, 3);
    EXPECT_EQ(functions.constants, (std::vector<double>{0.0, 0.0, 3.0}));
    EXPECT_EQ(functions.counts, (std::vector<std::int64_t>{0, 0, 1}));
    EXPECT_EQ(functions.variables, std::vector<int>{1});
    EXPECT_EQ(functions.coefficients, std::vector<double>{7.0});
    EXPECT_EQ(read_pairs(model, 3).counts, (std::vector<std::int64_t>{0, 0, 0}));
    EXPECT_EQ(roots_of(model, 3), std::vector<BIYOS_offset>(3, biyos::kNoRoot));
    ASSERT_EQ(attach(model, {1}, {exponential}), BIYOS_OK);
    EXPECT_EQ(roots_of(model, 3)[1], exponential);
    EXPECT_EQ(next_id(model), 2);
    EXPECT_EQ(BIYOS_clrfunsp(model.get(), 0, nullptr), BIYOS_OK);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, RefusedClear)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    add_functions(model, 2);
    const int functions[] = {0, 2};
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(BIYOS_clrfunsp(model.get(), 2, functions), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 2: unknown");
    EXPECT_EQ(BIYOS_clrfunsp(model.get(), 1, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'idx': is null");
    EXPECT_EQ(BIYOS_clrfunsp(model.get(), -1, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'n': -1 is negative");
    EXPECT_EQ(read_functions(model, 2).constants, (std::vector<double>{0.5, 0.5}));
}

TEST(CApiTest, ClearRange)
{
    const Env env;
    const Model model(env);
    add_variables(model, 1);
    add_functions(model, 3);
    const BIYOS_offset x = variable_node(model, 0);
    const BIYOS_offset sine = node_of(model, BIYOS_OP_SIN, {x}, {});
    const BIYOS_offset sum = node_of(model, BIYOS_OP_SUM, {x, sine}, {0.0, 1.0, 1.0});
    ASSERT_EQ(attach(model, {0, 1, 2}, {sum, sum, sum}), BIYOS_OK);
    ASSERT_EQ(BIYOS_clrfunrg(model.get(), 1, 3), BIYOS_OK);
    const Read functions = read_functions(model, 3);
    EXPECT_EQ(functions.constants, (std::vector<double>{0.5, 0.0, 0.0}));
    EXPECT_EQ(functions.counts, (std::vector<std::int64_t>{1, 0, 0}));
    const std::vector<BIYOS_offset> roots{sine, biyos::kNoRoot, biyos::kNoRoot};
    EXPECT_EQ(roots_of(model, 3), roots);
    const std::string bad = "invalid argument: ";
    EXPECT_EQ(BIYOS_clrfunrg(model.get(), 2, 4), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 3: unknown");
    EXPECT_EQ(BIYOS_clrfunrg(model.get(), -1, 0), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function -1: unknown");
    EXPECT_EQ(BIYOS_clrfunrg(model.get(), 0, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'end': -1 is below 'beg' 0");
    EXPECT_EQ(BIYOS_clrfunrg(model.get(), 3, 3), BIYOS_OK);
    EXPECT_EQ(roots_of(model, 3), roots);
}

TEST(CApiTest, ReadExpressions)
{
    const Env env;
    const Model model(env);
    add_ratio(model);
    const Program all = read_range(model, 0, 3);
    EXPECT_EQ(all.counts, (std::vector<std::int64_t>{3, 1, 3}));
    const std::vector<int> codes{
        BIYOS_OP_VAR,
        BIYOS_OP_SIN,
        BIYOS_OP_KEEP,
        BIYOS_OP_CONST,
        BIYOS_OP_REF,
        BIYOS_OP_VAR,
        BIYOS_OP_DIV,
    };
    EXPECT_EQ(all.codes, codes);
    EXPECT_EQ(all.arguments, (std::vector<std::int64_t>{0, 1, 1, 0, 2, 1, 2}));
    EXPECT_EQ(bits_of(all.values), bits_of({0.0}));
    const Program twice = read_list(model, {2, 2});
    EXPECT_EQ(twice.counts, (std::vector<std::int64_t>{5, 1}));
    const std::vector<int> shared{
        BIYOS_OP_VAR,
        BIYOS_OP_SIN,
        BIYOS_OP_VAR,
        BIYOS_OP_DIV,
        BIYOS_OP_KEEP,
        BIYOS_OP_REF,
    };
    EXPECT_EQ(twice.codes, shared);
    EXPECT_EQ(twice.arguments, (std::vector<std::int64_t>{0, 1, 1, 2, 1, 4}));
    EXPECT_TRUE(twice.values.empty());
    EXPECT_TRUE(read_range(model, 3, 0).counts.empty());
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, PartialRead)
{
    const Env env;
    const Model model(env);
    add_ratio(model);
    std::vector<int> codes(7, -1);
    const BIYOS_status status =
        BIYOS_getexprrg(model.get(), 0, 3, 7, 1, nullptr, codes.data(), nullptr, nullptr);
    EXPECT_EQ(status, BIYOS_OK);
    EXPECT_EQ(codes, read_range(model, 0, 3).codes);
    EXPECT_EQ(BIYOS_getexprrg(model.get(), 1, 1, 0, 0, nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
}

TEST(CApiTest, RefusedRead)
{
    const Env env;
    const Model model(env);
    add_ratio(model);
    const std::string bad = "invalid argument: ";
    Program read = sized(6, 1, 3);
    EXPECT_EQ(get_range(model, 0, 3, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'cap': holds 6 of the 7 entries");
    read = sized(7, 0, 3);
    EXPECT_EQ(get_range(model, 0, 3, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'valcap': holds 0 of the 1 values");
    EXPECT_EQ(read.counts, std::vector<std::int64_t>(3, -1));
    EXPECT_EQ(read.codes, std::vector<int>(7, -1));
    EXPECT_EQ(read.arguments, std::vector<std::int64_t>(7, -1));
    EXPECT_EQ(BIYOS_getexprrg(model.get(), 0, 3, -1, 1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'cap': -1 is negative");
    EXPECT_EQ(get_range(model, 2, 2, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 3: unknown");
    const int unknown[] = {0, 3};
    EXPECT_EQ(BIYOS_getexprsp(model.get(), 2, unknown, 7, 1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 3: unknown");
    EXPECT_EQ(BIYOS_getexprsp(model.get(), 1, nullptr, 7, 1, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'idx': is null");
}

TEST(CApiTest, RefusedCount)
{
    const Env env;
    const Model model(env);
    add_ratio(model);
    const std::string bad = "invalid argument: ";
    BIYOS_offset nnz = -1;
    BIYOS_offset nval = -1;
    EXPECT_EQ(BIYOS_getnexprrg(model.get(), 0, 3, nullptr, &nval), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'nnz': is null");
    EXPECT_EQ(BIYOS_getnexprrg(model.get(), 0, 3, &nnz, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'nval': is null");
    EXPECT_EQ(BIYOS_getnexprrg(model.get(), 0, -1, &nnz, &nval), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "argument 'end': -1 is below 'beg' 0");
    const int unknown[] = {3};
    EXPECT_EQ(BIYOS_getnexprsp(model.get(), 1, unknown, &nnz, &nval), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), bad + "function 3: unknown");
    EXPECT_EQ(nnz, -1);
    EXPECT_EQ(nval, -1);
}
