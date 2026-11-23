#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/terms.hpp"
#include "model/callbacks.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{

using biyos::Callback;
using biyos::difference;
using biyos::Index;
using biyos::kNoCallback;
using biyos::kNoRoot;
using biyos::Model;
using biyos::Offset;
using biyos::Request;
using biyos::UserFunction;
using biyos::Variable;
using biyos::VariablePair;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
const std::string kBad = "invalid argument: ";

Model with_functions(std::size_t n)
{
    Model model;
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    model.add_variables(std::vector<Variable>(3, free));
    model.add_functions(std::vector<double>(n, 0.0), std::vector<Offset>(n + 1, 0), {});
    return model;
}

int succeed(const Request &, std::span<double>, void *)
{
    return 0;
}

Index add(Model &model, const std::vector<Index> &functions)
{
    return model.add_callback(functions, nullptr);
}

std::string add_refusal(Model &model, const std::vector<Index> &functions)
{
    return refusal([&] { add(model, functions); });
}

void set_jacobian(Model &model,
                  Index callback,
                  const std::vector<Offset> &starts,
                  const std::vector<Index> &variables)
{
    model.set_jacobian(callback, starts, variables, succeed);
}

std::string jacobian_refusal(Model &model,
                             const std::vector<Offset> &starts,
                             const std::vector<Index> &variables)
{
    return refusal([&] { set_jacobian(model, 0, starts, variables); });
}

void set_hessian(Model &model, const std::vector<Index> &first, const std::vector<Index> &second)
{
    model.set_hessian(0, first, second, succeed);
}

std::string hessian_refusal(Model &model,
                            const std::vector<Index> &first,
                            const std::vector<Index> &second)
{
    return refusal([&] { set_hessian(model, first, second); });
}

std::vector<Index> firsts(const Callback &callback)
{
    std::vector<Index> found;
    for (const VariablePair &pair : callback.hessian_pairs)
    {
        found.push_back(pair.first);
    }
    return found;
}

std::vector<Index> seconds(const Callback &callback)
{
    std::vector<Index> found;
    for (const VariablePair &pair : callback.hessian_pairs)
    {
        found.push_back(pair.second);
    }
    return found;
}

void attach_sine(Model &model, Index function)
{
    const Offset x = model.add_variable_node(0);
    const std::vector<Offset> children{x};
    const Offset sine = model.add_node(BIYOS_OP_SIN, children, {});
    const std::vector<Index> functions{function};
    const std::vector<Offset> nodes{sine};
    model.attach_expressions(functions, nodes);
}

int counted(const Request &, std::span<double>, void *data)
{
    ++*static_cast<int *>(data);
    return 0;
}

int calls_of(const UserFunction &function)
{
    int calls = 0;
    const Request request{
        .model = nullptr,
        .callback = 0,
        .thread = 0,
        .point = {},
        .weights = {},
        .direction = {},
    };
    function(request, {}, &calls);
    return calls;
}

}

TEST(CallbackTest, Added)
{
    Model model = with_functions(4);
    int data = 0;
    EXPECT_EQ(model.n_callbacks(), 0);
    EXPECT_EQ(model.add_callback(std::vector<Index>{2, 0}, &data), 0);
    EXPECT_EQ(model.add_callback(std::vector<Index>{3}, nullptr), 1);
    EXPECT_EQ(model.n_callbacks(), 2);
    const Callback &first = model.callback(0);
    EXPECT_EQ(first.functions, (std::vector<Index>{2, 0}));
    EXPECT_EQ(first.data, &data);
    EXPECT_EQ(first.jacobian_starts, (std::vector<Offset>{0, 0, 0}));
    EXPECT_TRUE(first.jacobian_variables.empty());
    EXPECT_TRUE(first.hessian_pairs.empty());
    EXPECT_FALSE(first.values);
    EXPECT_FALSE(first.jacobian);
    EXPECT_FALSE(first.hessian);
    EXPECT_FALSE(first.product);
    EXPECT_EQ(model.callback_of(0), 0);
    EXPECT_EQ(model.callback_of(1), kNoCallback);
    EXPECT_EQ(model.callback_of(2), 0);
    EXPECT_EQ(model.callback_of(3), 1);
}

