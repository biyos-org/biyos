#include "biyos/biyos.h"
#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/compare/compare.hpp"
#include "model/expressions/callbacks.hpp"
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
using biyos::IndexPair;
using biyos::kNoRoot;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::Request;
using biyos::Routine;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::refusal;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
const std::string kBad = "invalid argument: ";

Model with_functions(std::size_t n)
{
    Model model;
    const Variable free{
        .kind = VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    model.add_variables(std::vector<Variable>(3, free));
    model.add_functions(std::vector<Real>(n, 0.0), std::vector<Offset>(n + 1, 0), {});
    return model;
}

int succeed(const Request &, std::span<Real>, void *)
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
                  const std::vector<Index> &functions,
                  const std::vector<Index> &variables)
{
    model.set_callback_jacobian(callback, functions, variables, succeed);
}

std::string jacobian_refusal(Model &model,
                             const std::vector<Index> &functions,
                             const std::vector<Index> &variables)
{
    return refusal([&] { set_jacobian(model, 0, functions, variables); });
}

void set_hessian(Model &model, const std::vector<Index> &first, const std::vector<Index> &second)
{
    model.set_callback_hessian(0, first, second, succeed);
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
    for (const IndexPair &pair : callback.hessian_pairs)
    {
        found.push_back(pair.row);
    }
    return found;
}

