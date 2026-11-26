#include "core/env.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "expr/forms/terms.hpp"
#include "expr/sparsity/reach.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace
{

using biyos::Env;
using biyos::Evaluation;
using biyos::Index;
using biyos::Lookup;
using biyos::Model;
using biyos::Offset;
using biyos::Pattern;
using biyos::Real;
using biyos::Request;
using biyos::to_index;
using biyos::to_size;
using biyos::Variable;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kExact = 1e-15;
constexpr Real kStep = 1e-6;
constexpr Real kCentral = 1e-8;

const std::vector<Real> kPoint{0.3, -0.7, 1.1, 0.9, -0.4};
const std::vector<Real> kWeights{0.6, -1.4};
const std::vector<Real> kDirection{0.2, 1.3, -0.5, -0.8, 0.7};
const std::array<Index, 2> kCovered{0, 1};
const std::array<Index, 2> kListed{3, 1};
const std::array<Index, 4> kRows{0, 0, 1, 1};
const std::array<Index, 4> kColumns{3, 1, 1, 3};
const std::array<Index, 3> kFirsts{1, 1, 3};
const std::array<Index, 3> kSeconds{1, 3, 3};

struct Reads
{
    std::size_t first;
    std::size_t second;
};

struct Seen
{
    Reads reads;
    std::size_t x = 0;
    std::size_t v = 0;
};

struct Monomial
{
    std::vector<Index> list;
    bool dense;
    std::vector<Real> x;
};

Monomial listed_on(std::vector<Index> list)
{
    return {.list = std::move(list), .dense = false, .x = {}};
}

Monomial dense_on(std::vector<Index> list)
{
    return {.list = std::move(list), .dense = true, .x = {}};
}

enum class Curvature
{
    Hessian,
    Product,
};

Reads reads_of(void *data)
{
    return static_cast<Seen *>(data)->reads;
}

void note(const Request &request, void *data)
{
    Seen &seen = *static_cast<Seen *>(data);
    seen.x = request.x.size();
    seen.v = request.v.size();
}

int values(const Request &request, std::span<Real> out, void *data)
{
    const auto [a, b] = reads_of(data);
    note(request, data);
    out[0] = request.x[a] * request.x[a] * request.x[b];
    out[1] = std::sin(request.x[b]) + 2.0 * request.x[a];
    return 0;
}

int gradients(const Request &request, std::span<Real> out, void *data)
{
    const auto [a, b] = reads_of(data);
    out[0] = 2.0 * request.x[a] * request.x[b];
    out[1] = request.x[a] * request.x[a];
    out[2] = std::cos(request.x[b]);
    out[3] = 2.0;
    return 0;
}

int curvatures(const Request &request, std::span<Real> out, void *data)
{
    const auto [a, b] = reads_of(data);
    out[0] = -request.w[1] * std::sin(request.x[b]);
    out[1] = 2.0 * request.w[0] * request.x[a];
    out[2] = 2.0 * request.w[0] * request.x[b];
    return 0;
}

int products(const Request &request, std::span<Real> out, void *data)
{
    const auto [a, b] = reads_of(data);
    note(request, data);
    const Real bb = -request.w[1] * std::sin(request.x[b]);
    const Real ab = 2.0 * request.w[0] * request.x[a];
    const Real aa = 2.0 * request.w[0] * request.x[b];
    out[a] = aa * request.v[a] + ab * request.v[b];
    out[b] = ab * request.v[a] + bb * request.v[b];
    return 0;
}

Model free_model(std::size_t functions)
{
    Model model;
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    model.add_variables(std::vector<Variable>(kPoint.size(), free));
    model.add_functions(std::vector<Real>(functions, 0.0),
                        std::vector<Offset>(functions + 1, 0),
                        {});
    return model;
}

void set_parts(Model &model, Index callback, Curvature curvature)
{
    model.set_callback_function(callback, values);
    model.set_callback_jacobian(callback, kRows, kColumns, gradients);
    model.set_callback_hessian(callback, kFirsts, kSeconds, curvatures);
    if (curvature == Curvature::Product)
    {
        model.set_callback_hessian_product(callback, products);
    }
}

std::vector<Real> picked(const Monomial &monomial, std::span<const Real> values)
{
    if (!monomial.dense)
    {
        return {values.begin(), values.end()};
    }
    std::vector<Real> picked;
    for (const Index j : monomial.list)
    {
        picked.push_back(values[to_size(j)]);
    }
    return picked;
}

Real skipping(std::span<const Real> x, std::size_t a, std::size_t b)
{
    Real product = 1.0;
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        product *= i == a || i == b ? 1.0 : x[i];
    }
    return product;
}

