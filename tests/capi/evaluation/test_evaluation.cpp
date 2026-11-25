#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "core/options/lookup.hpp"
#include "handles.hpp"
#include "mixed.hpp"
#include "model/evaluation/evaluation.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

using biyos::test::build_mixed;
using biyos::test::Env;
using biyos::test::kDirection;
using biyos::test::kPoint;
using biyos::test::kWeights;
using biyos::test::last_message;
using biyos::test::Model;
using Values = std::vector<double>;
using Bits = std::vector<std::uint64_t>;

Bits bits(const Values &values)
{
    Bits found(values.size());
    std::ranges::transform(values, found.begin(), std::bit_cast<std::uint64_t, double>);
    return found;
}

struct Reference
{
    explicit Reference(const Model &model)
        : handle(*model.get()), evaluation(handle.handle.env(), handle.core, &handle),
          lookup(handle.settings, handle.handle.env().options())
    {
    }

    BIYOS_model &handle;
    biyos::Evaluation evaluation;
    biyos::Lookup lookup;
};

Values values_of(const Model &model)
{
    Reference reference(model);
    Values f(3);
    reference.evaluation.values(reference.lookup, kPoint, f);
    return f;
}

Values jacobian_of(const Model &model)
{
    Reference reference(model);
    Values g(reference.evaluation.jacobian_pattern().variables.size());
    reference.evaluation.jacobian(reference.lookup, kPoint, g);
    return g;
}

Values hessian_of(const Model &model)
{
    Reference reference(model);
    Values h(reference.evaluation.hessian_pattern().rows.size());
    reference.evaluation.hessian(reference.lookup, kPoint, kWeights, h);
    return h;
}

Values product_of(const Model &model)
{
    Reference reference(model);
    Values hv(4);
    reference.evaluation.product(reference.lookup, kPoint, kWeights, kDirection, hv);
    return hv;
}

Values product(const Model &model)
{
    Values hv(4);
    EXPECT_EQ(
        BIYOS_evalhv(model.get(), kPoint.data(), kWeights.data(), kDirection.data(), hv.data()),
        BIYOS_OK);
    return hv;
}

struct Mixed
{
    Mixed() { build_mixed(model.get(), true); }

    Env env;
    Model model{env};
};

}

TEST(EvaluationTest, Functions)
{
    const Mixed mixed;
    Values f(3);
    ASSERT_EQ(BIYOS_evalf(mixed.model.get(), kPoint.data(), f.data()), BIYOS_OK);
    EXPECT_EQ(bits(f), bits(values_of(mixed.model)));
}

TEST(EvaluationTest, Gradients)
{
    const Mixed mixed;
    Values g(9);
    ASSERT_EQ(BIYOS_evalg(mixed.model.get(), kPoint.data(), g.data()), BIYOS_OK);
    EXPECT_EQ(bits(g), bits(jacobian_of(mixed.model)));
}

TEST(EvaluationTest, Lagrangian)
{
    const Mixed mixed;
    Values h(7);
    ASSERT_EQ(BIYOS_evalh(mixed.model.get(), kPoint.data(), kWeights.data(), h.data()), BIYOS_OK);
    EXPECT_EQ(bits(h), bits(hessian_of(mixed.model)));
}

TEST(EvaluationTest, Product)
{
    const Mixed mixed;
    EXPECT_EQ(bits(product(mixed.model)), bits(product_of(mixed.model)));
    const Model pairs(mixed.env);
    build_mixed(pairs.get(), false);
    EXPECT_EQ(bits(product(pairs)), bits(product_of(pairs)));
}

TEST(EvaluationTest, Nulls)
{
    const Mixed mixed;
    Values f(3, -1.0);
    EXPECT_EQ(BIYOS_evalf(mixed.model.get(), nullptr, f.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(mixed.env.message(), "invalid argument: argument 'x': is null");
    EXPECT_EQ(BIYOS_evalf(mixed.model.get(), kPoint.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(mixed.env.message(), "invalid argument: argument 'f': is null");
    EXPECT_EQ(BIYOS_evalg(mixed.model.get(), kPoint.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(mixed.env.message(), "invalid argument: argument 'g': is null");
    EXPECT_EQ(BIYOS_evalh(mixed.model.get(), kPoint.data(), nullptr, f.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(mixed.env.message(), "invalid argument: argument 'w': is null");
    EXPECT_EQ(BIYOS_evalhv(mixed.model.get(), kPoint.data(), kWeights.data(), nullptr, f.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(mixed.env.message(), "invalid argument: argument 'v': is null");
    EXPECT_EQ(BIYOS_evalf(nullptr, kPoint.data(), f.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(bits(f), bits(Values(3, -1.0)));
}

TEST(EvaluationTest, Bare)
{
    const Env env;
    const Model empty(env);
    EXPECT_EQ(BIYOS_evalf(empty.get(), nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_evalg(empty.get(), nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_evalh(empty.get(), nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_evalhv(empty.get(), nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(EvaluationTest, Rebuilt)
{
    const Mixed mixed;
    Values f(3);
    ASSERT_EQ(BIYOS_evalf(mixed.model.get(), kPoint.data(), f.data()), BIYOS_OK);
    const BIYOS_index function = 1;
    const BIYOS_index variable = 0;
    const double coefficient = 1.0;
    ASSERT_EQ(BIYOS_addlintermsp(mixed.model.get(), 1, &function, &variable, &coefficient),
              BIYOS_OK);
    EXPECT_EQ(mixed.model.get()->evaluation, nullptr);
    Values g(10);
    ASSERT_EQ(BIYOS_evalg(mixed.model.get(), kPoint.data(), g.data()), BIYOS_OK);
    EXPECT_EQ(bits(g), bits(jacobian_of(mixed.model)));
    ASSERT_EQ(BIYOS_evalf(mixed.model.get(), kPoint.data(), f.data()), BIYOS_OK);
    EXPECT_EQ(bits(f), bits(values_of(mixed.model)));
}