TEST(CallbackTest, LaterFunction)
{
    Model model = with_functions(1);
    add(model, {0});
    model.add_functions(std::vector<double>{0.0}, std::vector<Offset>{0, 0}, {});
    EXPECT_EQ(model.callback_of(1), kNoCallback);
    EXPECT_EQ(add(model, {1}), 1);
    EXPECT_EQ(model.callback_of(1), 1);
}

TEST(CallbackTest, AddRefused)
{
    Model model = with_functions(3);
    add(model, {1});
    EXPECT_EQ(add_refusal(model, {}), kBad + "argument 'n': 0 is not positive");
    EXPECT_EQ(add_refusal(model, {0, 3}), kBad + "function 3: unknown");
    EXPECT_EQ(add_refusal(model, {-1}), kBad + "function -1: unknown");
    EXPECT_EQ(add_refusal(model, {2, 0, 2}), kBad + "argument 'funidx': function 2 is repeated");
    EXPECT_EQ(add_refusal(model, {0, 1}), kBad + "function 1: is in callback 0");
    EXPECT_EQ(model.n_callbacks(), 1);
    EXPECT_EQ(model.callback_of(0), kNoCallback);
}

TEST(CallbackTest, RootFirst)
{
    Model model = with_functions(2);
    attach_sine(model, 1);
    EXPECT_EQ(add_refusal(model, {0, 1}), kBad + "function 1: has an expression");
    EXPECT_EQ(model.n_callbacks(), 0);
    model.clear_functions(std::vector<Index>{1});
    EXPECT_EQ(add(model, {0, 1}), 0);
}

TEST(CallbackTest, RootAfter)
{
    Model model = with_functions(2);
    add(model, {1});
    const Offset size = model.graph().size();
    EXPECT_EQ(refusal([&] { attach_sine(model, 1); }), kBad + "function 1: is in callback 0");
    EXPECT_EQ(model.roots(1, 1).front(), kNoRoot);
    EXPECT_EQ(model.graph().size(), size + 2);
    attach_sine(model, 0);
    EXPECT_NE(model.roots(0, 1).front(), kNoRoot);
}

TEST(CallbackTest, LinearAttach)
{
    Model model = with_functions(1);
    add(model, {0});
    const Offset x = model.add_variable_node(2);
    const std::vector<Offset> children{x};
    const Offset scaled = model.add_node(BIYOS_OP_SUM, children, std::vector<double>{1.0, 3.0});
    model.attach_expressions(std::vector<Index>{0}, std::vector<Offset>{scaled});
    EXPECT_EQ(model.roots(0, 1).front(), kNoRoot);
    ASSERT_EQ(model.linear_terms(0, 1).size(), 1U);
    EXPECT_EQ(model.linear_terms(0, 1).front().variable, 2);
}

TEST(CallbackTest, Cleared)
{
    Model model = with_functions(2);
    add(model, {1, 0});
    model.clear_functions(std::vector<Index>{0, 1});
    EXPECT_EQ(model.callback_of(0), 0);
    EXPECT_EQ(model.callback_of(1), 0);
}

TEST(CallbackTest, Values)
{
    Model model = with_functions(1);
    int calls = 0;
    model.add_callback(std::vector<Index>{0}, &calls);
    model.set_values(0, counted);
    const Callback &held = model.callback(0);
    std::vector<double> out(1);
    const Request request{
        .model = nullptr,
        .callback = 0,
        .thread = 0,
        .point = {},
        .weights = {},
        .direction = {},
    };
    EXPECT_EQ(held.values(request, out, held.data), 0);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(refusal([&] { model.set_values(1, succeed); }), kBad + "callback 1: unknown");
    EXPECT_EQ(refusal([&] { model.set_product(-1, succeed); }), kBad + "callback -1: unknown");
}