Real row_of(std::span<const Real> x, std::span<const Real> v, std::size_t i)
{
    Real sum = 0.0;
    for (std::size_t j = 0; j < x.size(); ++j)
    {
        sum += j == i ? 0.0 : skipping(x, i, j) * v[j];
    }
    return sum;
}

int monomial_values(const Request &request, std::span<Real> out, void *data)
{
    Monomial &monomial = *static_cast<Monomial *>(data);
    monomial.x = picked(monomial, request.x);
    out[0] = skipping(monomial.x, monomial.x.size(), monomial.x.size());
    return 0;
}

int monomial_products(const Request &request, std::span<Real> out, void *data)
{
    const Monomial &monomial = *static_cast<const Monomial *>(data);
    const std::vector<Real> x = picked(monomial, request.x);
    const std::vector<Real> v = picked(monomial, request.v);
    std::ranges::fill(out, 0.0);
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        out[monomial.dense ? to_size(monomial.list[i]) : i] = request.w[0] * row_of(x, v, i);
    }
    return 0;
}

Model monomial_model(std::span<Monomial> monomials)
{
    Model model = free_model(monomials.size());
    for (std::size_t k = 0; k < monomials.size(); ++k)
    {
        Monomial &monomial = monomials[k];
        const std::array covered{to_index(k)};
        const Index callback = monomial.dense
                                   ? model.add_callback(covered, &monomial)
                                   : model.add_callback(covered, monomial.list, &monomial);
        model.set_callback_function(callback, monomial_values);
        model.set_callback_hessian_product(callback, monomial_products);
    }
    return model;
}

std::vector<Real> values_at(Evaluation &evaluation,
                            const Lookup &lookup,
                            const std::vector<Real> &x)
{
    std::vector<Real> f(2);
    evaluation.values(lookup, x, f);
    return f;
}

std::vector<Real> jacobian_at(Evaluation &evaluation,
                              const Lookup &lookup,
                              const std::vector<Real> &x)
{
    std::vector<Real> g(evaluation.jacobian_pattern().variables.size());
    evaluation.jacobian(lookup, x, g);
    return g;
}

std::vector<Real> gradient_at(Evaluation &evaluation,
                              const Lookup &lookup,
                              const std::vector<Real> &x)
{
    const Pattern &pattern = evaluation.jacobian_pattern();
    const std::vector<Real> g = jacobian_at(evaluation, lookup, x);
    std::vector<Real> gradient(x.size());
    for (std::size_t row = 0; row + 1 < pattern.starts.size(); ++row)
    {
        for (Offset k = pattern.starts[row]; k < pattern.starts[row + 1]; ++k)
        {
            const std::size_t entry = to_size(k);
            gradient[to_size(pattern.variables[entry])] += kWeights[row] * g[entry];
        }
    }
    return gradient;
}

std::vector<Real> shifted(Real step, const std::vector<Real> &along)
{
    std::vector<Real> x = kPoint;
    for (std::size_t j = 0; j < x.size(); ++j)
    {
        x[j] += step * along[j];
    }
    return x;
}

std::vector<Real> central_jacobian(Evaluation &evaluation, const Lookup &lookup)
{
    const Pattern &pattern = evaluation.jacobian_pattern();
    std::vector<Real> g(pattern.variables.size());
    for (std::size_t row = 0; row + 1 < pattern.starts.size(); ++row)
    {
        for (Offset k = pattern.starts[row]; k < pattern.starts[row + 1]; ++k)
        {
            std::vector<Real> along(kPoint.size());
            along[to_size(pattern.variables[to_size(k)])] = 1.0;
            const Real rise = values_at(evaluation, lookup, shifted(kStep, along))[row];
            const Real fall = values_at(evaluation, lookup, shifted(-kStep, along))[row];
            g[to_size(k)] = (rise - fall) / (2.0 * kStep);
        }
    }
    return g;
}

std::vector<Real> central_product(Evaluation &evaluation, const Lookup &lookup)
{
    const std::vector<Real> rise = gradient_at(evaluation, lookup, shifted(kStep, kDirection));
    const std::vector<Real> fall = gradient_at(evaluation, lookup, shifted(-kStep, kDirection));
    std::vector<Real> hv(kPoint.size());
    for (std::size_t j = 0; j < hv.size(); ++j)
    {
        hv[j] = (rise[j] - fall[j]) / (2.0 * kStep);
    }
    return hv;
}

std::vector<Real> product_at(Evaluation &evaluation,
                             const Lookup &lookup,
                             std::span<const Real> w = kWeights)
{
    std::vector<Real> hv(kPoint.size());
    evaluation.product(lookup, {.x = kPoint, .w = w, .v = kDirection}, hv);
    return hv;
}

