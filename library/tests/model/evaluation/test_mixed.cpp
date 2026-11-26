#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "expr/forms/terms.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/sparsity/reach.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace
{

using biyos::Constraint;
using biyos::default_objective;
using biyos::Env;
using biyos::Evaluation;
using biyos::Index;
using biyos::Lookup;
using biyos::Lower;
using biyos::Model;
using biyos::Offset;
using biyos::Pattern;
using biyos::Real;
using biyos::Request;
using biyos::VariableKind;
using biyos::widest;
using Dense = std::vector<std::vector<Real>>;

constexpr Real kExact = 1e-14;
constexpr Real kStale = 7.0;
constexpr Real kStep = 1e-6;
constexpr Real kCentral = 1e-8;
constexpr std::size_t kVariables = 4;
const std::vector<Real> kPoint{0.3, -0.7, 1.1, 0.8};
const std::vector<Real> kWeights{0.7, -1.3, 2.1};
const std::vector<Real> kDirection{0.4, -0.9, 1.7, 0.6};
const std::vector<Real> kKinks{-1.0, 0.0, 2.0};
const std::vector<Real> kHeights{1.0, 0.0, 3.0};

int user_values(const Request &request, std::span<Real> out, void *)
{
    const std::span<const Real> x = request.x;
    out[0] = x[0] * x[3] * x[3];
    out[1] = x[1] * x[1] * x[1];
    return 0;
}

int user_gradients(const Request &request, std::span<Real> out, void *)
{
    const std::span<const Real> x = request.x;
    out[0] = x[0] * x[3];
    out[1] = 3.0 * x[1] * x[1];
    out[2] = x[3] * x[3];
    out[3] = x[0] * x[3];
    return 0;
}

int user_hessian(const Request &request, std::span<Real> out, void *)
{
    const std::span<const Real> x = request.x;
    const std::span<const Real> w = request.w;
    out[0] = w[0] * x[3];
    out[1] = 6.0 * w[1] * x[1];
    out[2] = 2.0 * w[0] * x[0];
    out[3] = w[0] * x[3];
    return 0;
}

int user_product(const Request &request, std::span<Real> out, void *)
{
    const std::span<const Real> x = request.x;
    const std::span<const Real> w = request.w;
    const std::span<const Real> v = request.v;
    out[0] = 2.0 * w[0] * x[3] * v[3];
    out[1] = 6.0 * w[1] * x[1] * v[1];
    out[3] = 2.0 * w[0] * x[3] * v[0] + 2.0 * w[0] * x[0] * v[3];
    return 0;
}

int second_values(const Request &request, std::span<Real> out, void *)
{
    out[0] = request.x[2] * request.x[3];
    return 0;
}

int second_gradients(const Request &request, std::span<Real> out, void *)
{
    out[0] = request.x[2];
    out[1] = request.x[3];
    return 0;
}

int second_hessian(const Request &request, std::span<Real> out, void *)
{
    out[0] = request.w[0];
    return 0;
}

Offset unary(Model &model, int code, Offset child)
{
    const std::array children{child};
    return model.add_node(code, children, {});
}

void attach_graphs(Model &model)
{
    const Offset rise = unary(model, BIYOS_OP_EXP, model.add_variable_node(1));
    const Offset wave = unary(model, BIYOS_OP_SIN, model.add_variable_node(2));
    const std::array factors{rise, wave};
    const Offset product = model.add_node(BIYOS_OP_PROD, factors, std::array{1.0});
    const Offset pwl = model.add_piecewise_linear(kKinks, kHeights);
    const Offset kinked = model.add_piecewise(pwl, model.add_variable_node(3));
    const std::array nodes{product, unary(model, BIYOS_OP_EXP, kinked)};
    model.attach_expressions(std::vector<Index>{0, 1}, nodes);
}

void add_terms(Model &model)
{
    model.add_linear_terms(std::vector<Index>{0, 2},
                           std::vector<Offset>{0, 2, 3},
                           {{.variable = 0, .coefficient = 1.5},
                            {.variable = 3, .coefficient = -1.0},
                            {.variable = 1, .coefficient = 2.0}});
    model.add_quadratic_terms(std::vector<Index>{0, 2},
                              std::vector<Offset>{0, 2, 3},
                              {{.row = 0, .column = 0, .coefficient = 0.7},
                               {.row = 1, .column = 2, .coefficient = -0.4},
                               {.row = 0, .column = 3, .coefficient = 0.3}});
}

void add_user(Model &model, bool product)
{
    const std::array covered{2, 1};
    const Index callback = model.add_callback(covered, nullptr);
    model.set_callback_function(callback, user_values);
    model.set_callback_jacobian(callback,
                                std::vector<Index>{2, 1, 2, 2},
                                std::vector<Index>{3, 1, 0, 3},
                                user_gradients);
    model.set_callback_hessian(callback,
                               std::vector<Index>{0, 1, 3, 0},
                               std::vector<Index>{3, 1, 3, 3},
                               user_hessian);
    if (product)
    {
        model.set_callback_hessian_product(callback, user_product);
    }
}

void add_second(Model &model)
{
    const std::array covered{0};
    const Index callback = model.add_callback(covered, nullptr);
    model.set_callback_function(callback, second_values);
    model.set_callback_jacobian(callback,
                                std::vector<Index>{0, 0},
                                std::vector<Index>{3, 2},
                                second_gradients);
    model.set_callback_hessian(callback,
                               std::vector<Index>{2},
                               std::vector<Index>{3},
                               second_hessian);
}

Model mixed(bool product)
{
    Model model;
    model.add_variables(std::vector(kVariables, widest(VariableKind::Continuous)));
    model.add_functions(std::vector{0.5, 0.0, -1.0}, std::vector<Offset>{0, 0, 0, 0}, {});
    attach_graphs(model);
    add_terms(model);
    add_user(model, product);
    add_second(model);
    model.add_constraints(std::vector{Constraint{.function = 0, .lower = 0.0, .upper = 1.0}});
    model.add_objectives(std::vector{default_objective(0, 0)});
    return model;
}

Real pwl_at(Real x)
{
    return x < 0.0 ? -x : 1.5 * x;
}

std::vector<Real> exact_values(std::span<const Real> x)
{
    const Real f0 = 0.5 + 1.5 * x[0] - x[3] + 0.7 * x[0] * x[0] - 0.4 * x[1] * x[2] +
                    std::exp(x[1]) * std::sin(x[2]) + x[2] * x[3];
    const Real f1 = std::exp(pwl_at(x[3])) + x[1] * x[1] * x[1];
    const Real f2 = -1.0 + 2.0 * x[1] + 0.3 * x[0] * x[3] + x[0] * x[3] * x[3];
    return {f0, f1, f2};
}

class Probe
{
  public:
    explicit Probe(bool product)
        : model_(mixed(product)), evaluation_(model_, env_.stream(), env_.pool())
    {
    }

    std::vector<Real> values(std::vector<Real> x)
    {
        std::vector<Real> f(3, kStale);
        evaluation_.values(lookup_, x, f);
        return f;
    }

    std::vector<Real> jacobian(std::vector<Real> x)
    {
        std::vector<Real> g(evaluation_.jacobian_pattern().variables.size(), kStale);
        evaluation_.jacobian(lookup_, x, g);
        return g;
    }

    std::vector<Real> gradient(std::vector<Real> x)
    {
        const std::vector<Real> g = jacobian(x);
        const Pattern &pattern = evaluation_.jacobian_pattern();
        std::vector<Real> sum(kVariables, 0.0);
        for (std::size_t k = 0; k < kWeights.size(); ++k)
        {
            for (auto p = static_cast<std::size_t>(pattern.starts[k]);
                 p < static_cast<std::size_t>(pattern.starts[k + 1]);
                 ++p)
            {
                sum[static_cast<std::size_t>(pattern.variables[p])] += kWeights[k] * g[p];
            }
        }
        return sum;
    }

    Dense hessian()
    {
        const Lower &lower = evaluation_.hessian_pattern();
        std::vector<Real> h(lower.rows.size(), kStale);
        evaluation_.hessian(lookup_, {.x = kPoint, .w = kWeights, .v = {}}, h);
        Dense dense(kVariables, std::vector<Real>(kVariables, 0.0));
        for (std::size_t column = 0; column + 1 < lower.starts.size(); ++column)
        {
            for (auto p = static_cast<std::size_t>(lower.starts[column]);
                 p < static_cast<std::size_t>(lower.starts[column + 1]);
                 ++p)
            {
                const auto row = static_cast<std::size_t>(lower.rows[p]);
                dense[row][column] = h[p];
                dense[column][row] = h[p];
            }
        }
        return dense;
    }

    std::vector<Real> product()
    {
        std::vector<Real> hv(kVariables, kStale);
        evaluation_.product(lookup_, {.x = kPoint, .w = kWeights, .v = kDirection}, hv);
        return hv;
    }

    const Pattern &pattern() const { return evaluation_.jacobian_pattern(); }

  private:
    Env env_;
    Model model_;
    Evaluation evaluation_;
    Lookup lookup_{env_.options()};
};

std::vector<Real> moved(std::vector<Real> x, std::size_t variable, Real step)
{
    x[variable] += step;
    return x;
}

std::vector<Real> central_jacobian(Probe &probe)
{
    const Pattern &pattern = probe.pattern();
    std::vector<Real> g(pattern.variables.size());
    for (std::size_t row = 0; row + 1 < pattern.starts.size(); ++row)
    {
        for (auto p = static_cast<std::size_t>(pattern.starts[row]);
             p < static_cast<std::size_t>(pattern.starts[row + 1]);
             ++p)
        {
            const auto column = static_cast<std::size_t>(pattern.variables[p]);
            const Real rise = probe.values(moved(kPoint, column, kStep))[row];
            const Real fall = probe.values(moved(kPoint, column, -kStep))[row];
            g[p] = (rise - fall) / (2.0 * kStep);
        }
    }
    return g;
}

Dense central_hessian(Probe &probe)
{
    Dense dense(kVariables);
    for (std::size_t column = 0; column < kVariables; ++column)
    {
        const std::vector<Real> rise = probe.gradient(moved(kPoint, column, kStep));
        const std::vector<Real> fall = probe.gradient(moved(kPoint, column, -kStep));
        for (std::size_t row = 0; row < kVariables; ++row)
        {
            dense[column].push_back((rise[row] - fall[row]) / (2.0 * kStep));
        }
    }
    return dense;
}

std::vector<Real> times(const Dense &dense, std::span<const Real> v)
{
    std::vector<Real> out(v.size(), 0.0);
    for (std::size_t row = 0; row < dense.size(); ++row)
    {
        for (std::size_t column = 0; column < v.size(); ++column)
        {
            out[row] += dense[row][column] * v[column];
        }
    }
    return out;
}

void expect_near(std::span<const Real> found, std::span<const Real> wanted, Real bound)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        EXPECT_NEAR(found[k], wanted[k], bound) << "entry " << k;
    }
}

}

TEST(EvaluationTest, MixedValues)
{
    Probe probe(false);
    expect_near(probe.values(kPoint), exact_values(kPoint), kExact);
}

TEST(EvaluationTest, MixedJacobian)
{
    Probe probe(false);
    expect_near(probe.jacobian(kPoint), central_jacobian(probe), kCentral);
}

TEST(EvaluationTest, MixedHessian)
{
    Probe probe(false);
    const Dense found = probe.hessian();
    const Dense wanted = central_hessian(probe);
    for (std::size_t row = 0; row < kVariables; ++row)
    {
        expect_near(found[row], wanted[row], kCentral);
    }
}

TEST(EvaluationTest, MixedProduct)
{
    for (const bool product : {false, true})
    {
        Probe probe(product);
        expect_near(probe.product(), times(central_hessian(probe), kDirection), kCentral);
        expect_near(probe.product(), times(probe.hessian(), kDirection), kExact);
    }
}