TEST(CallbackTest, Data)
{
    Model model = with_functions(1);
    int first = 0;
    int second = 0;
    model.add_callback(std::vector<Index>{0}, &first);
    model.set_data(0, &second);
    EXPECT_EQ(model.callback(0).data, &second);
    model.set_data(0, nullptr);
    EXPECT_EQ(model.callback(0).data, nullptr);
    EXPECT_EQ(refusal([&] { model.set_data(1, &first); }), kBad + "callback 1: unknown");
    EXPECT_EQ(refusal([&] { model.callback(1); }), kBad + "callback 1: unknown");
}

TEST(CallbackTest, Jacobian)
{
    Model model = with_functions(3);
    add(model, {2, 0});
    set_jacobian(model, 0, {0, 2, 3}, {0, 2, 1});
    const Callback &held = model.callback(0);
    EXPECT_EQ(held.jacobian_starts, (std::vector<Offset>{0, 2, 3}));
    EXPECT_EQ(held.jacobian_variables, (std::vector<Index>{0, 2, 1}));
    EXPECT_TRUE(held.jacobian);
    set_jacobian(model, 0, {0, 0, 1}, {2});
    EXPECT_EQ(held.jacobian_starts, (std::vector<Offset>{0, 0, 1}));
    EXPECT_EQ(held.jacobian_variables, (std::vector<Index>{2}));
}

TEST(CallbackTest, JacobianRefused)
{
    Model model = with_functions(3);
    add(model, {2, 0});
    model.set_jacobian(0, std::vector<Offset>{0, 1, 1}, std::vector<Index>{1}, counted);
    const std::string row = kBad + "callback 0: Jacobian of function ";
    EXPECT_EQ(jacobian_refusal(model, {0, 1, 2}, {0, 3}), row + "0: variable 3 is unknown");
    EXPECT_EQ(jacobian_refusal(model, {0, 1, 2}, {-1, 0}), row + "2: variable -1 is unknown");
    EXPECT_EQ(jacobian_refusal(model, {0, 2, 2}, {2, 1}),
              row + "2: variable 1 is listed after variable 2");
    EXPECT_EQ(jacobian_refusal(model, {0, 1, 3}, {0, 1, 1}), row + "0: variable 1 is repeated");
    EXPECT_EQ(jacobian_refusal(model, {1, 1, 2}, {0, 1}),
              kBad + "argument 'off': begins at 1, not 0");
    EXPECT_EQ(jacobian_refusal(model, {0, 2, 1}, {0, 1}),
              kBad + "argument 'off': decreases from 2 to 1 at entry 2");
    EXPECT_EQ(refusal([&] { set_jacobian(model, 1, {0, 0, 0}, {}); }),
              kBad + "callback 1: unknown");
    const Callback &held = model.callback(0);
    EXPECT_EQ(held.jacobian_starts, (std::vector<Offset>{0, 1, 1}));
    EXPECT_EQ(held.jacobian_variables, (std::vector<Index>{1}));
    EXPECT_EQ(calls_of(held.jacobian), 1);
}

TEST(CallbackTest, Hessian)
{
    Model model = with_functions(1);
    add(model, {0});
    set_hessian(model, {0, 0, 1, 2}, {0, 2, 1, 2});
    const Callback &held = model.callback(0);
    EXPECT_EQ(firsts(held), (std::vector<Index>{0, 0, 1, 2}));
    EXPECT_EQ(seconds(held), (std::vector<Index>{0, 2, 1, 2}));
    EXPECT_TRUE(held.hessian);
    set_hessian(model, {}, {});
    EXPECT_TRUE(held.hessian_pairs.empty());
}