std::vector<Real> hessian_at(Evaluation &evaluation, const Lookup &lookup)
{
    std::vector<Real> h(evaluation.hessian_pattern().rows.size());
    evaluation.hessian(lookup, {.x = kPoint, .w = kWeights, .v = {}}, h);
    return h;
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

class NumberingTest : public testing::Test
{
  protected:
    Model listed_model(Curvature curvature)
    {
        Model model = free_model(kWeights.size());
        seen_.reads = {0, 1};
        set_parts(model, model.add_callback(kCovered, kListed, &seen_), curvature);
        return model;
    }

    Model dense_model(Curvature curvature)
    {
        Model model = free_model(kWeights.size());
        dense_seen_.reads = {3, 1};
        set_parts(model, model.add_callback(kCovered, &dense_seen_), curvature);
        return model;
    }

    void expect_matching(std::span<Monomial> listed, std::span<Monomial> dense)
    {
        const Model listed_model = monomial_model(listed);
        const Model dense_model = monomial_model(dense);
        Evaluation evaluation(listed_model, env_.stream());
        Evaluation full(dense_model, env_.stream());
        std::vector<Real> f(listed.size());
        std::vector<Real> wanted(dense.size());
        evaluation.values(lookup_, kPoint, f);
        full.values(lookup_, kPoint, wanted);
        expect_near(f, wanted, kExact);
        const std::span<const Real> w = std::span(kWeights).first(listed.size());
        expect_near(product_at(evaluation, lookup_, w), product_at(full, lookup_, w), kExact);
    }

    Env env_;
    const Lookup lookup_{env_.options()};
    Seen seen_;
    Seen dense_seen_;
};

TEST_F(NumberingTest, Values)
{
    const Model model = listed_model(Curvature::Hessian);
    const Model dense = dense_model(Curvature::Hessian);
    Evaluation listed(model, env_.stream());
    Evaluation full(dense, env_.stream());
    expect_near(values_at(listed, lookup_, kPoint), values_at(full, lookup_, kPoint), kExact);
    EXPECT_EQ(seen_.x, kListed.size());
    EXPECT_EQ(seen_.v, 0U);
    EXPECT_EQ(dense_seen_.x, kPoint.size());
}

TEST_F(NumberingTest, Jacobian)
{
    const Model model = listed_model(Curvature::Hessian);
    const Model dense = dense_model(Curvature::Hessian);
    Evaluation listed(model, env_.stream());
    Evaluation full(dense, env_.stream());
    const std::vector<Real> g = jacobian_at(listed, lookup_, kPoint);
    expect_near(g, jacobian_at(full, lookup_, kPoint), kExact);
    expect_near(g, central_jacobian(listed, lookup_), kCentral);
}

TEST_F(NumberingTest, Hessian)
{
    const Model model = listed_model(Curvature::Hessian);
    const Model dense = dense_model(Curvature::Hessian);
    Evaluation listed(model, env_.stream());
    Evaluation full(dense, env_.stream());
    expect_near(hessian_at(listed, lookup_), hessian_at(full, lookup_), kExact);
    const std::vector<Real> hv = product_at(listed, lookup_);
    expect_near(hv, product_at(full, lookup_), kExact);
    expect_near(hv, central_product(listed, lookup_), kCentral);
}

TEST_F(NumberingTest, Product)
{
    const Model model = listed_model(Curvature::Product);
    const Model dense = dense_model(Curvature::Product);
    Evaluation listed(model, env_.stream());
    Evaluation full(dense, env_.stream());
    const std::vector<Real> hv = product_at(listed, lookup_);
    expect_near(hv, product_at(full, lookup_), kExact);
    EXPECT_EQ(seen_.x, kListed.size());
    EXPECT_EQ(seen_.v, kListed.size());
    EXPECT_EQ(dense_seen_.v, kPoint.size());
    expect_near(hv, central_product(listed, lookup_), kCentral);
}

TEST_F(NumberingTest, Wide)
{
    std::array listed{listed_on({4, 0, 2})};
    std::array dense{dense_on({4, 0, 2})};
    expect_matching(listed, dense);
    const std::array wanted{kPoint[4], kPoint[0], kPoint[2]};
    expect_near(listed[0].x, wanted, kExact);
}

TEST_F(NumberingTest, Shared)
{
    std::array listed{listed_on({3, 1}), listed_on({1, 4, 0})};
    std::array dense{dense_on({3, 1}), dense_on({1, 4, 0})};
    expect_matching(listed, dense);
    EXPECT_EQ(listed[0].x.size(), 2U);
    EXPECT_EQ(listed[1].x.size(), 3U);
}

TEST_F(NumberingTest, Empty)
{
    std::array listed{listed_on({})};
    std::array dense{dense_on({})};
    expect_matching(listed, dense);
    EXPECT_TRUE(listed[0].x.empty());
}
