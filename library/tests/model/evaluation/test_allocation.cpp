#include "allocations.hpp"
#include "biyos/biyos.h"
#include "core/env/env.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "expr/forms/terms.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace
{

using biyos::Env;
using biyos::Evaluation;
using biyos::Index;
using biyos::Lookup;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::Request;
using biyos::VariableKind;
using biyos::widest;
using biyos::test::allocations;

const std::vector<Real> kPoint{0.5, -1.5};

int squared(const Request &request, std::span<Real> out, void *)
{
    out[0] = request.x[0] * request.x[0];
    return 0;
}

int doubled(const Request &request, std::span<Real> out, void *)
{
    out[0] = 2.0 * request.x[0];
    return 0;
}

int curved(const Request &request, std::span<Real> out, void *)
{
    out[0] = 2.0 * request.w[0];
    return 0;
}

int multiplied(const Request &request, std::span<Real> out, void *)
{
    out[0] = 2.0 * request.w[0] * request.v[0];
    return 0;
}

void add_callback(Model &model)
{
    const std::array covered{1};
    const Index callback = model.add_callback(covered, nullptr);
    model.set_callback_function(callback, squared);
    const std::array<Index, 1> functions{1};
    const std::array<Index, 1> variables{0};
    model.set_callback_jacobian(callback, functions, variables, doubled);
    model.set_callback_hessian(callback, variables, variables, curved);
}

Model evaluated()
{
    Model model;
    model.add_variables(std::vector(2, widest(VariableKind::Continuous)));
    const std::array<Offset, 3> starts{0, 1, 2};
    model.add_functions(
        std::vector{1.0, 2.0},
        starts,
        {{.variable = 0, .coefficient = 1.5}, {.variable = 1, .coefficient = -1.0}});
    model.add_quadratic_terms(std::vector<Index>{0},
                              std::array<Offset, 2>{0, 1},
                              {{.row = 0, .column = 1, .coefficient = 0.5}});
    const std::array children{model.add_variable_node(1)};
    const Offset node = model.add_node(BIYOS_OP_EXP, children, {});
    model.attach_expressions(std::vector<Index>{1}, std::span(&node, 1));
    add_callback(model);
    return model;
}

Model listed()
{
    Model model = evaluated();
    const std::array covered{0};
    const std::array<Index, 1> variables{1};
    const Index callback = model.add_callback(covered, variables, nullptr);
    model.set_callback_function(callback, squared);
    model.set_callback_jacobian(callback, covered, variables, doubled);
    model.set_callback_hessian(callback, variables, variables, curved);
    model.set_callback_hessian_product(callback, multiplied);
    return model;
}

std::size_t allocated(const Model &model)
{
    Env env;
    Evaluation evaluation(model, env.stream(), env.pool());
    const Lookup lookup(env.options());
    std::vector<Real> f(2);
    std::vector<Real> g(evaluation.jacobian_pattern().variables.size());
    std::vector<Real> h(evaluation.hessian_pattern().rows.size());
    std::vector<Real> hv(2);
    const std::vector<Real> w{0.5, 2.0};
    const std::size_t start = allocations();
    evaluation.values(lookup, kPoint, f);
    evaluation.jacobian(lookup, kPoint, g);
    evaluation.hessian(lookup, {.x = kPoint, .w = w, .v = {}}, h);
    evaluation.product(lookup, {.x = kPoint, .w = w, .v = kPoint}, hv);
    return allocations() - start;
}

}

TEST(AllocationTest, Evaluation)
{
    EXPECT_EQ(allocated(evaluated()), 0U);
}

TEST(AllocationTest, Listed)
{
    EXPECT_EQ(allocated(listed()), 0U);
}