TEST(CallbackTest, HessianRefused)
{
    Model model = with_functions(1);
    add(model, {0});
    model.set_hessian(0, std::vector<Index>{1}, std::vector<Index>{2}, counted);
    const std::string bad = kBad + "callback 0: Hessian ";
    EXPECT_EQ(hessian_refusal(model, {0, 3}, {0, 0}), bad + "variable 3 is unknown");
    EXPECT_EQ(hessian_refusal(model, {0}, {-1}), bad + "variable -1 is unknown");
    EXPECT_EQ(hessian_refusal(model, {0, 2}, {1, 1}),
              bad + "pair (2, 1) has its second variable below its first");
    EXPECT_EQ(hessian_refusal(model, {1, 0}, {1, 2}),
              bad + "pair (0, 2) is listed after pair (1, 1)");
    EXPECT_EQ(hessian_refusal(model, {0, 0}, {2, 1}),
              bad + "pair (0, 1) is listed after pair (0, 2)");
    EXPECT_EQ(hessian_refusal(model, {0, 1, 1}, {1, 2, 2}), bad + "pair (1, 2) is repeated");
    EXPECT_EQ(refusal([&] { model.set_hessian(1, {}, {}, succeed); }),
              kBad + "callback 1: unknown");
    EXPECT_EQ(firsts(model.callback(0)), (std::vector<Index>{1}));
    EXPECT_EQ(seconds(model.callback(0)), (std::vector<Index>{2}));
    EXPECT_EQ(calls_of(model.callback(0).hessian), 1);
}

TEST(CallbackTest, Copied)
{
    Model model = with_functions(2);
    int calls = 0;
    model.add_callback(std::vector<Index>{1}, &calls);
    model.set_values(0, counted);
    set_jacobian(model, 0, {0, 2}, {0, 1});
    const Model copy = model;
    const Callback &held = copy.callback(0);
    EXPECT_EQ(held.data, &calls);
    EXPECT_EQ(held.jacobian_variables, (std::vector<Index>{0, 1}));
    EXPECT_EQ(copy.callback_of(1), 0);
    std::vector<double> out(1);
    const Request request{
        .model = nullptr,
        .callback = 0,
        .thread = 0,
        .point = {},
        .weights = {},
        .direction = {},
    };
    EXPECT_EQ(held.values(request, out, held.data), 0);
    EXPECT_EQ(calls, 1);
    model.set_data(0, nullptr);
    EXPECT_EQ(copy.callback(0).data, &calls);
}

TEST(CallbackTest, Difference)
{
    Model model = with_functions(3);
    int data = 0;
    model.add_callback(std::vector<Index>{2, 0}, &data);
    set_jacobian(model, 0, {0, 1, 2}, {1, 2});
    set_hessian(model, {0, 1}, {1, 1});
    const Model copy = model;
    EXPECT_EQ(difference(model, copy), "");
    Model more = copy;
    add(more, {1});
    EXPECT_EQ(difference(model, more), "callback count 1 against 2");
    Model other = with_functions(3);
    other.add_callback(std::vector<Index>{1, 0}, &data);
    EXPECT_EQ(difference(model, other), "callback 0: covered function 0: index 2 against 1");
    Model moved = copy;
    moved.set_data(0, nullptr);
    EXPECT_EQ(difference(model, moved), "callback 0: data differs");
    Model valued = copy;
    valued.set_values(0, succeed);
    EXPECT_EQ(difference(model, valued), "callback 0: value function none against given");
    Model row = copy;
    set_jacobian(row, 0, {0, 1, 2}, {1, 0});
    EXPECT_EQ(difference(model, row), "callback 0: Jacobian row 1: entry 0: variable 2 against 0");
    Model wider = copy;
    set_jacobian(wider, 0, {0, 2, 3}, {1, 2, 2});
    EXPECT_EQ(difference(model, wider), "callback 0: Jacobian row 0: entry count 1 against 2");
    Model pairs = copy;
    set_hessian(pairs, {0, 1}, {1, 2});
    EXPECT_EQ(difference(model, pairs), "callback 0: Hessian entry 1: second variable 1 against 2");
    Model fewer = copy;
    set_hessian(fewer, {0}, {1});
    EXPECT_EQ(difference(model, fewer), "callback 0: Hessian entry count 2 against 1");
    Model product = copy;
    product.set_product(0, succeed);
    EXPECT_EQ(difference(product, model), "callback 0: Hessian product given against none");
}
