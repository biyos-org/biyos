#include "allocations.hpp"
#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/index.hpp"
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

using biyos::Evaluation;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::Request;

const std::vector<double> kPoint{0.5, -1.5};

int squared(const Request &request, std::span<double> out, void *)
{
    out[0] = request.x[0] * request.x[0];
    return 0;
}

int doubled(const Request &request, std::span<double> out, void *)
{
    out[0] = 2.0 * request.x[0];
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
}

Model evaluated()
{
    Model model;
    model.add_variables(std::vector(2, biyos::widest(biyos::VariableKind::Continuous)));
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

}

TEST(AllocationTest, Evaluation)
{
    biyos::Env env;
    const Model model = evaluated();
    Evaluation evaluation(env, model);
    const biyos::Lookup lookup(env.options());
    std::vector<double> f(2);
    std::vector<double> g(evaluation.jacobian_pattern().variables.size());
    const std::size_t start = biyos::test::allocations();
    evaluation.values(lookup, kPoint, f);
    evaluation.jacobian(lookup, kPoint, g);
    EXPECT_EQ(biyos::test::allocations() - start, 0U);
}