std::vector<Index> seconds(const Callback &callback)
{
    std::vector<Index> found;
    for (const IndexPair &pair : callback.hessian_pairs)
    {
        found.push_back(pair.column);
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

int counted(const Request &, std::span<Real>, void *data)
{
    ++*static_cast<int *>(data);
    return 0;
}

int calls_of(const Routine &function)
{
    int calls = 0;
    const Request request{
        .session = nullptr,
        .callback = 0,
        .thread = 0,
        .x = {},
        .w = {},
        .v = {},
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
    EXPECT_EQ(first.covered, (std::vector<Index>{2, 0}));
    EXPECT_EQ(first.data, &data);
    EXPECT_TRUE(first.jacobian_pairs.empty());
    EXPECT_TRUE(first.hessian_pairs.empty());
    EXPECT_FALSE(first.function);
    EXPECT_FALSE(first.jacobian);
    EXPECT_FALSE(first.hessian);
    EXPECT_FALSE(first.hessian_product);
    EXPECT_EQ(model.callback(1).covered, (std::vector<Index>{3}));
}

TEST(CallbackTest, AddRefused)
{
    Model model = with_functions(3);
    add(model, {1});
    EXPECT_EQ(add_refusal(model, {}), kBad + "argument 'm': 0 is not positive");
    EXPECT_EQ(add_refusal(model, {0, 3}), kBad + "function 3: unknown");
    EXPECT_EQ(add_refusal(model, {-1}), kBad + "function -1: unknown");
    EXPECT_EQ(add_refusal(model, {2, 0, 2}), kBad + "argument 'fun': function 2 is repeated");
    EXPECT_EQ(add_refusal(model, {1, 1}), kBad + "argument 'fun': function 1 is repeated");
    EXPECT_EQ(model.n_callbacks(), 1);
}

TEST(CallbackTest, Shared)
{
    Model model = with_functions(3);
    attach_sine(model, 1);
    EXPECT_EQ(add(model, {1, 0}), 0);
    EXPECT_EQ(add(model, {2, 1}), 1);
    attach_sine(model, 1);
    EXPECT_NE(model.roots(1, 2).front(), kNoRoot);
    const Model copy = model;
    EXPECT_EQ(difference(model, copy), "");
    EXPECT_EQ(copy.callback(0).covered, (std::vector<Index>{1, 0}));
    EXPECT_EQ(copy.callback(1).covered, (std::vector<Index>{2, 1}));
}

TEST(CallbackTest, RootFirst)
{
    Model model = with_functions(2);
    attach_sine(model, 1);
    const Offset root = model.roots(1, 2).front();
    EXPECT_EQ(add(model, {0, 1}), 0);
    EXPECT_EQ(model.callback(0).covered, (std::vector<Index>{0, 1}));
    EXPECT_EQ(model.roots(1, 2).front(), root);
}

TEST(CallbackTest, RootAfter)
{
    Model model = with_functions(2);
    Model plain = model;
    add(model, {1});
    attach_sine(model, 1);
    attach_sine(model, 1);
    attach_sine(plain, 1);
    attach_sine(plain, 1);
    EXPECT_NE(model.roots(1, 2).front(), kNoRoot);
    EXPECT_EQ(difference(model, plain), "callback count 1 against 0");
}

TEST(CallbackTest, LinearAttach)
{
    Model model = with_functions(1);
    add(model, {0});
    const Offset x = model.add_variable_node(2);
    const std::vector<Offset> children{x};
    const Offset scaled = model.add_node(BIYOS_OP_SUM, children, std::vector<Real>{1.0, 3.0});
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
    EXPECT_EQ(model.callback(0).covered, (std::vector<Index>{1, 0}));
}

TEST(CallbackTest, Values)
{
    Model model = with_functions(1);
    int calls = 0;
    model.add_callback(std::vector<Index>{0}, &calls);
    model.set_callback_function(0, counted);
    const Callback &held = model.callback(0);
    std::vector<Real> out(1);
    const Request request{
        .session = nullptr,
        .callback = 0,
        .thread = 0,
        .x = {},
        .w = {},
        .v = {},
    };
    EXPECT_EQ(held.function(request, out, held.data), 0);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(refusal([&] { model.set_callback_function(1, succeed); }),
              kBad + "callback 1: unknown");
    EXPECT_EQ(refusal([&] { model.set_callback_hessian_product(-1, succeed); }),
              kBad + "callback -1: unknown");
}

TEST(CallbackTest, Data)
{
    Model model = with_functions(1);
    int first = 0;
    int second = 0;
    model.add_callback(std::vector<Index>{0}, &first);
    model.set_callback_data(0, &second);
    EXPECT_EQ(model.callback(0).data, &second);
    model.set_callback_data(0, nullptr);
    EXPECT_EQ(model.callback(0).data, nullptr);
    EXPECT_EQ(refusal([&] { model.set_callback_data(1, &first); }), kBad + "callback 1: unknown");
    EXPECT_EQ(refusal([&] { model.callback(1); }), kBad + "callback 1: unknown");
}

TEST(CallbackTest, Jacobian)
{
    Model model = with_functions(3);
    add(model, {2, 0});
    set_jacobian(model, 0, {0, 2, 0, 0}, {2, 1, 0, 2});
    const Callback &held = model.callback(0);
    EXPECT_EQ(held.jacobian_pairs, (std::vector<IndexPair>{{1, 2}, {0, 1}, {1, 0}, {1, 2}}));
    EXPECT_TRUE(held.jacobian);
    set_jacobian(model, 0, {}, {});
    EXPECT_TRUE(held.jacobian_pairs.empty());
}

TEST(CallbackTest, JacobianRefused)
{
    Model model = with_functions(3);
    add(model, {2, 0});
    model.set_callback_jacobian(0, std::vector<Index>{2}, std::vector<Index>{1}, counted);
    const std::string bad = kBad + "callback 0: Jacobian ";
    EXPECT_EQ(jacobian_refusal(model, {0, 1}, {0, 0}), bad + "function 1 is not covered");
    EXPECT_EQ(jacobian_refusal(model, {2, 3}, {0, 0}), bad + "function 3 is not covered");
    EXPECT_EQ(jacobian_refusal(model, {-1}, {0}), bad + "function -1 is not covered");
    EXPECT_EQ(jacobian_refusal(model, {2, 0}, {0, 3}),
              bad + "of function 0: variable 3 is unknown");
    EXPECT_EQ(jacobian_refusal(model, {2}, {-1}), bad + "of function 2: variable -1 is unknown");
    EXPECT_EQ(refusal([&] { set_jacobian(model, 1, {}, {}); }), kBad + "callback 1: unknown");
    const Callback &held = model.callback(0);
    EXPECT_EQ(held.jacobian_pairs, (std::vector<IndexPair>{{0, 1}}));
    EXPECT_EQ(calls_of(held.jacobian), 1);
}

TEST(CallbackTest, Uncovered)
{
    Model model = with_functions(4);
    add(model, {3, 1});
    set_jacobian(model, 0, {1, 3, 1}, {2, 0, 1});
    model.delete_functions(std::vector<Index>{0, 2});
    const Callback &held = model.callback(0);
    EXPECT_EQ(held.covered, (std::vector<Index>{1, 0}));
    EXPECT_EQ(held.jacobian_pairs, (std::vector<IndexPair>{{1, 2}, {0, 0}, {1, 1}}));
}

TEST(CallbackTest, Hessian)
{
    Model model = with_functions(1);
    add(model, {0});
    set_hessian(model, {1, 0, 2, 0, 1}, {1, 2, 2, 0, 1});
    const Callback &held = model.callback(0);
    EXPECT_EQ(firsts(held), (std::vector<Index>{1, 0, 2, 0, 1}));
    EXPECT_EQ(seconds(held), (std::vector<Index>{1, 2, 2, 0, 1}));
    EXPECT_TRUE(held.hessian);
    set_hessian(model, {}, {});
    EXPECT_TRUE(held.hessian_pairs.empty());
}

TEST(CallbackTest, HessianRefused)
{
    Model model = with_functions(1);
    add(model, {0});
    model.set_callback_hessian(0, std::vector<Index>{1}, std::vector<Index>{2}, counted);
    const std::string bad = kBad + "callback 0: Hessian ";
    EXPECT_EQ(hessian_refusal(model, {0, 3}, {0, 0}), bad + "variable 3 is unknown");
    EXPECT_EQ(hessian_refusal(model, {0}, {-1}), bad + "variable -1 is unknown");
    EXPECT_EQ(hessian_refusal(model, {0, 2}, {1, 1}),
              bad + "pair (2, 1) has its second variable below its first");
    EXPECT_EQ(hessian_refusal(model, {1, 1, 2}, {2, 2, 0}),
              bad + "pair (2, 0) has its second variable below its first");
    EXPECT_EQ(refusal([&] { model.set_callback_hessian(1, {}, {}, succeed); }),
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
    model.set_callback_function(0, counted);
    set_jacobian(model, 0, {1, 1, 1}, {2, 0, 2});
    set_hessian(model, {1, 0, 1}, {2, 1, 2});
    const Model copy = model;
    const Callback &held = copy.callback(0);
    EXPECT_EQ(held.data, &calls);
    EXPECT_EQ(held.jacobian_pairs, (std::vector<IndexPair>{{0, 2}, {0, 0}, {0, 2}}));
    EXPECT_EQ(held.hessian_pairs, (std::vector<IndexPair>{{1, 2}, {0, 1}, {1, 2}}));
    EXPECT_EQ(held.covered, (std::vector<Index>{1}));
    std::vector<Real> out(1);
    const Request request{
        .session = nullptr,
        .callback = 0,
        .thread = 0,
        .x = {},
        .w = {},
        .v = {},
    };
    EXPECT_EQ(held.function(request, out, held.data), 0);
    EXPECT_EQ(calls, 1);
    model.set_callback_data(0, nullptr);
    EXPECT_EQ(copy.callback(0).data, &calls);
}

TEST(CallbackTest, Difference)
{
    Model model = with_functions(3);
    int data = 0;
    model.add_callback(std::vector<Index>{2, 0}, &data);
    set_jacobian(model, 0, {2, 0}, {1, 2});
    set_hessian(model, {0, 1}, {1, 1});
    const Model copy = model;
    EXPECT_EQ(difference(model, copy), "");
    Model more = copy;
    add(more, {1});
    EXPECT_EQ(difference(model, more), "callback count 1 against 2");
    Model other = with_functions(3);
    other.add_callback(std::vector<Index>{1, 0}, &data);
    EXPECT_EQ(difference(model, other), "callback 0: covered function 0: index 2 against 1");
    Model second = copy;
    add(second, {0, 1});
    Model swapped = copy;
    add(swapped, {0, 2});
    EXPECT_EQ(difference(second, swapped), "callback 1: covered function 1: index 1 against 2");
    Model moved = copy;
    moved.set_callback_data(0, nullptr);
    EXPECT_EQ(difference(model, moved), "callback 0: data differs");
    Model valued = copy;
    valued.set_callback_function(0, succeed);
    EXPECT_EQ(difference(model, valued), "callback 0: value function none against given");
    Model reordered = copy;
    set_jacobian(reordered, 0, {0, 2}, {2, 1});
    EXPECT_EQ(difference(model, reordered), "callback 0: Jacobian entry 0: row 0 against 1");
    Model column = copy;
    set_jacobian(column, 0, {2, 0}, {1, 0});
    EXPECT_EQ(difference(model, column), "callback 0: Jacobian entry 1: column 2 against 0");
    Model wider = copy;
    set_jacobian(wider, 0, {2, 0, 0}, {1, 2, 2});
    EXPECT_EQ(difference(model, wider), "callback 0: Jacobian entry count 2 against 3");
    Model flipped = copy;
    set_hessian(flipped, {1, 0}, {1, 1});
    EXPECT_EQ(difference(model, flipped), "callback 0: Hessian entry 0: row 0 against 1");
    Model pairs = copy;
    set_hessian(pairs, {0, 1}, {1, 2});
    EXPECT_EQ(difference(model, pairs), "callback 0: Hessian entry 1: column 1 against 2");
    Model fewer = copy;
    set_hessian(fewer, {0}, {1});
    EXPECT_EQ(difference(model, fewer), "callback 0: Hessian entry count 2 against 1");
    Model product = copy;
    product.set_callback_hessian_product(0, succeed);
    EXPECT_EQ(difference(product, model), "callback 0: Hessian product given against none");
}

TEST(CallbackTest, Deleted)
{
    Model model = with_functions(4);
    int data = 0;
    add(model, {0});
    model.add_callback(std::vector<Index>{3, 1}, &data);
    add(model, {2});
    add(model, {1});
    model.set_callback_function(1, counted);
    const std::vector<Index> map = model.delete_callbacks(std::vector<Index>{2, 0});
    EXPECT_EQ(map, (std::vector<Index>{-1, 0, -1, 1}));
    EXPECT_EQ(model.n_callbacks(), 2);
    const Callback &held = model.callback(0);
    EXPECT_EQ(held.covered, (std::vector<Index>{3, 1}));
    EXPECT_EQ(held.data, &data);
    EXPECT_EQ(calls_of(held.function), 1);
    EXPECT_EQ(model.callback(1).covered, (std::vector<Index>{1}));
}

TEST(CallbackTest, DeleteRefused)
{
    Model model = with_functions(2);
    add(model, {0});
    add(model, {1});
    EXPECT_EQ(refusal([&] { model.delete_callbacks(std::vector<Index>{2}); }),
              kBad + "callback 2: unknown");
    EXPECT_EQ(refusal([&] { model.delete_callbacks(std::vector<Index>{1, 1}); }),
              kBad + "argument 'idx': callback 1 is repeated");
    EXPECT_EQ(model.n_callbacks(), 2);
}

TEST(CallbackTest, DeleteFrees)
{
    Model model = with_functions(3);
    add(model, {1, 0});
    add(model, {2, 1});
    add(model, {0});
    model.delete_callbacks(std::vector<Index>{1});
    EXPECT_EQ(model.delete_functions(std::vector<Index>{2}), (std::vector<Index>{0, 1, -1}));
    model.delete_callbacks(std::vector<Index>{0});
    EXPECT_EQ(model.delete_functions(std::vector<Index>{1}), (std::vector<Index>{0, -1}));
    EXPECT_EQ(model.callback(0).covered, (std::vector<Index>{0}));
}

TEST(CallbackTest, DeleteLast)
{
    Model model = with_functions(2);
    add(model, {0});
    add(model, {1});
    model.delete_callbacks(std::vector<Index>{0, 1});
    EXPECT_EQ(model.n_callbacks(), 0);
    EXPECT_EQ(model.delete_variables(std::vector<Index>{1}), (std::vector<Index>{0, -1, 1}));
    EXPECT_EQ(model.n_variables(), 2);
}
